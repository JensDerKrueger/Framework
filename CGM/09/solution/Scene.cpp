#include "Scene.h"

#include "Material.h"
#include "Rectangle.h"
#include "Sphere.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace {

constexpr float vertexMergeEpsilon = 0.00001f;
constexpr float smoothNormalThreshold = 0.5f;

struct VertexKey {
	int64_t x;
	int64_t y;
	int64_t z;

	bool operator==(const VertexKey& other) const {
		return x == other.x && y == other.y && z == other.z;
	}
};

struct VertexKeyHash {
	size_t operator()(const VertexKey& key) const {
		const size_t h0 = std::hash<int64_t>{}(key.x);
		const size_t h1 = std::hash<int64_t>{}(key.y);
		const size_t h2 = std::hash<int64_t>{}(key.z);
		return h0 ^ (h1 << 1) ^ (h2 << 2);
	}
};

using VertexAdjacency = std::unordered_map<VertexKey, std::vector<size_t>, VertexKeyHash>;

VertexKey vertexKey(const Vec3& vertex) {
	return {
		int64_t(std::llround(vertex.x / vertexMergeEpsilon)),
		int64_t(std::llround(vertex.y / vertexMergeEpsilon)),
		int64_t(std::llround(vertex.z / vertexMergeEpsilon))
	};
}

Vec3 triangleNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
	// The cross product of two triangle edges is perpendicular to the patch.
	// We normalize it because the radiosity form factor only needs the
	// orientation; the patch size is stored separately as area.
	return Vec3::normalize(Vec3::cross(b - a, c - a));
}

float triangleArea(const Vec3& a, const Vec3& b, const Vec3& c) {
	// The cross product length is the area of the parallelogram spanned by
	// the two edge vectors. A triangle covers exactly half of it.
	return Vec3::cross(b - a, c - a).length() * 0.5f;
}

Vec3 toneMap(const Vec3& color) {
	const Vec3 mapped = color / (color + Vec3{1.0f, 1.0f, 1.0f});
	return Vec3::clamp(mapped, 0.0f, 1.0f);
}

void appendVertex(std::vector<float>& data, const Vec3& position, const Vec3& color) {
	data.push_back(position.x);
	data.push_back(position.y);
	data.push_back(position.z);
	data.push_back(color.r);
	data.push_back(color.g);
	data.push_back(color.b);
	data.push_back(1.0f);
}

void appendLitVertex(std::vector<float>& data, const Vec3& position, const Vec3& color, const Vec3& normal) {
	appendVertex(data, position, color);
	data.push_back(normal.x);
	data.push_back(normal.y);
	data.push_back(normal.z);
}

Vec3 progressColor(const RadiosityPatch& patch, bool isComplete) {
	if (patch.emission.sqlength() > 0.0f)
		return isComplete ? Vec3{1.0f, 0.95f, 0.65f} : Vec3{0.45f, 0.42f, 0.24f};

	if (isComplete)
		return Vec3{0.12f, 0.72f, 0.30f};

	return patch.reflectance * 0.28f + Vec3{0.06f, 0.06f, 0.06f};
}

void addPatchToAdjacency(VertexAdjacency& adjacency, const Vec3& vertex, size_t patchIndex) {
	adjacency[vertexKey(vertex)].push_back(patchIndex);
}

VertexAdjacency buildVertexAdjacency(const std::vector<RadiosityPatch>& patches) {
	VertexAdjacency adjacency;

	for (size_t patchIndex = 0; patchIndex < patches.size(); ++patchIndex) {
		const RadiosityPatch& patch = patches[patchIndex];
		addPatchToAdjacency(adjacency, patch.a, patchIndex);
		addPatchToAdjacency(adjacency, patch.b, patchIndex);
		addPatchToAdjacency(adjacency, patch.c, patchIndex);
	}

	return adjacency;
}

Vec3 smoothedRadiosityAtVertex(const std::vector<RadiosityPatch>& patches,
                               const VertexAdjacency& adjacency,
                               const Vec3& vertex,
                               const Vec3& normal) {
	const auto adjacentPatches = adjacency.find(vertexKey(vertex));
	if (adjacentPatches == adjacency.end())
		return Vec3{};

	Vec3 sum;
	float weightSum = 0.0f;
	for (const size_t patchIndex : adjacentPatches->second) {
		const RadiosityPatch& patch = patches[patchIndex];
		if (Vec3::dot(normal, patch.normal) < smoothNormalThreshold)
			continue;

		sum = sum + patch.radiosity * patch.area;
		weightSum += patch.area;
	}

	return weightSum > 0.0f ? sum / weightSum : Vec3{};
}

Vec3 bilerp(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, float u, float v) {
	const Vec3 bottom = a * (1.0f - u) + b * u;
	const Vec3 top = d * (1.0f - u) + c * u;
	return bottom * (1.0f - v) + top * v;
}

void addSubdividedRectangle(Scene& scene,
                            const Vec3& a,
                            const Vec3& b,
                            const Vec3& c,
                            const Vec3& d,
                            const Material& material,
                            size_t uSteps,
                            size_t vSteps) {
	for (size_t v = 0; v < vSteps; ++v) {
		const float v0 = float(v) / float(vSteps);
		const float v1 = float(v + 1) / float(vSteps);
		for (size_t u = 0; u < uSteps; ++u) {
			const float u0 = float(u) / float(uSteps);
			const float u1 = float(u + 1) / float(uSteps);
			scene.addObject(std::make_shared<Rectangle>(bilerp(a, b, c, d, u0, v0),
			                                            bilerp(a, b, c, d, u1, v0),
			                                            bilerp(a, b, c, d, u1, v1),
			                                            bilerp(a, b, c, d, u0, v1),
			                                            material));
		}
	}
}

} // namespace

void Scene::addObject(std::shared_ptr<const IntersectableObject> object) {
	sceneObjects.push_back(object);
}

void Scene::setModel(const Mat4& model) {
	this->model = model;
}

Mat4 Scene::getModel() const {
	return model;
}

Vec3 Scene::getBackgroundcolor() const {
	return backgroundColor;
}

std::optional<Intersection> Scene::intersect(const Ray& ray) const {
	std::optional<Intersection> result{};
	for (std::shared_ptr<const IntersectableObject> object : sceneObjects) {
		const std::optional<Intersection> intersection = object->intersect(ray);
		if (!intersection)
			continue;

		if (!result || intersection->getT() < result->getT())
			result = intersection;
	}
	return result;
}

std::vector<RadiosityPatch> Scene::createPatches() const {
	std::vector<RadiosityPatch> patches;

	// Radiosity works on surface elements, usually called patches. In this
	// simple implementation every triangle of every tessellated object becomes
	// exactly one patch.
	for (std::shared_ptr<const IntersectableObject> object : sceneObjects) {
		const Tessellation mesh = object->getMesh().unpack();
		const std::vector<float>& vertices = mesh.getVertices();
		const Material material = object->getMaterial();
		const size_t triangleCount = vertices.size() / 9;

		patches.reserve(patches.size() + triangleCount);
		for (size_t triangle = 0; triangle < triangleCount; ++triangle) {
			// The unpacked tessellation stores three vertices per triangle and
			// three coordinates per vertex:
			// a = vertices[offset + 0..2]
			// b = vertices[offset + 3..5]
			// c = vertices[offset + 6..8]
			const size_t offset = triangle * 9;
			RadiosityPatch patch;
			patch.a = Vec3{vertices[offset + 0], vertices[offset + 1], vertices[offset + 2]};
			patch.b = Vec3{vertices[offset + 3], vertices[offset + 4], vertices[offset + 5]};
			patch.c = Vec3{vertices[offset + 6], vertices[offset + 7], vertices[offset + 8]};

			// We use the patch center as a representative point for this
			// triangle when estimating distances, directions, and visibility.
			patch.center = (patch.a + patch.b + patch.c) / 3.0f;

			// The normal and area are the geometric data needed by the form
			// factor approximation:
			// F_ij ~= A_j cos(theta_i) cos(theta_j) / (pi r^2)
			patch.normal = triangleNormal(patch.a, patch.b, patch.c);
			patch.area = triangleArea(patch.a, patch.b, patch.c);

			// Reflectance is the diffuse fraction of incoming light that this
			// patch reflects. Emission is non-zero only for light source patches.
			patch.reflectance = material.getReflectance();
			patch.emission = material.getEmission();

			// Before solving light transport, the only known outgoing light is
			// the light emitted directly by the patch itself.
			patch.radiosity = patch.emission;

			// Degenerate triangles have zero area and would not contribute
			// meaningful light transport, so we skip them.
			if (patch.area > 0.0f)
				patches.push_back(patch);
		}
	}

	return patches;
}

std::vector<float> Scene::getTriangleData(const std::vector<RadiosityPatch>& patches) const {
	std::vector<float> data;
	data.reserve(patches.size() * 21);

	for (const RadiosityPatch& patch : patches) {
		const Vec3 color = toneMap(patch.radiosity);
		appendVertex(data, patch.a, color);
		appendVertex(data, patch.b, color);
		appendVertex(data, patch.c, color);
	}

	return data;
}

std::vector<float> Scene::getSmoothedTriangleData(const std::vector<RadiosityPatch>& patches) const {
	const VertexAdjacency adjacency = buildVertexAdjacency(patches);

	std::vector<float> data;
	data.reserve(patches.size() * 21);

	for (const RadiosityPatch& patch : patches) {
		appendVertex(data, patch.a, toneMap(smoothedRadiosityAtVertex(patches, adjacency, patch.a, patch.normal)));
		appendVertex(data, patch.b, toneMap(smoothedRadiosityAtVertex(patches, adjacency, patch.b, patch.normal)));
		appendVertex(data, patch.c, toneMap(smoothedRadiosityAtVertex(patches, adjacency, patch.c, patch.normal)));
	}

	return data;
}

std::vector<float> Scene::getFormFactorProgressTriangleData(const std::vector<RadiosityPatch>& patches,
                                                            size_t completedRows) const {
	std::vector<float> data;
	data.reserve(patches.size() * 30);

	for (size_t i = 0; i < patches.size(); ++i) {
		const RadiosityPatch& patch = patches[i];
		const Vec3 color = progressColor(patch, i < completedRows);
		appendLitVertex(data, patch.a, color, patch.normal);
		appendLitVertex(data, patch.b, color, patch.normal);
		appendLitVertex(data, patch.c, color, patch.normal);
	}

	return data;
}

Scene Scene::genCornellBox(size_t wallTessellationFactor, size_t sphereTessellationFactor) {
	Scene scene{Vec3{0.02f, 0.02f, 0.02f}};

	const Material whiteDiffuse{Vec3{0.82f, 0.82f, 0.78f}};
	const Material redDiffuse{Vec3{0.75f, 0.08f, 0.04f}};
	const Material blueDiffuse{Vec3{0.10f, 0.12f, 0.62f}};
	const Material yellowDiffuse{Vec3{0.80f, 0.68f, 0.12f}};
	const Material greenDiffuse{Vec3{0.18f, 0.58f, 0.26f}};
	const Material light{Vec3{}, Vec3{16.0f, 14.0f, 10.0f}};

	const float x0 = -1.6f;
	const float x1 = 1.6f;
	const float y0 = -1.25f;
	const float y1 = 1.45f;
	const float z0 = -4.2f;
	const float z1 = -0.7f;

	const size_t wallSteps = std::max<size_t>(1, wallTessellationFactor);
	addSubdividedRectangle(scene, Vec3{x0, y0, z1}, Vec3{x1, y0, z1}, Vec3{x1, y0, z0}, Vec3{x0, y0, z0}, whiteDiffuse, wallSteps, wallSteps);
	addSubdividedRectangle(scene, Vec3{x0, y1, z0}, Vec3{x1, y1, z0}, Vec3{x1, y1, z1}, Vec3{x0, y1, z1}, whiteDiffuse, wallSteps, wallSteps);
	addSubdividedRectangle(scene, Vec3{x0, y0, z0}, Vec3{x1, y0, z0}, Vec3{x1, y1, z0}, Vec3{x0, y1, z0}, whiteDiffuse, wallSteps, wallSteps);
	addSubdividedRectangle(scene, Vec3{x0, y0, z1}, Vec3{x0, y0, z0}, Vec3{x0, y1, z0}, Vec3{x0, y1, z1}, redDiffuse, wallSteps, wallSteps);
	addSubdividedRectangle(scene, Vec3{x1, y0, z0}, Vec3{x1, y0, z1}, Vec3{x1, y1, z1}, Vec3{x1, y1, z0}, blueDiffuse, wallSteps, wallSteps);

	addSubdividedRectangle(scene, Vec3{-0.45f, y1 - 0.01f, -2.75f},
	                       Vec3{0.45f, y1 - 0.01f, -2.75f},
	                       Vec3{0.45f, y1 - 0.01f, -1.85f},
	                       Vec3{-0.45f, y1 - 0.01f, -1.85f},
	                       light,
	                       2,
	                       2);

	scene.addObject(std::make_shared<Sphere>(Vec3{-0.52f, y0 + 0.42f, -2.15f},
	                                         0.42f,
	                                         yellowDiffuse,
	                                         sphereTessellationFactor));
	scene.addObject(std::make_shared<Sphere>(Vec3{0.55f, y0 + 0.34f, -2.85f},
	                                         0.34f,
	                                         greenDiffuse,
	                                         sphereTessellationFactor));

	return scene;
}
