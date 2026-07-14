#include "Scene.h"
#include "Material.h"
#include "Sphere.h"
#include "Plane.h"
#include "Texture.h"

#include <algorithm>
#include <cmath>

namespace {

Vec3 transformDirection(const Mat4& matrix, const Vec3& direction) {
	return (matrix * Vec4{direction, 0.0f}).xyz;
}

Vec3 sampleDiffuseDirection(const Vec3& normal) {
	// A perfectly diffuse (Lambertian) surface does not reflect light into one
	// deterministic mirror direction. Instead, incoming light is scattered into
	// many directions over the hemisphere above the surface.
	//
	// In a path tracer we approximate that integral by Monte Carlo sampling: each
	// path continues in one randomly chosen direction. If we average many paths,
	// the image converges to the hemispherical light integral.
	//
	// Vec3::randomUnitVector() gives us a random direction on the unit sphere.
	// Adding the surface normal shifts that random direction into the hemisphere
	// around the normal:
	//
	//   randomUnitVector() points everywhere on the sphere
	//   normal + randomUnitVector() points mostly away from the surface
	//
	// This is a simple cosine-weighted sampling trick. Directions close to the
	// normal are sampled more often than grazing directions, which matches the
	// cosine term of Lambert's law well enough for this introductory exercise.
	// Because the sampling distribution already contains the cosine-like weight,
	// the path throughput later only has to be multiplied by the material albedo.
	Vec3 direction = normal + Vec3::randomUnitVector();

	// Very rarely, the random vector can be almost exactly opposite to the normal.
	// Then the sum is close to zero and cannot be normalized reliably. In that
	// degenerate case we fall back to the normal direction itself.
	if (direction.sqlength() < 0.000001f)
		direction = normal;

	// The ray class expects normalized directions. The length is irrelevant for a
	// ray direction, but normalizing keeps intersection distances meaningful.
	return Vec3::normalize(direction);
}

Vec3 offsetRayOrigin(const Vec3& position, const Vec3& normal, const Vec3& direction) {
	constexpr float offsetEpsilon = 0.00001f;
	const float side = Vec3::dot(direction, normal) > 0.0f ? 1.0f : -1.0f;
	return position + normal * (offsetEpsilon * side);
}

Vec3 getAlbedo(const Intersection& inter)
{
	Vec3 albedo = inter.getMaterial().getDiffuse();
	if (inter.getMaterial().hasTexture() && inter.getTexCoords().has_value())
		albedo = albedo * inter.getMaterial().getTexture().value().sample(inter.getTexCoords().value());

	return albedo;
}

Vec3 environmentColor(const Vec3& direction)
{
	const Vec3 dir = Vec3::normalize(direction);
	const Vec3 horizon{0.32f, 0.36f, 0.43f};
	const Vec3 zenith{0.78f, 0.90f, 1.12f};
	const Vec3 ground{0.22f, 0.21f, 0.20f};

	if (dir.y > 0.0f)
	{
		const float t = std::sqrt(std::clamp(dir.y, 0.0f, 1.0f));
		const float zenithSpot = std::pow(std::clamp(dir.y, 0.0f, 1.0f), 40.0f);
		return horizon * (1.0f - t) + zenith * t + Vec3{10.0f, 9.5f, 8.0f} * zenithSpot;
	}

	const float t = std::clamp(-dir.y, 0.0f, 1.0f);
	return horizon * (1.0f - t) + ground * t;
}

} // namespace

void Scene::addObject(std::shared_ptr<const IntersectableObject> object) {
	sceneObjects.push_back(object);
}

void Scene::setModel(const Mat4& model) {
	// This local-space raytracing path assumes translations, rotations, and uniform scales only.
	this->model = model;
}

Mat4 Scene::getModel() const {
	return model;
}

Vec3 Scene::getBackgroundcolor() const {
	return backgroundColor;
}

std::vector<float> Scene::getTriangleData() const {
	std::vector<float> data;

	for (std::shared_ptr<const IntersectableObject> object : sceneObjects)
	{
		const Tessellation mesh = object->getMesh().unpack();
		const std::vector<float>& vertices = mesh.getVertices();
		const std::vector<float>& normals = mesh.getNormals();
		const Vec3 color = object->getMaterial().getDiffuse();
		const size_t vertexCount = vertices.size() / 3;

		data.reserve(data.size() + vertexCount * 10);
		for (size_t i = 0; i < vertexCount; ++i)
		{
			data.push_back(vertices[i * 3 + 0]);
			data.push_back(vertices[i * 3 + 1]);
			data.push_back(vertices[i * 3 + 2]);
			data.push_back(color.r);
			data.push_back(color.g);
			data.push_back(color.b);
			data.push_back(1.0f);

			if (normals.size() >= (i + 1) * 3)
			{
				data.push_back(normals[i * 3 + 0]);
				data.push_back(normals[i * 3 + 1]);
				data.push_back(normals[i * 3 + 2]);
			}
			else
			{
				data.push_back(0.0f);
				data.push_back(0.0f);
				data.push_back(1.0f);
			}
		}
	}

	return data;
}

std::optional<Intersection> Scene::intersect(const Ray& ray) const {
	std::optional<Intersection> result{};
	for (std::shared_ptr<const IntersectableObject> object : sceneObjects)
	{
		std::optional<Intersection> i = object->intersect(ray);
		if (!i)
			continue;

		if (!result || i.value().getT() < result.value().getT())
			result = i;
	}
	return result;
}

Vec3 Scene::tracePath(const Ray& ray, int maxDepth) const {
	const Mat4 inverseModel = Mat4::inverse(model);
	const Vec3 localDirection = Vec3::normalize(transformDirection(inverseModel, ray.getDirection()));
	if (localDirection.sqlength() == 0.0f)
		return backgroundColor;

	const Ray localRay{ inverseModel * ray.getOrigin(), localDirection };
	return traceLocalPath(localRay, maxDepth);
}

Vec3 Scene::traceLocalPath(const Ray& firstRay, int maxDepth) const {
	// This function has the same basic job as traceLocalRay() in the previous
	// texturing raytracer: follow a ray through the scene, find intersections,
	// react to the material, and return the color seen along that ray.
	//
	// The important path tracing change is that we no longer compute direct
	// illumination from explicit light sources at every surface. There are no
	// point-light loops and no shadow rays here. Instead, each ray represents one
	// possible light path. At every bounce we choose one continuation direction,
	// update the path throughput, and keep walking until the path escapes into the
	// environment or reaches maxDepth.
	Ray ray = firstRay;

	// Throughput stores how much light can still be carried along the current
	// path. It starts white, then every material interaction multiplies it by the
	// material color / texture / specular transmission. When the ray finally hits
	// the sky, the sky radiance is multiplied by this accumulated throughput.
	Vec3 throughput{1.0f, 1.0f, 1.0f};

	for (int depth = 0; depth < maxDepth; ++depth)
	{
		// Intersection computation is reused almost directly from the raytracer.
		// We still need the nearest object hit by the current ray.
		std::optional<Intersection> optIntersection = intersect(ray);

		// A miss means the ray left the scene. In this exercise the environment is
		// the light source, so the path ends by collecting the sky color.
    if (!optIntersection.has_value()) {
      if (environment)
        return throughput * environmentColor(transformDirection(model, ray.getDirection()));
      else
        return throughput * backgroundColor;
    }

		const Intersection inter = optIntersection.value();
		const Vec3 interPos = ray.getPosOnRay(inter.getT());
		const Vec3 normal = inter.getNormal();

		const Material material = inter.getMaterial();
		Vec3 nextDirection;

		// In this simplified renderer, a material either emits light or scatters
		// the path, but not both. Emissive geometry terminates the path.
		if (material.emits())
			return throughput * material.getEmission();

		if (material.refracts())
		{
			// Refraction is mostly the same idea as in the previous assignment:
			// compute a refracted direction using the material's index of
			// refraction. If total internal reflection happens, there is no valid
			// refracted direction and we fall back to reflection.
			const float reflectivity = material.getReflectivity(Vec3::dot(ray.getDirection(), normal));
			const std::optional<Vec3> refracted = Vec3::refract(ray.getDirection(), normal, material.getIndexOfRefraction().value());

			// In the recursive raytracer we combined reflection and refraction by
			// tracing both rays and blending them with Fresnel weights. In a path
			// tracer we trace only one continuation ray per path. The Fresnel
			// reflectivity becomes a probability: sometimes this path reflects,
			// sometimes it refracts. Averaged over many paths this converges to the
			// same mixture.
			if (!refracted || staticRand.rand01() < reflectivity)
				nextDirection = Vec3::reflect(ray.getDirection(), normal);
			else
				nextDirection = refracted.value();

			// Specular/glass surfaces attenuate the path by their specular color.
			throughput = throughput * material.getSpecular();
		}
		else if (material.reflects())
		{
			// Perfect mirror reflection is also reused from the raytracer. Unlike a
			// diffuse bounce, the outgoing direction is deterministic.
			nextDirection = Vec3::reflect(ray.getDirection(), normal);
			throughput = throughput * material.getSpecular();
		}
		else
		{
			// This is the main new path tracing part for diffuse materials. The old
			// raytracer evaluated local illumination by looping over lights and
			// computing ambient + diffuse + specular terms. Here we choose one
			// random diffuse bounce direction. Repeating this for many paths turns
			// indirect light, color bleeding, and soft environment illumination into
			// an average instead of an explicit lighting formula.
			const Vec3 shadingNormal = Vec3::dot(ray.getDirection(), normal) < 0.0f ? normal : normal * -1.0f;
			nextDirection = sampleDiffuseDirection(shadingNormal);
			throughput = throughput * getAlbedo(inter);
		}

		// Continue the path from just above the surface. The small offset avoids
		// immediately intersecting the same surface again because of floating point
		// precision.
		nextDirection = Vec3::normalize(nextDirection);
		ray = Ray{offsetRayOrigin(interPos, normal, nextDirection), nextDirection};
	}

	// If the path did not reach the environment within maxDepth, we terminate it
	// without adding light. A more advanced renderer could use Russian roulette
	// instead of this fixed cutoff.
	return Vec3{0.0f, 0.0f, 0.0f};
}

Scene Scene::genPathTracingScene() {
	Scene s;

	Texture checkerboard = Texture::genCheckerboardTexture(2, 2);
	Texture earth("Earth.png");

	Material glass(Vec3{0.35f, 0.55f, 1.0f}, Vec3{0.65f, 0.78f, 1.0f}, 0.0f, 1.52f);
	s.addObject(std::make_shared<Sphere>(Vec3{0.7f, -0.4f, -2.0f}, 0.9f, glass));

	Material earthMaterial(Vec3{1.0f, 1.0f, 1.0f}, Vec3{}, 1.0f, std::nullopt, earth);
	s.addObject(std::make_shared<Sphere>(Vec3{-0.9f, -0.1f, -2.2f}, 0.6f, earthMaterial, Vec3{-90.0f, 0.0f, -90.0f}));

	Material redDiffuse(Vec3{0.95f, 0.08f, 0.04f});
	s.addObject(std::make_shared<Sphere>(Vec3{2.15f, -0.95f, -3.35f}, 0.55f, redDiffuse));

	Material mirror(Vec3{1.0f, 0.9f, 0.1f}, Vec3{1.0f, 0.85f, 0.15f}, 0.0f);
	s.addObject(std::make_shared<Sphere>(Vec3{0.0f, 4.0f, -8.0f}, 3.9f, mirror, Vec3{-60.0f, 0.0f, -90.0f}));

	Material floor(Vec3{0.75f, 0.75f, 0.75f}, Vec3{}, 1.0f, std::nullopt, checkerboard);
	s.addObject(std::make_shared<Plane>(Vec3{0.0f, 1.0f, 0.0f}, 1.5f, floor));

	return s;
}

Scene Scene::genCornellBox() {
  Scene s{Vec3{0.0f, 0.0f, 0.0f}};

	const Material whiteDiffuse{Vec3{1.0f, 1.0f, 1.0f}};
	const Material redDiffuse{Vec3{0.75f, 0.08f, 0.04f}};
	const Material blueDiffuse{Vec3{0.10f, 0.12f, 0.62f}};
	const Material glass{Vec3{1.0f, 1.0f, 1.0f}, Vec3{1.0f, 1.0f, 1.0f}, 0.0f, 1.52f};
	const Material mirror{Vec3{1.0f, 1.0f, 1.0f}, Vec3{1.0f, 1.0f, 1.0f}, 0.0f};
	const Material light{Vec3{1.0f, 0.96f, 0.82f}, Vec3{}, 1.0f, std::nullopt, std::nullopt, Vec3{18.0f, 16.0f, 12.0f}};

	s.addObject(std::make_shared<Plane>(Vec3{0.0f, 1.0f, 0.0f}, 1.25f, whiteDiffuse));
	s.addObject(std::make_shared<Plane>(Vec3{0.0f, -1.0f, 0.0f}, 1.45f, whiteDiffuse));
	s.addObject(std::make_shared<Plane>(Vec3{0.0f, 0.0f, 1.0f}, 4.2f, whiteDiffuse));
	s.addObject(std::make_shared<Plane>(Vec3{1.0f, 0.0f, 0.0f}, 1.65f, redDiffuse));
	s.addObject(std::make_shared<Plane>(Vec3{-1.0f, 0.0f, 0.0f}, 1.65f, blueDiffuse));

	s.addObject(std::make_shared<Sphere>(Vec3{0.0f, 11.44f, -1.0f}, 10.0f, light));
	s.addObject(std::make_shared<Sphere>(Vec3{-0.55f, -0.72f, -2.15f}, 0.48f, mirror));
	s.addObject(std::make_shared<Sphere>(Vec3{0.55f, -0.78f, -1.7f}, 0.42f, glass));

	return s;
}


Scene Scene::genCausticsScene() {
  Scene s{Vec3{0.0f, 0.0f, 0.0f}};

  const Material grayDiffuse{Vec3{0.5f, 0.5f, 0.5f}};
  const Material glass{Vec3{1.0f, 1.0f, 1.0f}, Vec3{1.0f, 1.0f, 1.0f}, 0.0f, 1.52f};
  const Material redGlass{Vec3{1.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}, 0.1f, 1.52f};
  const Material greenGlass{Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, 0.1f, 1.52f};
  const Material blueGlass{Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, 1.0f}, 0.1f, 1.52f};
  const Material light{Vec3{1.0f, 0.96f, 0.82f}, Vec3{}, 1.0f, std::nullopt, std::nullopt, Vec3{18.0f, 16.0f, 12.0f}};

  s.addObject(std::make_shared<Sphere>(Vec3{0.0f, 6.0f, -1.0f}, 1.0f, light));
  s.addObject(std::make_shared<Sphere>(Vec3{0.55f, -0.18f, -1.7f}, 0.42f, greenGlass));
  s.addObject(std::make_shared<Sphere>(Vec3{-0.85f, -0.48f, -1.7f}, 0.42f, glass));
  s.addObject(std::make_shared<Sphere>(Vec3{0.0f, 0.6f, -1.7f}, 0.42f, redGlass));
  s.addObject(std::make_shared<Sphere>(Vec3{0.0f, 1.2f, -4.7f}, 1.42f, blueGlass));
  s.addObject(std::make_shared<Sphere>(Vec3{0.0f, -0.18f, -0.7f}, 0.42f, glass));

  s.addObject(std::make_shared<Plane>(Vec3{0.0f, 1.0f, 0.0f}, 1.25f, grayDiffuse));
  return s;
}
