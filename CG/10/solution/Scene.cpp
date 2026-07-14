#include "Scene.h"
#include "Material.h"
#include "PointLight.h"
#include "Sphere.h"
#include "Triangle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

Vec3 transformDirection(const Mat4& matrix, const Vec3& direction)
{
  return (matrix * Vec4{direction, 0.0f}).xyz;
}

Material makeDiffuseMaterial(const Vec3& color) {
  return Material{color * 0.25f, color, Vec3{0.18f, 0.18f, 0.18f}, 16.0f};
}

void addTriangle(Scene& scene, const Vec3& a, const Vec3& b, const Vec3& c, const Material& material) {
  scene.addObject(std::make_shared<Triangle>(a, b, c, material));
}

void addQuad(Scene& scene, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Material& material) {
  addTriangle(scene, a, b, c, material);
  addTriangle(scene, a, c, d, material);
}

void addObjects(Scene& scene, const std::vector<std::shared_ptr<const IntersectableObject>>& objects) {
  for (const std::shared_ptr<const IntersectableObject>& object : objects) {
    scene.addObject(object);
  }
}

void appendLineVertex(std::vector<float>& data, const Vec3& position, const Vec3& color, float alpha) {
  data.push_back(position.x);
  data.push_back(position.y);
  data.push_back(position.z);
  data.push_back(color.r);
  data.push_back(color.g);
  data.push_back(color.b);
  data.push_back(alpha);
}

void appendLine(std::vector<float>& data, const Vec3& a, const Vec3& b, const Vec3& color, float alpha) {
  appendLineVertex(data, a, color, alpha);
  appendLineVertex(data, b, color, alpha);
}

void appendBoxLines(std::vector<float>& data, const AABB& bounds, const Vec3& color, float alpha) {
  const Vec3 minCorner = bounds.getMin();
  const Vec3 maxCorner = bounds.getMax();
  const Vec3 p000{minCorner.x, minCorner.y, minCorner.z};
  const Vec3 p100{maxCorner.x, minCorner.y, minCorner.z};
  const Vec3 p010{minCorner.x, maxCorner.y, minCorner.z};
  const Vec3 p110{maxCorner.x, maxCorner.y, minCorner.z};
  const Vec3 p001{minCorner.x, minCorner.y, maxCorner.z};
  const Vec3 p101{maxCorner.x, minCorner.y, maxCorner.z};
  const Vec3 p011{minCorner.x, maxCorner.y, maxCorner.z};
  const Vec3 p111{maxCorner.x, maxCorner.y, maxCorner.z};

  appendLine(data, p000, p100, color, alpha);
  appendLine(data, p100, p110, color, alpha);
  appendLine(data, p110, p010, color, alpha);
  appendLine(data, p010, p000, color, alpha);

  appendLine(data, p001, p101, color, alpha);
  appendLine(data, p101, p111, color, alpha);
  appendLine(data, p111, p011, color, alpha);
  appendLine(data, p011, p001, color, alpha);

  appendLine(data, p000, p001, color, alpha);
  appendLine(data, p100, p101, color, alpha);
  appendLine(data, p110, p111, color, alpha);
  appendLine(data, p010, p011, color, alpha);
}

std::array<AABB, 8> computeOctreeChildren(const AABB& bounds) {
  const Vec3 minCorner = bounds.getMin();
  const Vec3 maxCorner = bounds.getMax();
  const Vec3 center = bounds.center();
  std::array<AABB, 8> children;

  for (uint32_t child = 0; child < children.size(); ++child) {
    const Vec3 childMin{
      (child & 1) ? center.x : minCorner.x,
      (child & 2) ? center.y : minCorner.y,
      (child & 4) ? center.z : minCorner.z
    };
    const Vec3 childMax{
      (child & 1) ? maxCorner.x : center.x,
      (child & 2) ? maxCorner.y : center.y,
      (child & 4) ? maxCorner.z : center.z
    };
    children[child] = AABB{childMin, childMax};
  }

  return children;
}

AABB makeCubicBounds(const AABB& bounds) {
  if (bounds.isEmpty())
    return bounds;

  const Vec3 center = bounds.center();
  const Vec3 extent = bounds.extent();
  const float maxExtent = std::max(extent.x, std::max(extent.y, extent.z));
  const Vec3 halfExtent{maxExtent * 0.5f + 0.001f};
  return AABB{center - halfExtent, center + halfExtent};
}

void addOBJTriangles(Scene& scene, const std::string& filename, const Material& material, const Vec3& scale, const Vec3& translation) {
  std::vector<std::shared_ptr<const IntersectableObject>> triangles = Triangle::loadOBJ(filename, material, scale, translation);
  if (triangles.empty())
    triangles = Triangle::loadOBJ("Datasets/" + filename, material, scale, translation);

  addObjects(scene, triangles);
}

} // namespace

void Scene::addObject(std::shared_ptr<const IntersectableObject> object) {
  sceneObjects.push_back(object);
  octreeObjectIndices.clear();
  octreeNodes.clear();
  octreeMailbox.clear();
}

void Scene::addLight(std::shared_ptr<const LightSource> ls) {
  lightSources.push_back(ls);
}

std::shared_ptr<const LightSource> Scene::getLight(size_t index) const {
  if (index >= lightSources.size())
    return {};

  return lightSources[index];
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

  for (std::shared_ptr<const IntersectableObject> object : sceneObjects) {
    object->appendTriangleData(data);
  }

  return data;
}

std::vector<float> Scene::getOctreeLineData(uint32_t maxDepth) const {
  std::vector<float> data;
  if (octreeNodes.empty())
    return data;

  const auto appendNode = [this, maxDepth, &data](const auto& self, uint32_t nodeIndex, uint32_t depth) -> void {
    if (depth > maxDepth)
      return;

    const float t = maxDepth == 0 ? 0.0f : float(depth) / float(maxDepth);
    const Vec3 color{1.0f - 0.35f * t, 0.85f - 0.45f * t, 0.20f + 0.80f * t};
    appendBoxLines(data, octreeNodes[nodeIndex].bounds, color, 0.75f);

    if (octreeNodes[nodeIndex].isLeaf())
      return;

    for (uint32_t child : octreeNodes[nodeIndex].children) {
      if (child != INVALID_NODE)
        self(self, child, depth + 1);
    }
  };

  appendNode(appendNode, 0, 0);
  return data;
}

void Scene::buildOctree() {
  octreeObjectIndices.clear();
  octreeNodes.clear();

  AABB sceneBounds;
  std::vector<size_t> finiteObjects;
  finiteObjects.reserve(sceneObjects.size());

  // First collect all objects that have a finite bounding box. The actual
  // sceneObjects vector is not reordered; the octree stores indices into it.
  for (size_t i = 0; i < sceneObjects.size(); ++i) {
    const AABB objectBounds = sceneObjects[i]->getBounds();
    if (objectBounds.isEmpty())
      continue;

    finiteObjects.push_back(i);
    sceneBounds.join(objectBounds);
  }

  // A true octree splits a cube into eight equally sized cubes. The scene may
  // be rectangular, so we expand its bounds to a cube before creating the root.
  if (!finiteObjects.empty())
    buildOctreeNode(makeCubicBounds(sceneBounds), finiteObjects, 0);
}

uint32_t Scene::buildOctreeNode(const AABB& bounds, const std::vector<size_t>& objectIndices, uint32_t depth) {
  OctreeNode node;
  node.bounds = bounds;

  // Store the node first and remember its index. Children will later store their
  // indices in this node's children array.
  const uint32_t nodeIndex = uint32_t(octreeNodes.size());
  octreeNodes.push_back(node);

  // Stop recursion if the node is already small enough or if we reached the
  // maximum depth. The leaf stores a contiguous range in octreeObjectIndices.
  if (objectIndices.size() <= OCTREE_LEAF_SIZE || depth >= OCTREE_MAX_DEPTH) {
    octreeNodes[nodeIndex].firstObject = uint32_t(octreeObjectIndices.size());
    octreeNodes[nodeIndex].objectCount = uint32_t(objectIndices.size());
    octreeObjectIndices.insert(octreeObjectIndices.end(), objectIndices.begin(), objectIndices.end());
    return nodeIndex;
  }

  const std::array<AABB, 8> childBounds = computeOctreeChildren(bounds);
  std::array<std::vector<size_t>, 8> childObjects;
  std::vector<size_t> localObjects;

  for (size_t objectIndex : objectIndices) {
    const AABB objectBounds = sceneObjects[objectIndex]->getBounds();
    std::array<uint32_t, 8> overlappingChildren{};
    uint32_t overlappingChildCount = 0;

    // Insert a primitive into every child cell touched by its bounds. This is
    // the important optimization compared to keeping all boundary-crossing
    // triangles in the parent: most bunny triangles are small and belong only to
    // one or two child cells.
    for (uint32_t child = 0; child < childBounds.size(); ++child) {
      if (childBounds[child].overlaps(objectBounds))
        overlappingChildren[overlappingChildCount++] = child;
    }

    // Large objects, for example the floor, can overlap many children. If we
    // duplicated them everywhere, memory use and traversal work would explode.
    // Keep such objects in the current node and test them once when entering it.
    if (overlappingChildCount > OCTREE_MAX_CHILD_REFERENCES) {
      localObjects.push_back(objectIndex);
    } else {
      for (uint32_t i = 0; i < overlappingChildCount; ++i)
        childObjects[overlappingChildren[i]].push_back(objectIndex);
    }
  }

  bool hasChildObjects = false;
  for (const std::vector<size_t>& objects : childObjects) {
    if (!objects.empty()) {
      hasChildObjects = true;
      break;
    }
  }

  // If all objects stayed local, subdivision would not improve the tree. In
  // that case, make this node a leaf and stop.
  if (!hasChildObjects) {
    octreeNodes[nodeIndex].firstObject = uint32_t(octreeObjectIndices.size());
    octreeNodes[nodeIndex].objectCount = uint32_t(objectIndices.size());
    octreeObjectIndices.insert(octreeObjectIndices.end(), objectIndices.begin(), objectIndices.end());
    return nodeIndex;
  }

  octreeNodes[nodeIndex].firstObject = uint32_t(octreeObjectIndices.size());
  octreeNodes[nodeIndex].objectCount = uint32_t(localObjects.size());
  octreeObjectIndices.insert(octreeObjectIndices.end(), localObjects.begin(), localObjects.end());

  // Recursively build only non-empty children. Empty cells stay INVALID_NODE and
  // are skipped immediately by traversal.
  for (uint32_t child = 0; child < childObjects.size(); ++child) {
    if (!childObjects[child].empty())
      octreeNodes[nodeIndex].children[child] = buildOctreeNode(childBounds[child], childObjects[child], depth + 1);
  }

  return nodeIndex;
}

std::optional<Intersection> Scene::intersectBruteForce(const Ray& ray, bool shadowRay) const {
  std::optional<Intersection> result{};
  for (std::shared_ptr<const IntersectableObject> object : sceneObjects) {
    if (shadowRay && !object->getMaterial().isShadowCaster())
      continue;

    std::optional<Intersection> i = object->intersect(ray);
    if (!i)
      continue;

    if (!result || i.value().getT() < result.value().getT())
      result = i;
  }
  return result;
}

std::optional<Intersection> Scene::intersectOctreeNode(uint32_t nodeIndex, const Ray& ray, bool shadowRay, float tEnter, float tExit, float maxT, uint32_t mailboxStamp) const {
  static constexpr float DDA_EPSILON = 0.00001f;

  const OctreeNode& node = octreeNodes[nodeIndex];
  if (tEnter > tExit || tEnter > maxT)
    return {};

  std::optional<Intersection> result;

  // Objects stored directly in this node either belong to a leaf or were kept
  // local because they touched too many children. Test them when the ray enters
  // the node.
  for (uint32_t i = 0; i < node.objectCount; ++i) {
    const size_t objectIndex = octreeObjectIndices[node.firstObject + i];
    // The same primitive can appear in several child cells. The mailbox marks
    // primitives already tested by this ray so duplicated references do not
    // cause duplicated intersection tests.
    if (octreeMailbox[objectIndex] == mailboxStamp)
      continue;
    octreeMailbox[objectIndex] = mailboxStamp;

    const std::shared_ptr<const IntersectableObject>& object = sceneObjects[objectIndex];
    if (shadowRay && !object->getMaterial().isShadowCaster())
      continue;

    std::optional<Intersection> intersection = object->intersect(ray);
    if (!intersection || intersection->getT() > maxT)
      continue;

    if (!result || intersection->getT() < result->getT()) {
      result = intersection;
      maxT = intersection->getT();
    }
  }

  if (node.isLeaf())
    return result;

  const Vec3 minCorner = node.bounds.getMin();
  const Vec3 maxCorner = node.bounds.getMax();
  const Vec3 center = node.bounds.center();
  const Vec3 origin = ray.getOrigin();
  const Vec3 direction = ray.getDirection();
  const float startT = std::max(tEnter, 0.0f);
  const Vec3 startPoint = ray.getPosOnRay(std::min(startT + DDA_EPSILON, tExit));

  // DDA state for the node's 2x2x2 child grid:
  // cell     current child coordinate, each component is 0 or 1
  // step     direction in which the ray leaves the current child on each axis
  // tMaxAxis ray parameter of the next child boundary on each axis
  // tDelta   parameter distance between two child boundaries on each axis
  std::array<int, 3> cell{};
  std::array<int, 3> step{};
  std::array<float, 3> tMaxAxis{};
  std::array<float, 3> tDelta{};

  for (int axis = 0; axis < 3; ++axis) {
    // The current child is selected by comparing the ray entry point with the
    // node center. The child index later packs these three bits as x + 2y + 4z.
    cell[axis] = startPoint.e[axis] < center.e[axis] ? 0 : 1;

    if (std::abs(direction.e[axis]) < DDA_EPSILON) {
      // A parallel ray will never cross a child boundary on this axis.
      step[axis] = 0;
      tMaxAxis[axis] = std::numeric_limits<float>::infinity();
      tDelta[axis] = std::numeric_limits<float>::infinity();
      continue;
    }

    if (direction.e[axis] > 0.0f) {
      step[axis] = 1;
      const float boundary = cell[axis] == 0 ? center.e[axis] : maxCorner.e[axis];
      tMaxAxis[axis] = (boundary - origin.e[axis]) / direction.e[axis];
      tDelta[axis] = (maxCorner.e[axis] - minCorner.e[axis]) * 0.5f / direction.e[axis];
    } else {
      step[axis] = -1;
      const float boundary = cell[axis] == 1 ? center.e[axis] : minCorner.e[axis];
      tMaxAxis[axis] = (boundary - origin.e[axis]) / direction.e[axis];
      tDelta[axis] = -(maxCorner.e[axis] - minCorner.e[axis]) * 0.5f / direction.e[axis];
    }
  }

  float currentT = startT;
  while (cell[0] >= 0 && cell[0] < 2 &&
         cell[1] >= 0 && cell[1] < 2 &&
         cell[2] >= 0 && cell[2] < 2 &&
         currentT <= tExit && currentT <= maxT) {
    // The next cell boundary is the earliest boundary reached on any axis. The
    // current child is valid for the interval [currentT, nextT].
    const float nextT = std::min(tExit, std::min(tMaxAxis[0], std::min(tMaxAxis[1], tMaxAxis[2])));
    const uint32_t childIndex = uint32_t(cell[0]) | (uint32_t(cell[1]) << 1) | (uint32_t(cell[2]) << 2);
    const uint32_t childNode = node.children[childIndex];

    if (childNode != INVALID_NODE) {
      std::optional<Intersection> childIntersection = intersectOctreeNode(childNode, ray, shadowRay, currentT, nextT, maxT, mailboxStamp);
      if (childIntersection) {
        result = childIntersection;
        maxT = childIntersection->getT();
        // Cells are visited front to back. If the closest hit is inside the
        // interval of the current child, no later child can contain a closer hit.
        if (maxT <= nextT + DDA_EPSILON)
          return result;
      }
    }

    if (nextT >= tExit || nextT > maxT)
      break;

    // Step to the next child. If the ray hits an edge or a corner, several
    // axes can have the same nextT and all of them must advance.
    for (int axis = 0; axis < 3; ++axis) {
      if (tMaxAxis[axis] <= nextT + DDA_EPSILON) {
        cell[axis] += step[axis];
        tMaxAxis[axis] += tDelta[axis];
      }
    }
    currentT = nextT;
  }

  return result;
}

std::optional<Intersection> Scene::intersectOctree(const Ray& ray, bool shadowRay) const {
  if (octreeNodes.empty())
    return intersectBruteForce(ray, shadowRay);

  float tEnter = 0.0f;
  float tExit = 0.0f;
  if (!octreeNodes[0].bounds.intersectInterval(ray, tEnter, tExit, 0.0f, std::numeric_limits<float>::max()))
    return {};

  // Start a new mailbox stamp for this ray. Clearing a large array for every
  // ray would be expensive; incrementing a stamp gives the same effect. The
  // array is only cleared when the stamp wraps around.
  if (octreeMailbox.size() != sceneObjects.size())
    octreeMailbox.assign(sceneObjects.size(), 0);

  ++octreeMailboxStamp;
  if (octreeMailboxStamp == 0) {
    std::fill(octreeMailbox.begin(), octreeMailbox.end(), 0);
    octreeMailboxStamp = 1;
  }

  return intersectOctreeNode(0, ray, shadowRay, tEnter, tExit, std::numeric_limits<float>::max(), octreeMailboxStamp);
}

std::optional<Intersection> Scene::intersect(const Ray& ray, bool shadowRay, bool useOctree) const {
  return useOctree ? intersectOctree(ray, shadowRay) : intersectBruteForce(ray, shadowRay);
}

/// <summary>
/// Trace a ray through the scene and compute its color value.
/// </summary>
/// <param name="ray">to trace</param>
/// <param name="IOR">optical density of the material we are currently travelling in</param>
/// <param name="recDepth">recursion depth</param>
/// <returns>final color value computed for this ray</returns>
Vec3 Scene::traceRay(const Ray& ray, float IOR, int recDepth, bool useOctree) const {
  const Mat4 inverseModel = Mat4::inverse(model);
  const Vec3 localDirection = Vec3::normalize(transformDirection(inverseModel, ray.getDirection()));
  if (localDirection.sqlength() == 0.0f)
    return backgroundColor;

  const Ray localRay{ inverseModel * ray.getOrigin(), localDirection };
  return traceLocalRay(localRay, IOR, recDepth, useOctree);
}

Vec3 Scene::traceLocalRay(const Ray& ray, float IOR, int recDepth, bool useOctree) const {
  if (recDepth == 0)
    return backgroundColor;

  // no intersection found
  std::optional<Intersection> opt_intersection = intersect(ray, false, useOctree);
  if (!opt_intersection)
    return backgroundColor;

  // else intersection found, do recursive ray tracing
  Intersection inter = opt_intersection.value();
  Vec3 interPos = ray.getPosOnRay(inter.getT());

  Vec3 reflColor{ 0.0f, 0.0f, 0.0f };
  if (inter.getMaterial().reflects()) {
    Vec3 reflDir = Vec3::reflect(ray.getDirection(), inter.getNormal());
    Vec3 reflOrigin = interPos + inter.getNormal() * (Vec3::dot(reflDir, inter.getNormal()) > 0 ? OFFSET_EPSILON : -OFFSET_EPSILON);
    Ray reflRay{reflOrigin, reflDir};
    reflColor = traceLocalRay(reflRay, IOR, recDepth - 1, useOctree);
  }

  Vec3 refractionColor{ 0.0f, 0.0f, 0.0f };
  if (inter.getMaterial().refracts()) {
    const float matIOR = *inter.getMaterial().getIndexOfRefraction();
    std::optional<Vec3> potentialRefDir = Vec3::refract(ray.getDirection(), inter.getNormal(), matIOR);
    if (potentialRefDir) {
      const Vec3 refDir = *potentialRefDir;
      if (Vec3::dot(refDir, inter.getNormal()) > 0) {
        // Ray --> from material into air
        Ray refrRay{ interPos + inter.getNormal() * OFFSET_EPSILON, refDir };
        refractionColor = traceLocalRay(refrRay, 1.0, recDepth - 1, useOctree);
      } else {
        // Ray --> from air into material
        Vec3 inSurfacePos = interPos + inter.getNormal() * -OFFSET_EPSILON;
        Ray refrRay{ inSurfacePos, refDir };
        refractionColor = traceLocalRay(refrRay, matIOR, recDepth - 1, useOctree);
      }
    } else {
      // Total internal reflection
    }
  }


  Vec3 localColor{ 0.0f, 0.0f, 0.0f };
  for (std::shared_ptr<const LightSource> ls : lightSources) {
    const Vec3 offSurfacePos = interPos + inter.getNormal() * OFFSET_EPSILON;
    Ray shadowRay{ offSurfacePos, ls->getDirection(offSurfacePos) };
    std::optional<Intersection> shadowInter = intersect(shadowRay, true, useOctree);

    Vec3 ambient = inter.getMaterial().getAmbient() * ls->getAmbient();

    if (!shadowInter || shadowInter->getT() > ls->getDistance(offSurfacePos)) {
      float d = Vec3::dot(ls->getDirection(offSurfacePos), inter.getNormal());
      Vec3 diffuse = inter.getMaterial().getDiffuse() * ls->getDiffuse() * d;
      diffuse = Vec3::clamp(diffuse, 0.0f, 1.0f);

      Vec3 Rv = Vec3::reflect(ray.getDirection(), inter.getNormal());
      float s = pow(std::max(0.0f, Vec3::dot(Rv, ls->getDirection(offSurfacePos))), inter.getMaterial().getExp());
      Vec3 specular = inter.getMaterial().getSpecular() * ls->getSpecular() * s;
      specular = Vec3::clamp(specular, 0.0f, 1.0f);

      localColor = localColor + ambient + diffuse + specular;
    } else {
      localColor = localColor + ambient;
    }
  }

  // compose final color
  float cosI = Vec3::dot(ray.getDirection(), inter.getNormal());
  float l = 0, r = 0, t = 0;
  if (inter.getMaterial().refracts()) {
    l = inter.getMaterial().getLocalRefectivity();
    r = inter.getMaterial().getReflectivity(cosI);
    t = 1 - r;
    r = (1 - l) * r;
    t = (1 - l) * t;
  } else if (inter.getMaterial().reflects()) {
    r = inter.getMaterial().getReflectivity(cosI);
    l = 1 - r;
  } else {
    l = 1;
  }

  return localColor * l + reflColor * r + refractionColor * t;
}

Scene Scene::genSimpleScene() {
  Scene s;

  const auto l = std::make_shared<const PointLight>(Vec3{0.0f, 5.5f, -2.0f}, Vec3{0.22f, 0.22f, 0.22f}, Vec3{1.0f, 1.0f, 1.0f}, Vec3{1.0f, 1.0f, 1.0f});
  s.addLight(l);

  const Material floorMaterial = makeDiffuseMaterial(Vec3{0.55f, 0.55f, 0.50f});
  addQuad(s, Vec3{-12.0f, -1.5f, 3.0f}, Vec3{12.0f, -1.5f, 3.0f}, Vec3{12.0f, -1.5f, -15.0f}, Vec3{-12.0f, -1.5f, -15.0f}, floorMaterial);

  const Material bunnyMaterial = makeDiffuseMaterial(Vec3{0.74f, 0.62f, 0.46f});
  addOBJTriangles(s, "bunny.obj", bunnyMaterial, Vec3{2.65f}, Vec3{-0.25f, -0.17f, -4.2f});

  const Material glassMaterial{Vec3{0.02f, 0.03f, 0.04f}, Vec3{0.62f, 0.78f, 0.95f}, Vec3{1.0f, 1.0f, 1.0f}, 96.0f, 0.04f, 1.52f, false};
  s.addObject(std::make_shared<Sphere>(Vec3{-2.2f, -0.70f, -3.2f}, 0.80f, glassMaterial));
  s.addObject(std::make_shared<Sphere>(Vec3{1.9f, -0.62f, -4.4f}, 0.88f, glassMaterial));
  s.addObject(std::make_shared<Sphere>(Vec3{-1.7f, -0.48f, -6.0f}, 1.02f, glassMaterial));
  s.addObject(std::make_shared<Sphere>(Vec3{1.1f, -0.95f, -6.6f}, 0.55f, glassMaterial));

  const Material mirrorMaterial{Vec3{0.08f, 0.08f, 0.08f}, Vec3{0.2f, 0.2f, 0.22f}, Vec3{0.95f, 0.95f, 0.92f}, 64.0f, 0.25f};
  s.addObject(std::make_shared<Sphere>(Vec3{3.0f, -0.95f, -2.8f}, 0.55f, mirrorMaterial));

  const Material blueMaterial = makeDiffuseMaterial(Vec3{0.18f, 0.38f, 0.82f});
  s.addObject(std::make_shared<Sphere>(Vec3{-3.2f, -1.05f, -5.0f}, 0.45f, blueMaterial));

  const Material redMaterial = makeDiffuseMaterial(Vec3{0.85f, 0.18f, 0.12f});
  s.addObject(std::make_shared<Sphere>(Vec3{2.8f, -1.12f, -5.7f}, 0.38f, redMaterial));

  s.buildOctree();
  return s;
}
