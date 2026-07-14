#include "Sphere.h"
#include <Mat3.h>
#include <cmath>

#ifndef M_PI
constexpr float M_PI = 3.14159265358979323846f;
#endif

Sphere::Sphere(const Vec3& center, float radius, const Material& material) :
  Sphere(center, radius, material, { 0, 0, 0 }, { 1.0f, 1.0f }, {0.0f, 0.0f})
{
}

Sphere::Sphere(const Vec3& center, float radius, const Material& material,
               const Vec3& rotation) :
  Sphere(center, radius, material, rotation, { 1.0f, 1.0f }, { 0.0f, 0.0f })
{
}

Sphere::Sphere(const Vec3& center, float radius, const Material& material,
               const Vec3& rotation, const TextureCoordinates& scale,
               const TextureCoordinates& bias) :
  center(center),
  sqradius(radius*radius),
  material(material),
  rotation(rotation),
  scale(scale),
  bias(bias)
{
}

Material Sphere::getMaterial() const {
	return material;
}

std::optional<Intersection> Sphere::intersect(const Ray& ray) const {
  const Vec3 l = center - ray.getOrigin();
  const float tCenter = Vec3::dot(l, ray.getDirection());
	if (tCenter < 0) return {};	// no intersection

  const float dSq = l.sqlength() - tCenter * tCenter;
	if (dSq > sqradius) return {};	// no intersection

	const float dist = sqrt(sqradius - dSq);
  float t = tCenter - dist;

	if (t < 0) t = tCenter + dist;	// when inside sphere

	const Vec3 normal = Vec3::normalize(ray.getPosOnRay(t) - center);
  if (!material.hasTexture()) return Intersection{material, normal, {}, t };

  const Vec3 r = Mat3::rotationZ(rotation.z) *
                 Mat3::rotationY(rotation.y) *
                 Mat3::rotationX(rotation.x) * normal;

  // normalize between [0, 1]
  const float u = atan2f(r.y, r.x) * 0.5f / float(M_PI) + 0.5f;
  const float v = acos(r.z / r.length()) / float(M_PI);

  TextureCoordinates tc{ u * scale.u + bias.u, v * scale.v + bias.v};
  return Intersection{material, normal, tc, t };
}

Tessellation Sphere::getMesh() const
{
	return Tessellation::genSphere(center, std::sqrt(sqradius), 32, 16);
}
