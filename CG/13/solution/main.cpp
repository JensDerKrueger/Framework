#include <GLApp.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <sstream>
#include <vector>

struct PixelPoint {
  int x;
  int y;
};

class MyGLApp : public GLApp {
public:
  Image image{800, 520, 4};
  float animationPhase{0.0f};

  MyGLApp() : GLApp{800, 600, 1, "Solution 13 - Line Rasterization"} {
  }

  uint8_t toByte(const float value) const {
    return uint8_t(std::clamp(value, 0.0f, 1.0f) * 255.0f);
  }

  void clearImage(const Vec4& color) {
    for (uint32_t y = 0; y < image.height; ++y) {
      for (uint32_t x = 0; x < image.width; ++x) {
        const size_t index = image.computeIndex(x, y, 0);
        image.data[index + 0] = toByte(color.r);
        image.data[index + 1] = toByte(color.g);
        image.data[index + 2] = toByte(color.b);
        image.data[index + 3] = toByte(color.a);
      }
    }
  }

  void setPixel(const int x, const int y, const Vec4& color) {
    if (x < 0 || y < 0) return;
    if (x >= int(image.width) || y >= int(image.height)) return;

    const size_t index = image.computeIndex(uint32_t(x), uint32_t(y), 0);
    image.data[index + 0] = toByte(color.r);
    image.data[index + 1] = toByte(color.g);
    image.data[index + 2] = toByte(color.b);
    image.data[index + 3] = toByte(color.a);
  }

  void drawDisc(const PixelPoint center, const int radius, const Vec4& color) {
    const int radiusSquared = radius * radius;
    for (int y = -radius; y <= radius; ++y) {
      for (int x = -radius; x <= radius; ++x) {
        if (x * x + y * y <= radiusSquared) {
          setPixel(center.x + x, center.y + y, color);
        }
      }
    }
  }

  void drawGrid() {
    for (int y = 20; y < int(image.height); y += 40) {
      for (uint32_t x = 0; x < image.width; ++x) {
        setPixel(int(x), y, {0.88f, 0.90f, 0.94f, 1.0f});
      }
    }
    for (int x = 20; x < int(image.width); x += 40) {
      for (uint32_t y = 0; y < image.height; ++y) {
        setPixel(x, int(y), {0.88f, 0.90f, 0.94f, 1.0f});
      }
    }
    for (uint32_t y = 0; y < image.height; ++y) {
      setPixel(int(image.width / 2), int(y), {0.25f, 0.28f, 0.34f, 1.0f});
    }
  }

  void drawLineDDA(const PixelPoint a, const PixelPoint b, const Vec4& color) {
    // DDA treats the line as a continuous parametric segment and samples it in
    // regular steps. The number of steps is chosen from the dominant axis, so
    // every iteration advances by at most one pixel in x and at most one pixel in
    // y. The resulting floating point position is rounded to the nearest pixel.
    const int dx = b.x - a.x;
    const int dy = b.y - a.y;
    const int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps == 0) {
      setPixel(a.x, a.y, color);
      return;
    }

    const float xStep = float(dx) / float(steps);
    const float yStep = float(dy) / float(steps);
    float x = float(a.x);
    float y = float(a.y);

    // Each iteration follows the mathematical line by one DDA step. This is easy
    // to understand and works for all slopes, but it uses floating point
    // arithmetic and rounding for every pixel.
    for (int i = 0; i <= steps; ++i) {
      setPixel(int(std::round(x)), int(std::round(y)), color);
      x += xStep;
      y += yStep;
    }
  }

  void drawLineBresenham(PixelPoint a, const PixelPoint b, const Vec4& color) {
    // Bresenham's algorithm avoids floating point arithmetic. It keeps an
    // integer error value that measures how far the discrete pixel path has
    // drifted from the ideal line. Depending on this error, the algorithm steps
    // in x, in y, or in both directions.
    const int dx = std::abs(b.x - a.x);
    const int dy = std::abs(b.y - a.y);
    const int sx = a.x < b.x ? 1 : -1;
    const int sy = a.y < b.y ? 1 : -1;
    int error = dx - dy;

    while (true) {
      setPixel(a.x, a.y, color);
      if (a.x == b.x && a.y == b.y) break;

      // The doubled error lets us compare against half-pixel thresholds without
      // using fractions. If the error became too large in one direction, we
      // compensate by taking a step in that axis and updating the error term.
      const int doubledError = 2 * error;
      if (doubledError > -dy) {
        error -= dy;
        a.x += sx;
      }
      if (doubledError < dx) {
        error += dx;
        a.y += sy;
      }
    }
  }

  PixelPoint leftPoint(const int x, const int y) const {
    return {x, y};
  }

  PixelPoint rightPoint(const int x, const int y) const {
    return {x + int(image.width / 2), y};
  }

  PixelPoint animatedPoint(const PixelPoint point, const size_t index) const {
    const float xOffset = 18.0f * std::sin(animationPhase * 0.9f + float(index) * 1.7f);
    const float yOffset = 14.0f * std::cos(animationPhase * 1.1f + float(index) * 1.3f);
    return {int(std::round(float(point.x) + xOffset)),
            int(std::round(float(point.y) + yOffset))};
  }

  void renderLineSet() {
    const std::vector<std::pair<PixelPoint, PixelPoint>> lines{
      {{50, 60}, {335, 105}},
      {{60, 140}, {345, 320}},
      {{80, 430}, {350, 155}},
      {{40, 465}, {360, 465}},
      {{190, 45}, {190, 460}},
      {{345, 60}, {65, 350}},
      {{75, 250}, {350, 250}}
    };

    const std::vector<Vec4> colors{
      {0.10f, 0.28f, 0.88f, 1.0f},
      {0.92f, 0.24f, 0.18f, 1.0f},
      {0.18f, 0.62f, 0.30f, 1.0f},
      {0.96f, 0.70f, 0.18f, 1.0f},
      {0.50f, 0.22f, 0.75f, 1.0f},
      {0.00f, 0.60f, 0.70f, 1.0f},
      {0.85f, 0.30f, 0.55f, 1.0f}
    };

    for (size_t i = 0; i < lines.size(); ++i) {
      const PixelPoint a = animatedPoint(lines[i].first, i * 2);
      const PixelPoint b = animatedPoint(lines[i].second, i * 2 + 1);
      drawLineDDA(leftPoint(a.x, a.y), leftPoint(b.x, b.y), colors[i]);
      drawLineBresenham(rightPoint(a.x, a.y), rightPoint(b.x, b.y), colors[i]);
      drawDisc(leftPoint(a.x, a.y), 3, {0.05f, 0.05f, 0.06f, 1.0f});
      drawDisc(leftPoint(b.x, b.y), 3, {0.05f, 0.05f, 0.06f, 1.0f});
      drawDisc(rightPoint(a.x, a.y), 3, {0.05f, 0.05f, 0.06f, 1.0f});
      drawDisc(rightPoint(b.x, b.y), 3, {0.05f, 0.05f, 0.06f, 1.0f});
    }
  }

  void renderScene() {
    clearImage({0.98f, 0.98f, 0.96f, 1.0f});
    drawGrid();
    renderLineSet();
  }

  virtual void init() override {
    setImageFilter(GL_NEAREST, GL_NEAREST);
    setAnimation(false);
    renderScene();
  }

  virtual void animate(double animationTime) override {
    animationPhase = float(animationTime);
    renderScene();
  }

  virtual void keyboard(int key, int scancode, int action, int mods) override {
    if (action != GLENV_PRESS) return;

    switch (key) {
      case GLENV_KEY_SPACE:
        setAnimation(!getAnimation());
        break;
      case GLENV_KEY_R:
        resetAnimation();
        animationPhase = 0.0f;
        renderScene();
        break;
      case GLENV_KEY_ESCAPE:
        closeWindow();
        break;
    }
  }

  virtual void draw() override {
    drawImage(image);
  }
} myApp;

#ifdef _WIN32
#include <Windows.h>
INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
#else
int main(int argc, char** argv) {
#endif
  try {
    myApp.run();
  }
  catch (const GLException& e) {
    std::stringstream ss;
    ss << "Insufficient OpenGL Support " << e.what();
#ifndef _WIN32
    std::cerr << ss.str().c_str() << std::endl;
#else
    MessageBoxA(NULL, ss.str().c_str(), "OpenGL Error", MB_ICONERROR | MB_OK);
#endif
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
