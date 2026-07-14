#include <ImageLoader.h>
#include <GLApp.h>
#include <Vec2.h>
#include <GLFramebuffer.h>

#include "Teapot.h"
#include "UnitPlane.h"

class LightProperties {
public:
  float degreesPerSecond{45.0f};
  float angle{0};
};

class MyGLApp : public GLApp {
public:
  LightProperties light;
  Mat4 projectionMatrix;

  GLTexture2D stonesDiffuse{GL_LINEAR, GL_LINEAR};
  GLTexture2D stonesSpecular{GL_LINEAR, GL_LINEAR};
  GLTexture2D stonesNormals{GL_LINEAR, GL_LINEAR};
  GLTexture2D udeNormals{GL_LINEAR, GL_LINEAR};

  GLTexture2D diffuseBuffer{GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D specularBuffer{GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D normalBuffer{GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLTexture2D positionBuffer{GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE};
  GLDepthBuffer depthBuffer;
  GLFramebuffer gBuffer;
  Dimensions gBufferSize{0, 0};

  GLProgram pGBuffer;
  GLProgram pDeferred;

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

  GLArray fullscreenArray;

  bool showDebugBuffers{false};
  bool leftMouseDown{false};
  bool rightMouseDown{false};
  bool controlDown{false};

  bool cameraActive{false};
  bool firstCameraUpdate{true};
  Vec3 viewPosition = {0, 0, 100};
  Vec3 viewRotation = {-45, 0, 0};
  Vec2 mouse = {0, 0};
  float mouseSensitivity{0.15f};
  float mousewheelFactor{10.0f};

  Mat4 viewMatrix;
  Mat4 lightModelMatrix;
  Vec4 lightPosition;

  MyGLApp() :
  GLApp(800, 600, 1, "Solution 12 - Deferred Shading"),
  pGBuffer{GLProgram::createFromFile("gbuffer.vert", "gbuffer.frag", "", false, true)},
  pDeferred{GLProgram::createFromFile("deferred.vert", "deferred.frag", "", false, true)}
  {
  }

  virtual void init() override {
    setupTextures();
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

  void renderSceneToGBuffer() {
    pGBuffer.enable();

    Mat4 modelMatrix = Mat4::scaling(100, 100, 100);
    setGBufferMatrices(modelMatrix);
    setGBufferMaterial({1, 1, 1}, {1, 1, 1}, 50.0f, 8.0f, 1, 1, 1);
    pGBuffer.setTexture("diffuseTexture", stonesDiffuse, 0);
    pGBuffer.setTexture("specularTexture", stonesSpecular, 1);
    pGBuffer.setTexture("normalTexture", stonesNormals, 2);
    planeArray.bind();
    GL(glDrawArrays(GL_TRIANGLES, 0, sizeof(UnitPlane::vertices) / (3 * sizeof(UnitPlane::vertices[0]))));

    modelMatrix = {};
    setGBufferMatrices(modelMatrix);
    setGBufferMaterial({0.10f, 0.28f, 0.85f}, {1.0f, 1.0f, 1.0f}, 80.0f, 1.0f, 0, 0, 1);
    pGBuffer.setTexture("normalTexture", udeNormals, 2);
    teapotArray.bind();
    GL(glDrawElements(GL_TRIANGLES, sizeof(Teapot::indices) / sizeof(Teapot::indices[0]), GL_UNSIGNED_INT, (void*)0));
  }

  void renderDeferredImage() {
    GL(glDisable(GL_DEPTH_TEST));
    pDeferred.enable();
    pDeferred.setUniform("showDebugBuffers", showDebugBuffers ? 1 : 0);
    pDeferred.setUniform("lightPosition", lightPosition);
    pDeferred.setTexture("diffuseBuffer", diffuseBuffer, 0);
    pDeferred.setTexture("specularBuffer", specularBuffer, 1);
    pDeferred.setTexture("normalBuffer", normalBuffer, 2);
    pDeferred.setTexture("positionBuffer", positionBuffer, 3);
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
          break;
        case GLENV_KEY_R:
          resetAnimation();
          viewPosition = Vec3{0, 0, 100};
          viewRotation = Vec3{-45, 0, 0};
          showDebugBuffers = false;
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
