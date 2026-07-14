#include <ImageLoader.h>
#include <GLApp.h>
#include <Vec2.h>
#include <GLFramebuffer.h>

#include <array>
#include <vector>

#include "Teapot.h"
#include "UnitPlane.h"

class LightProperties {
public:
  float degreesPerSecond{45.0f};
  float angle{0};
};

class MyGLApp : public GLApp {
public:
  struct TeapotInstance {
    Vec3 position;
    Vec3 diffuse;
    Vec3 specular;
    float scale{1.0f};
    float yaw{0.0f};
    float pitch{0.0f};
    float roll{0.0f};
    float shininess{70.0f};
  };

  LightProperties light;
  Mat4 projectionMatrix;

  GLTexture2D stonesDiffuse{GL_LINEAR, GL_LINEAR};
  GLTexture2D stonesSpecular{GL_LINEAR, GL_LINEAR};
  GLTexture2D stonesNormals{GL_LINEAR, GL_LINEAR};
  GLTexture2D udeNormals{GL_LINEAR, GL_LINEAR};

  GLTexture2D diffuseBuffer{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D specularBuffer{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D normalBuffer{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D positionBuffer{GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D rawAmbientOcclusionBuffer{GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D tempAmbientOcclusionBuffer{GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D ambientOcclusionBuffer{GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLDepthBuffer depthBuffer;
  GLFramebuffer gBuffer;
  GLFramebuffer rawAmbientOcclusionFramebuffer;
  GLFramebuffer tempAmbientOcclusionFramebuffer;
  GLFramebuffer ambientOcclusionFramebuffer;
  Dimensions gBufferSize{0, 0};

  GLProgram pGBuffer;
  GLProgram pDeferred;
  GLProgram pSSAO;
  GLProgram pSSAOBlur;

  GLArray planeArray;
  GLBuffer planePosBuffer{GL_ARRAY_BUFFER};
  GLBuffer planeNormalBuffer{GL_ARRAY_BUFFER};
  GLBuffer planeTangBuffer{GL_ARRAY_BUFFER};
  GLBuffer planeBinBuffer{GL_ARRAY_BUFFER};
  GLBuffer planeTexCoordBuffer{GL_ARRAY_BUFFER};

  GLArray teapotArray;
  GLBuffer teapotPosBuffer{GL_ARRAY_BUFFER};
  GLBuffer teapotNormalBuffer{GL_ARRAY_BUFFER};
  GLBuffer teapotTangBuffer{GL_ARRAY_BUFFER};
  GLBuffer teapotBinBuffer{GL_ARRAY_BUFFER};
  GLBuffer teapotTexCoordBuffer{GL_ARRAY_BUFFER};
  GLBuffer teapotIndexBuffer{GL_ELEMENT_ARRAY_BUFFER};

  std::vector<TeapotInstance> teapotInstances;

  GLArray fullscreenArray;

  bool showDebugBuffers{false};
  bool showAmbientOcclusionBuffer{false};
  bool leftMouseDown{false};
  bool rightMouseDown{false};
  bool controlDown{false};

  bool cameraActive{false};
  bool firstCameraUpdate{true};
  Vec3 viewPosition = {0, 0, 145};
  Vec3 viewRotation = {-38, 0, 0};
  Vec2 mouse = {0, 0};
  float mouseSensitivity{0.15f};
  float mousewheelFactor{10.0f};

  Mat4 viewMatrix;
  Mat4 lightModelMatrix;
  Vec4 lightPosition;

  MyGLApp() :
  GLApp(800, 600, 1, "Solution 14 - Screen Space Ambient Occlusion"),
  pGBuffer{GLProgram::createFromFile("gbuffer.vert", "gbuffer.frag", "", false, true)},
  pDeferred{GLProgram::createFromFile("deferred.vert", "deferred.frag", "", false, true)},
  pSSAO{GLProgram::createFromFile("deferred.vert", "ssao.frag", "", false, true)},
  pSSAOBlur{GLProgram::createFromFile("deferred.vert", "ssaoBlur.frag", "", false, true)}
  {
  }

  virtual void init() override {
    setupTextures();
    generateTeapotPyramid();
    setupGeometry();
    GL(glDisable(GL_CULL_FACE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LESS));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    setAnimation(false);
    resetAnimation();
  }

  void setupTextures() {
    Image image = ImageLoader::load("Stones_Diffuse.png");
    stonesDiffuse.setData(image.data, image.width, image.height, image.componentCount);

    image = ImageLoader::load("Stones_Specular.png");
    stonesSpecular.setData(image.data, image.width, image.height, image.componentCount);

    image = ImageLoader::load("Stones_Normals.png");
    stonesNormals.setData(image.data, image.width, image.height, image.componentCount);

    image = ImageLoader::load("teapot_normals.png");
    udeNormals.setData(image.data, image.width, image.height, image.componentCount);
  }

  virtual void animate(double animationTime) override {
    light.angle = light.degreesPerSecond * float(animationTime);
  }

  void updateState() {
    viewMatrix = Mat4::lookAt(viewPosition, {0, 0, 0}, {0, 1, 0});

    viewMatrix = viewMatrix * Mat4::rotationX(viewRotation[0]);
    viewMatrix = viewMatrix * Mat4::rotationY(viewRotation[1]);
    viewMatrix = viewMatrix * Mat4::rotationZ(viewRotation[2]);

    lightModelMatrix = Mat4::rotationY(light.angle) * Mat4::translation(-80, 60, 80);
    lightPosition = viewMatrix * lightModelMatrix * Vec4(0, 0, 0, 1);
  }

  void ensureGBufferSize(const Dimensions dim) {
    if (gBufferSize.width == dim.width && gBufferSize.height == dim.height) return;

    diffuseBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    specularBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    normalBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    positionBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    // SSAO is computed in its own full-screen pass after the G-buffer pass.
    // The result is again an image-space texture: one floating point value per
    // pixel, stored as a grayscale color. White means "fully open", black means
    // "strongly occluded".
    rawAmbientOcclusionBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    tempAmbientOcclusionBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    ambientOcclusionBuffer.setEmpty(dim.width, dim.height, 4, GLDataType::FLOAT);
    depthBuffer.setSize(dim.width, dim.height);
    gBufferSize = dim;
  }

  void setGBufferMatrices(const Mat4& modelMatrix) {
    const Mat4 modelView = viewMatrix * modelMatrix;
    const Mat4 modelViewProjection = projectionMatrix * modelView;
    const Mat4 modelViewIT = Mat4::transpose(Mat4::inverse(modelView));

    pGBuffer.setUniform("MVP", modelViewProjection);
    pGBuffer.setUniform("MV", modelView);
    pGBuffer.setUniform("MVit", modelViewIT);
  }

  void setGBufferMaterial(const Vec3& diffuse,
                          const Vec3& specular,
                          const float shininess,
                          const float texCoordScale,
                          const int useDiffuseTexture,
                          const int useSpecularTexture,
                          const int useNormalTexture) {
    pGBuffer.setUniform("materialDiffuse", diffuse);
    pGBuffer.setUniform("materialSpecular", specular);
    pGBuffer.setUniform("shininess", shininess);
    pGBuffer.setUniform("texCoordScale", texCoordScale);
    pGBuffer.setUniform("useDiffuseTexture", useDiffuseTexture);
    pGBuffer.setUniform("useSpecularTexture", useSpecularTexture);
    pGBuffer.setUniform("useNormalTexture", useNormalTexture);
  }

  Vec3 colorFromPalette(const size_t index) const {
    static const std::array<Vec3, 8> colors{
      Vec3{0.56f, 0.68f, 0.96f},
      Vec3{0.96f, 0.56f, 0.50f},
      Vec3{0.58f, 0.86f, 0.62f},
      Vec3{0.98f, 0.84f, 0.46f},
      Vec3{0.74f, 0.58f, 0.88f},
      Vec3{0.94f, 0.68f, 0.46f},
      Vec3{0.52f, 0.84f, 0.88f},
      Vec3{0.92f, 0.58f, 0.72f}
    };

    return colors[index % colors.size()];
  }

  void addTeapotInstance(const Vec3& position,
                         const float scale,
                         const float yaw,
                         const float pitch,
                         const float roll,
                         const Vec3& diffuse,
                         const float shininess) {
    TeapotInstance instance;
    instance.position = position;
    instance.diffuse = diffuse;
    instance.specular = diffuse * 0.08f + Vec3{0.10f, 0.10f, 0.10f};
    instance.scale = scale;
    instance.yaw = yaw;
    instance.pitch = pitch;
    instance.roll = roll;
    instance.shininess = shininess;
    teapotInstances.push_back(instance);
  }

  void generateTeapotPyramid() {
    teapotInstances.clear();

    constexpr int layerCount = 4;
    constexpr float baseSpacing = 22.0f;
    constexpr float layerHeight = 17.0f;
    constexpr float baseScale = 0.54f;
    size_t colorIndex = 0;

    for (int layer = 0; layer < layerCount; ++layer) {
      const int rowCount = layerCount - layer;
      const float spacing = baseSpacing * (1.0f - 0.08f * float(layer));
      const float y = layerHeight * float(layer);
      const float scale = baseScale * (1.0f - 0.05f * float(layer));
      const float offset = -0.5f * spacing * float(rowCount - 1);

      for (int z = 0; z < rowCount; ++z) {
        for (int x = 0; x < rowCount; ++x) {
          const float px = offset + spacing * float(x);
          const float pz = offset + spacing * float(z);
          const float checker = float((x + z + layer) % 2);
          const float yaw = 35.0f * float(x) - 27.0f * float(z) + 18.0f * float(layer);
          const float pitch = checker == 0.0f ? -4.0f : 4.0f;
          const float roll = checker == 0.0f ? 3.0f : -3.0f;
          addTeapotInstance({px, y, pz}, scale, yaw, pitch, roll, colorFromPalette(colorIndex++), 34.0f + 4.0f * float(layer));
        }
      }
    }
  }

  void renderTeapotToGBuffer(const Mat4& modelMatrix,
                             const Vec3& diffuse,
                             const Vec3& specular,
                             const float shininess) {
    setGBufferMatrices(modelMatrix);
    setGBufferMaterial(diffuse, specular, shininess, 1.0f, 0, 0, 1);
    pGBuffer.setTexture("normalTexture", udeNormals, 2);
    teapotArray.bind();
    GL(glDrawElements(GL_TRIANGLES, sizeof(Teapot::indices) / sizeof(Teapot::indices[0]), GL_UNSIGNED_INT, (void*)0));
  }

  void renderSceneToGBuffer() {
    pGBuffer.enable();

    Mat4 modelMatrix = Mat4::scaling(100, 100, 100);
    setGBufferMatrices(modelMatrix);
    setGBufferMaterial({1, 1, 1}, {0.35f, 0.35f, 0.35f}, 28.0f, 8.0f, 1, 1, 1);
    pGBuffer.setTexture("diffuseTexture", stonesDiffuse, 0);
    pGBuffer.setTexture("specularTexture", stonesSpecular, 1);
    pGBuffer.setTexture("normalTexture", stonesNormals, 2);
    planeArray.bind();
    GL(glDrawArrays(GL_TRIANGLES, 0, sizeof(UnitPlane::vertices) / (3 * sizeof(UnitPlane::vertices[0]))));

    for (const TeapotInstance& teapot : teapotInstances) {
      modelMatrix = Mat4::translation(teapot.position) *
                    Mat4::rotationY(teapot.yaw) *
                    Mat4::rotationX(teapot.pitch) *
                    Mat4::rotationZ(teapot.roll) *
                    Mat4::scaling(teapot.scale);
      renderTeapotToGBuffer(modelMatrix, teapot.diffuse, teapot.specular, teapot.shininess);
    }
  }

  void generateDeferredBufferMipmaps() {
    // The SSAO shader samples positions at several radii around the current
    // pixel. For larger radii we use coarser mip levels. This gives the shader a
    // larger support area without needing a huge number of direct texture reads.
    diffuseBuffer.generateMipmap();
    specularBuffer.generateMipmap();
    normalBuffer.generateMipmap();
    positionBuffer.generateMipmap();
  }

  void renderScreenSpaceAmbientOcclusion() {
    GL(glDisable(GL_DEPTH_TEST));
    pSSAO.enable();
    // The SSAO shader reconstructs test positions in view space and then
    // projects them back to texture coordinates. Therefore it needs the same
    // projection matrix that was used while filling the G-buffer.
    pSSAO.setUniform("projectionMatrix", projectionMatrix);
    // radius is measured in view-space units. Larger values search farther away
    // and therefore darken larger creases, but can also create overly broad AO.
    pSSAO.setUniform("radius", 16.0f);
    // bias prevents a point from immediately occluding itself due to numerical
    // precision and depth-buffer discretization.
    pSSAO.setUniform("bias", 0.60f);
    // intensity maps the estimated occlusion to the final grayscale AO value.
    pSSAO.setUniform("intensity", 2.10f);
    // The sample vectors in ssao.frag contain 24 directions in the upper
    // hemisphere. Keeping the number as a uniform makes quality/performance
    // experiments easy.
    pSSAO.setUniform("sampleCount", 24);
    // Farther samples read from coarser mip levels of the position buffer.
    pSSAO.setUniform("maxMipLevel", 5.0f);
    // SSAO only needs geometry information from the G-buffer: the surface normal
    // to orient the hemisphere and the view-space position to compare depths.
    pSSAO.setTexture("normalBuffer", normalBuffer, 0);
    pSSAO.setTexture("positionBuffer", positionBuffer, 1);
    fullscreenArray.bind();
    GL(glDrawArrays(GL_TRIANGLES, 0, 3));
    GL(glEnable(GL_DEPTH_TEST));
  }

  void renderAmbientOcclusionBlurPass(const GLTexture2D& sourceBuffer, const Vec2& direction) {
    GL(glDisable(GL_DEPTH_TEST));
    pSSAOBlur.enable();
    // The first SSAO pass deliberately uses only a small number of hemisphere
    // samples and therefore produces a noisy image. The blur shader is applied
    // twice, once horizontally and once vertically. A separable 5-tap blur costs
    // 10 texture samples instead of a full 5 x 5 filter with 25 samples.
    pSSAOBlur.setUniform("direction", direction);
    pSSAOBlur.setUniform("depthFalloff", 12.0f);
    pSSAOBlur.setTexture("sourceAmbientOcclusionBuffer", sourceBuffer, 0);
    // The position buffer lets the blur become edge-aware. Samples with a very
    // different view-space depth receive little weight, so foreground objects do
    // not smear their AO across the background.
    pSSAOBlur.setTexture("positionBuffer", positionBuffer, 1);
    fullscreenArray.bind();
    GL(glDrawArrays(GL_TRIANGLES, 0, 3));
    GL(glEnable(GL_DEPTH_TEST));
  }

  void renderDeferredImage() {
    GL(glDisable(GL_DEPTH_TEST));
    pDeferred.enable();
    pDeferred.setUniform("showDebugBuffers", showDebugBuffers ? 1 : 0);
    pDeferred.setUniform("showAmbientOcclusionBuffer", showAmbientOcclusionBuffer ? 1 : 0);
    pDeferred.setUniform("lightPosition", lightPosition);
    pDeferred.setTexture("diffuseBuffer", diffuseBuffer, 0);
    pDeferred.setTexture("specularBuffer", specularBuffer, 1);
    pDeferred.setTexture("normalBuffer", normalBuffer, 2);
    pDeferred.setTexture("positionBuffer", positionBuffer, 3);
    // The final deferred lighting shader treats SSAO as a visibility term for
    // ambient and local contact light. The AO pass has already produced this
    // texture, so the final shader only has to read it.
    pDeferred.setTexture("ambientOcclusionBuffer", ambientOcclusionBuffer, 4);
    fullscreenArray.bind();
    GL(glDrawArrays(GL_TRIANGLES, 0, 3));
    GL(glEnable(GL_DEPTH_TEST));
  }

  virtual void draw() override {
    updateState();

    const Dimensions dim = glEnv.getFramebufferSize();
    ensureGBufferSize(dim);

    gBuffer.bind(diffuseBuffer, specularBuffer, normalBuffer, positionBuffer, depthBuffer);
    GL(glViewport(0, 0, GLsizei(dim.width), GLsizei(dim.height)));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 0.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    renderSceneToGBuffer();
    gBuffer.unbind2D();
    generateDeferredBufferMipmaps();

    // Second pass: compute one ambient occlusion value per visible screen pixel.
    // The pass renders only a full-screen triangle. All geometric information is
    // read back from the G-buffer textures created above.
    rawAmbientOcclusionFramebuffer.bind(rawAmbientOcclusionBuffer);
    GL(glViewport(0, 0, GLsizei(dim.width), GLsizei(dim.height)));
    GL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    renderScreenSpaceAmbientOcclusion();
    rawAmbientOcclusionFramebuffer.unbind2D();

    // Third pass: horizontal smoothing. The result goes into a temporary texture
    // because the vertical pass will read it next.
    tempAmbientOcclusionFramebuffer.bind(tempAmbientOcclusionBuffer);
    GL(glViewport(0, 0, GLsizei(dim.width), GLsizei(dim.height)));
    GL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    renderAmbientOcclusionBlurPass(rawAmbientOcclusionBuffer, Vec2{1.0f, 0.0f});
    tempAmbientOcclusionFramebuffer.unbind2D();

    // Fourth pass: vertical smoothing. This completes the separable blur and
    // writes the texture used by the final lighting pass. This is why the AO
    // computation is useful as a separate pass: intermediate image-space results
    // can be filtered, debugged, and reused.
    ambientOcclusionFramebuffer.bind(ambientOcclusionBuffer);
    GL(glViewport(0, 0, GLsizei(dim.width), GLsizei(dim.height)));
    GL(glClearColor(1.0f, 1.0f, 1.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT));
    renderAmbientOcclusionBlurPass(tempAmbientOcclusionBuffer, Vec2{0.0f, 1.0f});
    ambientOcclusionFramebuffer.unbind2D();

    GL(glViewport(0, 0, GLsizei(dim.width), GLsizei(dim.height)));
    GL(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
    renderDeferredImage();
  }

  virtual void resize(const Dimensions winDim, const Dimensions fbDim) override {
    GLApp::resize(winDim, fbDim);
    projectionMatrix = Mat4::perspective(60.0f, fbDim.aspect(), 0.1f, 10000.0f);
  }

  void setupGeometry() {
    planePosBuffer.setData(UnitPlane::vertices,
                           sizeof(UnitPlane::vertices) / sizeof(UnitPlane::vertices[0]),
                           3, GL_STATIC_DRAW);
    planeArray.connectVertexAttrib(planePosBuffer, pGBuffer, "vertexPosition", 3);
    planeNormalBuffer.setData(UnitPlane::normals,
                              sizeof(UnitPlane::normals) / sizeof(UnitPlane::normals[0]),
                              3, GL_STATIC_DRAW);
    planeArray.connectVertexAttrib(planeNormalBuffer, pGBuffer, "vertexNormal", 3);
    planeTangBuffer.setData(UnitPlane::tangents,
                            sizeof(UnitPlane::tangents) / sizeof(UnitPlane::tangents[0]),
                            3, GL_STATIC_DRAW);
    planeArray.connectVertexAttrib(planeTangBuffer, pGBuffer, "vertexTangent", 3);
    planeBinBuffer.setData(UnitPlane::binormals,
                           sizeof(UnitPlane::binormals) / sizeof(UnitPlane::binormals[0]),
                           3, GL_STATIC_DRAW);
    planeArray.connectVertexAttrib(planeBinBuffer, pGBuffer, "vertexBinormal", 3);
    planeTexCoordBuffer.setData(UnitPlane::texCoords,
                                sizeof(UnitPlane::texCoords) / sizeof(UnitPlane::texCoords[0]),
                                2, GL_STATIC_DRAW);
    planeArray.connectVertexAttrib(planeTexCoordBuffer, pGBuffer, "vertexTexCoords", 2);

    teapotPosBuffer.setData(Teapot::vertices,
                            sizeof(Teapot::vertices) / sizeof(Teapot::vertices[0]),
                            3, GL_STATIC_DRAW);
    teapotArray.connectVertexAttrib(teapotPosBuffer, pGBuffer, "vertexPosition", 3);
    teapotNormalBuffer.setData(Teapot::normals,
                               sizeof(Teapot::normals) / sizeof(Teapot::normals[0]),
                               3, GL_STATIC_DRAW);
    teapotArray.connectVertexAttrib(teapotNormalBuffer, pGBuffer, "vertexNormal", 3);
    teapotTangBuffer.setData(Teapot::tangents,
                             sizeof(Teapot::tangents) / sizeof(Teapot::tangents[0]),
                             3, GL_STATIC_DRAW);
    teapotArray.connectVertexAttrib(teapotTangBuffer, pGBuffer, "vertexTangent", 3);
    teapotBinBuffer.setData(Teapot::binormals,
                            sizeof(Teapot::binormals) / sizeof(Teapot::binormals[0]),
                            3, GL_STATIC_DRAW);
    teapotArray.connectVertexAttrib(teapotBinBuffer, pGBuffer, "vertexBinormal", 3);
    teapotTexCoordBuffer.setData(Teapot::texCoords,
                                 sizeof(Teapot::texCoords) / sizeof(Teapot::texCoords[0]),
                                 3, GL_STATIC_DRAW);
    teapotArray.connectVertexAttrib(teapotTexCoordBuffer, pGBuffer, "vertexTexCoords", 3);
    teapotIndexBuffer.setData(Teapot::indices, sizeof(Teapot::indices) / sizeof(Teapot::indices[0]));
  }

  virtual void keyboard(int key, int scancode, int action, int mods) override {
    if (key == GLENV_KEY_LEFT_CONTROL) controlDown = action == GLENV_PRESS;

    if (action == GLENV_PRESS) {
      switch (key) {
        case GLENV_KEY_ESCAPE:
          closeWindow();
          break;
        case GLENV_KEY_SPACE:
          setAnimation(!getAnimation());
          break;
        case GLENV_KEY_D:
          showDebugBuffers = !showDebugBuffers;
          showAmbientOcclusionBuffer = false;
          break;
        case GLENV_KEY_A:
          showAmbientOcclusionBuffer = !showAmbientOcclusionBuffer;
          showDebugBuffers = false;
          break;
        case GLENV_KEY_R:
          resetAnimation();
          viewPosition = Vec3{0, 0, 145};
          viewRotation = Vec3{-38, 0, 0};
          showDebugBuffers = false;
          showAmbientOcclusionBuffer = false;
          break;
      }
    }
  }

  virtual void mouseMove(double xPosition, double yPosition) override {
    if (cameraActive) {
      if (firstCameraUpdate) {
        mouse[0] = float(xPosition);
        mouse[1] = float(yPosition);
        firstCameraUpdate = false;
      }

      if (leftMouseDown) {
        viewRotation[0] += (mouse[1] - float(yPosition)) * mouseSensitivity;
        viewRotation[1] += (mouse[0] - float(xPosition)) * mouseSensitivity;
      } else if (rightMouseDown) {
        const float f = 0.6f;
        if (!controlDown) {
          viewPosition[0] -= (mouse[0] - float(xPosition)) * mouseSensitivity * f;
          viewPosition[1] += (mouse[1] - float(yPosition)) * mouseSensitivity * f;
        } else {
          viewPosition[2] -= (mouse[1] - float(yPosition)) * mouseSensitivity * f;
        }
      }
      mouse[0] = float(xPosition);
      mouse[1] = float(yPosition);
    }
  }

  virtual void mouseButton(int button, int action, int mods, double xPosition, double yPosition) override {
    if (button == GLENV_MOUSE_BUTTON_RIGHT) rightMouseDown = action == GLENV_MOUSE_PRESS;
    if (button == GLENV_MOUSE_BUTTON_LEFT) leftMouseDown = action == GLENV_MOUSE_PRESS;

    if ((button == GLENV_MOUSE_BUTTON_LEFT ||
         button == GLENV_MOUSE_BUTTON_RIGHT) && action == GLENV_MOUSE_PRESS) {
      mouse[0] = static_cast<float>(xPosition);
      mouse[1] = static_cast<float>(yPosition);
      cameraActive = true;
      firstCameraUpdate = true;
    } else if ((button == GLENV_MOUSE_BUTTON_LEFT ||
                button == GLENV_MOUSE_BUTTON_RIGHT) && action == GLENV_MOUSE_RELEASE) {
      cameraActive = false;
      firstCameraUpdate = false;
    }
  }

  virtual void mouseWheel(double xOffset, double yOffset, double xPosition, double yPosition) override {
    const float f = viewPosition[2] / mousewheelFactor;
    viewPosition[0] -= float(xOffset) * f;
    viewPosition[2] -= float(yOffset) * f;
  }
};

int main(int argc, char** argv) {
  MyGLApp myApp;
  myApp.run();
  return EXIT_SUCCESS;
}
