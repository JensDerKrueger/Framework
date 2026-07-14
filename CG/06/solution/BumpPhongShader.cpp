#define _USE_MATH_DEFINES
#include <cmath>

#include "BumpPhongShader.h"

/**
 * @param phong a phong shader to use for the actual shading
 * @param cellSize size of the bumps in the x-y-plane measured in pixels
 * @param bumpHeight amplitude of the bumps in positive z-direction (out of the image plane)
 */
BumpPhongShader::BumpPhongShader(const PhongShader& phong, float cellSize,
                                 float bumpHeight)
: PhongShader(phong), cellSize(cellSize), bumpHeight(bumpHeight)
{
}

static float f(float x, float y)  {
  const double sinX = sin(M_PI * x);
  const double sinY = sin(M_PI * y);
  return float(sinX * sinX * sinY * sinY);
}

static float fx(float x, float y)  {
  const double sinX = sin(M_PI * x);
  const double sinY = sin(M_PI * y);
  const double cosX = cos(M_PI * x);
  return float(2 * M_PI * sinY * sinY * cosX * sinX);
}

static float fy(float x, float y)  {
  const double sinX = sin(M_PI * x);
  const double sinY = sin(M_PI * y);
  const double cosY = cos(M_PI * y);
  return float(2 * M_PI * sinX * sinX * cosY * sinY);
}

// Inherited via Shader
Vec3 BumpPhongShader::shade(Vertex surface) const {
  const Vec3 cell{ surface.position.x / cellSize,
                   surface.position.y / cellSize,
                   0.0f };

  surface.position = { surface.position.x,
                       surface.position.y,
                       bumpHeight * f(cell.x, cell.y) };
  
  surface.normal = { bumpHeight * -fx(cell.x, cell.y),
                     bumpHeight * -fy(cell.x, cell.y),
                     1.0f };

  return PhongShader::shade(surface);
}
