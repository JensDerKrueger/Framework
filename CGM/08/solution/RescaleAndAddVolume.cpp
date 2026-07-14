#include "RescaleAndAddVolume.h"

#include <algorithm>
#include <cmath>

RescaleAndAddVolume::RescaleAndAddVolume(const RescaleAndAddVolumeParameters& parameters) :
  parameters{parameters}
{
}

Volume RescaleAndAddVolume::generate() {
  generate(parameters);
  return volume;
}

void RescaleAndAddVolume::generate(const RescaleAndAddVolumeParameters& parameters) {
  this->parameters = parameters;
  buildPermutation(parameters.seed);

  volume.width = std::max<size_t>(1, parameters.width);
  volume.height = std::max<size_t>(1, parameters.height);
  volume.depth = std::max<size_t>(1, parameters.depth);
  volume.scale = parameters.scale;
  volume.normalizeScale();

  volume.data.resize(volume.width * volume.height * volume.depth);

  const float widthDenominator = float(std::max<size_t>(1, volume.width - 1));
  const float heightDenominator = float(std::max<size_t>(1, volume.height - 1));
  const float depthDenominator = float(std::max<size_t>(1, volume.depth - 1));

  size_t index{0};
  for (size_t w = 0; w < volume.depth; ++w) {
    const float z = float(w) / depthDenominator;
    for (size_t v = 0; v < volume.height; ++v) {
      const float y = float(v) / heightDenominator;
      for (size_t u = 0; u < volume.width; ++u) {
        const float x = float(u) / widthDenominator;
        const float noise = fractalNoise(x, y, z);
        const float envelope = cloudEnvelope(x, y, z);
        const float detail = perlin(x * 13.0f + 5.7f,
                                    y * 13.0f + 7.3f,
                                    z * 13.0f + 11.1f) * 0.5f + 0.5f;
        const float erosion = 1.0f - parameters.detailErosion * (1.0f - detail);
        const float shapedNoise = noise * (0.20f + 0.80f * envelope) * erosion;
        const float cloudShape = smoothStep(parameters.coverage, 1.0f, shapedNoise);
        const float density = std::clamp(std::pow(cloudShape,
                                                  parameters.densityExponent) *
                                         parameters.densityScale +
                                         parameters.densityOffset,
                                         0.0f, 1.0f) * envelope;
        volume.data[index++] = uint8_t(std::round(density * 255.0f));
      }
    }
  }

  if (parameters.computeNormals) {
    volume.computeNormals();
  } else {
    volume.normals.clear();
  }
}

void RescaleAndAddVolume::buildPermutation(const uint32_t seed) {
  std::array<int, 256> values;
  for (size_t i = 0; i < values.size(); ++i) {
    values[i] = int(i);
  }

  uint32_t state = seed == 0 ? 1 : seed;
  for (size_t i = values.size() - 1; i > 0; --i) {
    state = state * 1664525u + 1013904223u;
    const size_t j = size_t(state % uint32_t(i + 1));
    std::swap(values[i], values[j]);
  }

  for (size_t i = 0; i < permutation.size(); ++i) {
    permutation[i] = values[i & 255];
  }
}

float RescaleAndAddVolume::fractalNoise(const float x, const float y, const float z) const {
  float value{0.0f};
  float amplitude{1.0f};
  float amplitudeSum{0.0f};
  float frequency{parameters.baseFrequency};

  for (size_t octave = 0; octave < parameters.octaves; ++octave) {
    const float octaveValue = perlin(x * frequency, y * frequency, z * frequency);
    const float noise = parameters.billowyNoise
      ? 1.0f - std::abs(octaveValue)
      : octaveValue * 0.5f + 0.5f;
    value += noise * amplitude;
    amplitudeSum += amplitude;
    amplitude *= parameters.roughness;
    frequency *= parameters.lacunarity;
  }

  if (amplitudeSum == 0.0f) return 0.0f;
  return value / amplitudeSum;
}

float RescaleAndAddVolume::perlin(const float inputX, const float inputY, const float inputZ) const {
  float x = inputX;
  float y = inputY;
  float z = inputZ;

  const int cellX = int(std::floor(x)) & 255;
  const int cellY = int(std::floor(y)) & 255;
  const int cellZ = int(std::floor(z)) & 255;

  x -= std::floor(x);
  y -= std::floor(y);
  z -= std::floor(z);

  const float u = fade(x);
  const float v = fade(y);
  const float w = fade(z);

  const int a = perm(cellX) + cellY;
  const int aa = perm(a) + cellZ;
  const int ab = perm(a + 1) + cellZ;
  const int b = perm(cellX + 1) + cellY;
  const int ba = perm(b) + cellZ;
  const int bb = perm(b + 1) + cellZ;

  return lerp(
    lerp(
      lerp(gradient(perm(aa), x, y, z),
           gradient(perm(ba), x - 1.0f, y, z),
           u),
      lerp(gradient(perm(ab), x, y - 1.0f, z),
           gradient(perm(bb), x - 1.0f, y - 1.0f, z),
           u),
      v),
    lerp(
      lerp(gradient(perm(aa + 1), x, y, z - 1.0f),
           gradient(perm(ba + 1), x - 1.0f, y, z - 1.0f),
           u),
      lerp(gradient(perm(ab + 1), x, y - 1.0f, z - 1.0f),
           gradient(perm(bb + 1), x - 1.0f, y - 1.0f, z - 1.0f),
           u),
      v),
    w);
}

float RescaleAndAddVolume::gradient(const int hash, const float x, const float y, const float z) const {
  const int h = hash & 15;
  const float u = h < 8 ? x : y;
  const float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
  return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float RescaleAndAddVolume::cloudEnvelope(const float x, const float y, const float z) const {
  const float silhouetteNoise = perlin((x + 12.5f) * parameters.silhouetteNoiseFrequency,
                                       (y + 23.5f) * parameters.silhouetteNoiseFrequency,
                                       (z + 34.5f) * parameters.silhouetteNoiseFrequency) * 0.5f + 0.5f;
  const float radiusScale = 1.0f - parameters.silhouetteNoiseStrength * 0.5f +
                            silhouetteNoise * parameters.silhouetteNoiseStrength;

  const float dx = x - 0.5f;
  const float dy = y - 0.5f;
  const float dz = z - 0.5f;
  const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  const float radius = std::max(0.001f, parameters.radius * radiusScale);
  const float softness = std::max(0.001f, parameters.envelopeSoftness);

  return 1.0f - smoothStep(radius - softness, radius, distance);
}

int RescaleAndAddVolume::perm(const int index) const {
  return permutation[size_t(index & 511)];
}

float RescaleAndAddVolume::fade(const float t) {
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float RescaleAndAddVolume::lerp(const float a, const float b, const float t) {
  return a + t * (b - a);
}

float RescaleAndAddVolume::smoothStep(const float edge0, const float edge1, const float x) {
  const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
