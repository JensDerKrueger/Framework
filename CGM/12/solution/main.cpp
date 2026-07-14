#include <GLApp.h>
#include <Image.h>
#include <Vec2.h>
#include <Vec3.h>

#include "SignalGenerator.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <utility>

namespace {
constexpr uint32_t imageWidth = 800;
constexpr uint32_t imageHeight = 600;
constexpr uint32_t samplingImageWidth = imageWidth / 2;
constexpr uint32_t samplingImageHeight = imageHeight / 2;
constexpr uint32_t comparisonImageCount = 4;

enum class SamplingMode {
  Point,
  Regular,
  Jittered,
  Reference
};

Vec2 pixelToUV(const uint32_t x,
               const uint32_t y,
               const Vec2& subPixelOffset,
               const uint32_t width,
               const uint32_t height) {
  const float u = (float(x) + subPixelOffset.x) / float(width);
  const float v = (float(y) + subPixelOffset.y) / float(height);
  return {u, v};
}

Vec2 regularSampleOffset(const uint32_t sampleX,
                         const uint32_t sampleY,
                         const uint32_t samplesPerAxis) {
  // Regular supersampling partitions the pixel into an N x N grid. The sample
  // is placed at the center of each sub-cell, so every pixel uses the same
  // deterministic sub-pixel pattern.
  const float inv = 1.0f / float(samplesPerAxis);
  return {(float(sampleX) + 0.5f) * inv,
          (float(sampleY) + 0.5f) * inv};
}

Vec2 jitteredSampleOffset(const uint32_t sampleX,
                          const uint32_t sampleY,
                          const uint32_t samplesPerAxis) {
  // Jittered supersampling keeps the stratification of the regular grid, but
  // moves each sample randomly inside its own stratum. This preserves coverage
  // while turning regular alias patterns into less structured noise.
  const Vec2 jitter = Vec2::random();
  const float inv = 1.0f / float(samplesPerAxis);
  return {(float(sampleX) + jitter.x) * inv,
          (float(sampleY) + jitter.y) * inv};
}

uint32_t samplesPerAxisForMode(const SamplingMode mode) {
  // The four images compare increasing quality levels in reading order. The
  // jittered image uses more samples than regular supersampling because a
  // single low-sample jittered image can trade structured aliases for visible
  // noise. The reference image uses still more samples and therefore comes
  // closest to the pixel integral.
  switch (mode) {
    case SamplingMode::Regular:
      return 4U;
    case SamplingMode::Jittered:
      return 8U;
    case SamplingMode::Reference:
      return 16U;
    case SamplingMode::Point:
      return 1U;
  }

  return 1U;
}

Vec3 samplePixel(const uint32_t x,
                 const uint32_t y,
                 const uint32_t width,
                 const uint32_t height,
                 const SamplingMode mode,
                 const int sceneIndex,
                 const float frequencyScale) {
  if (mode == SamplingMode::Point) {
    const Vec2 uv = pixelToUV(x, y, {0.5f, 0.5f}, width, height);
    return evaluateContinuousSignal(uv, sceneIndex, frequencyScale);
  }

  const uint32_t samplesPerAxis = samplesPerAxisForMode(mode);
  Vec3 color = {0.0f, 0.0f, 0.0f};

  // Supersampling approximates the pixel integral by evaluating the continuous
  // signal several times inside the pixel and averaging the results. The
  // jittered modes keep one sample per stratum but randomize its position.
  for (uint32_t sy = 0; sy < samplesPerAxis; ++sy) {
    for (uint32_t sx = 0; sx < samplesPerAxis; ++sx) {
      Vec2 offset = regularSampleOffset(sx, sy, samplesPerAxis);

      if (mode == SamplingMode::Jittered || mode == SamplingMode::Reference) {
        offset = jitteredSampleOffset(sx, sy, samplesPerAxis);
      }

      const Vec2 uv = pixelToUV(x, y, offset, width, height);
      color = color + evaluateContinuousSignal(uv, sceneIndex, frequencyScale);
    }
  }

  const float sampleCount = float(samplesPerAxis * samplesPerAxis);
  return color / sampleCount;
}

void setPixel(Image& image, const uint32_t x, const uint32_t y, const Vec3& color) {
  image.setNormalizedValue(x, y, 0, std::clamp(color.r, 0.0f, 1.0f));
  image.setNormalizedValue(x, y, 1, std::clamp(color.g, 0.0f, 1.0f));
  image.setNormalizedValue(x, y, 2, std::clamp(color.b, 0.0f, 1.0f));
}

Image renderSamplingImage(const SamplingMode mode,
                          const int sceneIndex,
                          const float frequencyScale) {
  Image image(samplingImageWidth, samplingImageHeight, 3);

  for (uint32_t y = 0; y < samplingImageHeight; ++y) {
    for (uint32_t x = 0; x < samplingImageWidth; ++x) {
      const Vec3 color = samplePixel(x, y, samplingImageWidth, samplingImageHeight,
                                     mode, sceneIndex, frequencyScale);
      setPixel(image, x, y, color);
    }
  }

  return image;
}

constexpr std::array<SamplingMode, comparisonImageCount> comparisonModes = {
  SamplingMode::Point,
  SamplingMode::Regular,
  SamplingMode::Jittered,
  SamplingMode::Reference
};

const std::array<std::pair<Vec2, Vec2>, comparisonImageCount> imageBounds = {{
  {Vec2{-1.0f,  0.0f}, Vec2{ 0.0f,  1.0f}},
  {Vec2{ 0.0f,  0.0f}, Vec2{ 1.0f,  1.0f}},
  {Vec2{-1.0f, -1.0f}, Vec2{ 0.0f,  0.0f}},
  {Vec2{ 0.0f, -1.0f}, Vec2{ 1.0f,  0.0f}},
}};
}

class MyGLApp : public GLApp {
public:
  std::array<Image, comparisonImageCount> samplingImages{
    Image{samplingImageWidth, samplingImageHeight, 3},
    Image{samplingImageWidth, samplingImageHeight, 3},
    Image{samplingImageWidth, samplingImageHeight, 3},
    Image{samplingImageWidth, samplingImageHeight, 3}
  };
  int sceneIndex{0};
  float frequencyScale{1.0f};
  uint32_t jitterSeed{1};

  MyGLApp() :
  GLApp(imageWidth, imageHeight, 1, "Exercise 12 - Sampling", true, true, true) {
  }

  virtual void init() override {
    renderSamplingImages();
  }

  void renderSamplingImages() {
    staticRand.seed(jitterSeed);

    for (uint32_t imageIndex = 0; imageIndex < comparisonImageCount; ++imageIndex) {
      samplingImages[imageIndex] = renderSamplingImage(comparisonModes[imageIndex],
                                                       sceneIndex,
                                                       frequencyScale);
    }
  }

  virtual void draw() override {
    for (uint32_t imageIndex = 0; imageIndex < comparisonImageCount; ++imageIndex) {
      drawImage(samplingImages[imageIndex],
                imageBounds[imageIndex].first,
                imageBounds[imageIndex].second);
    }
  }

  virtual void keyboard(int key, int scancode, int action, int mods) override {
    if (action != GLENV_PRESS) {
      return;
    }

    switch (key) {
      case GLENV_KEY_ESCAPE:
        closeWindow();
        break;
      case GLENV_KEY_S:
        sceneIndex = (sceneIndex + 1) % 3;
        renderSamplingImages();
        break;
      case GLENV_KEY_J:
        ++jitterSeed;
        renderSamplingImages();
        break;
      case GLENV_KEY_UP:
        frequencyScale = std::min(frequencyScale * 1.15f, 3.0f);
        renderSamplingImages();
        break;
      case GLENV_KEY_DOWN:
        frequencyScale = std::max(frequencyScale / 1.15f, 0.35f);
        renderSamplingImages();
        break;
      case GLENV_KEY_R:
        sceneIndex = 0;
        frequencyScale = 1.0f;
        jitterSeed = 1;
        renderSamplingImages();
        break;
    }
  }
};

int main(int argc, char** argv) {
  MyGLApp myApp;
  myApp.run();
  return EXIT_SUCCESS;
}
