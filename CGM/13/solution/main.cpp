#include <GLApp.h>
#include <Image.h>
#include <Vec2.h>
#include <Vec3.h>

#include "JPEGCodec.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

constexpr uint32_t windowWidth = 1000;
constexpr uint32_t windowHeight = 560;
constexpr uint32_t sourceWidth = 384;
constexpr uint32_t sourceHeight = 256;

float fract(const float value) {
  return value - std::floor(value);
}

float smoothStep(const float edge0, const float edge1, const float value) {
  const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

Vec3 mix(const Vec3& a, const Vec3& b, const float t) {
  return a * (1.0f - t) + b * t;
}

void setPixel(Image& image, const uint32_t x, const uint32_t y, const Vec3& color) {
  image.setNormalizedValue(x, y, 0, std::clamp(color.x, 0.0f, 1.0f));
  image.setNormalizedValue(x, y, 1, std::clamp(color.y, 0.0f, 1.0f));
  image.setNormalizedValue(x, y, 2, std::clamp(color.z, 0.0f, 1.0f));
}

Image createDemoImage() {
  Image image{sourceWidth, sourceHeight, 3};

  for (uint32_t y = 0; y < sourceHeight; ++y) {
    for (uint32_t x = 0; x < sourceWidth; ++x) {
      const Vec2 uv{float(x) / float(sourceWidth - 1U),
                    float(y) / float(sourceHeight - 1U)};
      const Vec2 centered = uv * 2.0f - Vec2{1.0f, 1.0f};
      const float radius = centered.length();
      const float diagonal = centered.x * 0.78f + centered.y * 0.62f;
      const float rings = std::abs(fract(radius * 34.0f) - 0.5f) < 0.075f ? 1.0f : 0.0f;
      const float stripes = std::abs(fract(diagonal * 42.0f) - 0.5f) < 0.08f ? 1.0f : 0.0f;
      const float checker = (int(std::floor(uv.x * 18.0f)) + int(std::floor(uv.y * 13.0f))) & 1;

      Vec3 color = mix(Vec3{0.12f, 0.20f, 0.34f}, Vec3{0.88f, 0.82f, 0.62f}, uv.y);
      color = mix(color, Vec3{0.95f, 0.12f, 0.12f}, smoothStep(0.58f, 0.48f, radius) * rings);
      color = mix(color, Vec3{0.05f, 0.36f, 0.95f}, smoothStep(-0.6f, 0.9f, centered.x) * stripes * 0.75f);
      color = mix(color, Vec3{0.06f, 0.06f, 0.06f}, checker * smoothStep(0.54f, 0.76f, uv.y) * 0.35f);

      const float discA = (centered - Vec2{-0.52f, -0.28f}).length();
      const float discB = (centered - Vec2{0.48f, 0.22f}).length();
      color = mix(color, Vec3{1.0f, 0.86f, 0.10f}, smoothStep(0.24f, 0.20f, discA));
      color = mix(color, Vec3{0.05f, 0.85f, 0.38f}, smoothStep(0.20f, 0.16f, discB));

      setPixel(image, x, y, color);
    }
  }

  return image;
}

std::string byteCountString(const size_t bytes) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(1) << (double(bytes) / 1024.0) << " KiB";
  return stream.str();
}

} // namespace

class MyGLApp : public GLApp {
public:
  Image sourceImage{createDemoImage()};
  Image reconstructedImage{sourceWidth, sourceHeight, 3};
  JPEG::CompressionResult compressionResult;
  int quality{70};
  JPEG::ChromaMode chromaMode{JPEG::ChromaMode::YCbCr420};
  JPEG::QuantizationPreset quantizationPreset{JPEG::QuantizationPreset::JPEGStandard};

  MyGLApp() :
  GLApp(windowWidth, windowHeight, 1, "Exercise 13 - Image Compression", true, true, true) {
  }

  virtual void init() override {
    setBackground(0.90f, 0.90f, 0.86f, 1.0f);
    recompress();
  }

  void recompress() {
    compressionResult = JPEG::compressImage(sourceImage,
                                            quality,
                                            chromaMode,
                                            quantizationPreset);
    reconstructedImage = compressionResult.reconstructed;
    updateTitle();
  }

  void updateTitle() {
    std::ostringstream title;
    title << "Exercise 13 - Image Compression | Q " << quality
          << " | " << JPEG::chromaModeName(chromaMode)
          << " | " << JPEG::quantizationPresetName(quantizationPreset)
          << " | " << byteCountString(compressionResult.jpegBytes.size());
    glEnv.setTitle(title.str());
  }

  void saveCurrentJPEG() const {
#ifndef __EMSCRIPTEN__
    std::ostringstream filename;
    filename << "compressed_q" << quality << "_"
             << JPEG::chromaModeName(chromaMode) << ".jpg";
    std::string safeFilename = filename.str();
    std::replace(safeFilename.begin(), safeFilename.end(), ':', '_');
    JPEG::saveJPEG(safeFilename, compressionResult.jpegBytes);
#endif
  }

  virtual void draw() override {
    drawImage(sourceImage, Vec2{-0.98f, -0.84f}, Vec2{-0.02f, 0.84f});
    drawImage(reconstructedImage, Vec2{0.02f, -0.84f}, Vec2{0.98f, 0.84f});
  }

  void cycleChromaMode() {
    switch (chromaMode) {
      case JPEG::ChromaMode::YCbCr444:
        chromaMode = JPEG::ChromaMode::YCbCr422;
        break;
      case JPEG::ChromaMode::YCbCr422:
        chromaMode = JPEG::ChromaMode::YCbCr420;
        break;
      case JPEG::ChromaMode::YCbCr420:
        chromaMode = JPEG::ChromaMode::YCbCr444;
        break;
    }
  }

  void cycleQuantizationPreset() {
    switch (quantizationPreset) {
      case JPEG::QuantizationPreset::JPEGStandard:
        quantizationPreset = JPEG::QuantizationPreset::Flat;
        break;
      case JPEG::QuantizationPreset::Flat:
        quantizationPreset = JPEG::QuantizationPreset::HighFrequency;
        break;
      case JPEG::QuantizationPreset::HighFrequency:
        quantizationPreset = JPEG::QuantizationPreset::JPEGStandard;
        break;
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
      case GLENV_KEY_UP:
        quality = std::min(quality + 5, 95);
        recompress();
        break;
      case GLENV_KEY_DOWN:
        quality = std::max(quality - 5, 5);
        recompress();
        break;
      case GLENV_KEY_C:
        cycleChromaMode();
        recompress();
        break;
      case GLENV_KEY_M:
        cycleQuantizationPreset();
        recompress();
        break;
      case GLENV_KEY_W:
        saveCurrentJPEG();
        break;
      case GLENV_KEY_R:
        quality = 70;
        chromaMode = JPEG::ChromaMode::YCbCr420;
        quantizationPreset = JPEG::QuantizationPreset::JPEGStandard;
        recompress();
        break;
    }
  }
};

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--save") {
    const Image source = createDemoImage();
    const JPEG::CompressionResult result =
      JPEG::compressImage(source, 70,
                          JPEG::ChromaMode::YCbCr420,
                          JPEG::QuantizationPreset::JPEGStandard);
    const std::string filename = "compressed_q70_4_2_0.jpg";
    if (!JPEG::saveJPEG(filename, result.jpegBytes)) {
      std::cerr << "Could not save " << filename << "\n";
      return EXIT_FAILURE;
    }
    std::cout << "Saved " << filename << " (" << result.jpegBytes.size() << " bytes)\n";
    return EXIT_SUCCESS;
  }

  MyGLApp myApp;
  myApp.run();
  return EXIT_SUCCESS;
}

/*
 Copyright (c) 2026 Computer Graphics and Visualization Group, University of
 Duisburg-Essen

 Permission is hereby granted, free of charge, to any person obtaining a copy of
 this software and associated documentation files (the "Software"), to deal in the
 Software without restriction, including without limitation the rights to use, copy,
 modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and
 to permit persons to whom the Software is furnished to do so, subject to the following
 conditions:

 The above copyright notice and this permission notice shall be included in all copies
 or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
