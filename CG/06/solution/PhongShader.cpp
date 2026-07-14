#include "PhongShader.h"
#include <cmath>
#include <algorithm>

PhongShader::PhongShader(const Vec3& viewer, const Vec3& light, const Vec3& light_ambient_color, const Vec3& light_diffuse_color, const Vec3& light_specular_color, float exponent)
: light(light), viewer(viewer), la(light_ambient_color),
  ld(light_diffuse_color), ls(light_specular_color), r(exponent)
{
}

PhongShader::PhongShader(const PhongShader& other)
: light(other.light), viewer(other.viewer), la(other.la), ld(other.ld), ls(other.ls), r(other.r)
{
}

Vec3 PhongShader::shade(Vertex surface) const {
  const Material m = surface.material;
  Vec3 c = m.color_ambient * la;

  const Vec3 N = Vec3::normalize(surface.normal);
  const Vec3 L = Vec3::normalize(light - surface.position);
  const float d = std::fmaxf(0.0f, Vec3::dot(N, L));

  c = c + m.color_diffuse * ld * d;

  const Vec3 R = N * 2.0f * Vec3::dot(N, L) - L;
  const Vec3 V = Vec3::normalize(viewer - surface.position);
  const float s = (float)pow(std::max(0.0f, Vec3::dot(V, R)), r);

  c = c + m.color_specular * ls * s;

  return c;
}
