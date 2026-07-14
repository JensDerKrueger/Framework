#include <GLApp.h>
#include <OBJFile.h>
#include <Mat4.h>

class MyGLApp : public GLApp {
public:
  double angle{0.0};
  bool bCube{false};
  std::vector<float> data;
  std::vector<float> data_cube;

  MyGLApp() : GLApp{640,480,1,"Shared vertices to explicit representation demo"} {}

  virtual void init() override {
    GL(glDisable(GL_CULL_FACE));
    GL(glEnable(GL_DEPTH_TEST));
    data = loadOBJ("bunny.obj");
    data_cube = loadOBJ("cube.obj");
  }

  virtual void animate(double animationTime) override{
    angle = animationTime*30;
  }

  virtual void draw() override {
    setDrawProjection(Mat4::perspective(45, glEnv.getFramebufferSize().aspect(), 0.0001f, 100.0f));
    setDrawTransform(Mat4::lookAt({0,0,2},{0,0,0},{0,1,0}) * Mat4::rotationY(float(angle)));
    drawTriangles(bCube ? data_cube : data, TrisDrawType::LIST, false, true);
  }

  virtual void keyboardChar(unsigned int key) override {
    if (key == GLENV_KEY_SPACE) bCube = !bCube;
  }

private:
  std::vector<float> loadOBJ(const std::string& filename) {
    std::vector<float> target;
    const OBJFile m{filename, true};
    for (const OBJFile::IndexType& triangle : m.indices) {
      for (const size_t& index : triangle) {
        target.push_back(m.vertices[index].x);
        target.push_back(m.vertices[index].y);
        target.push_back(m.vertices[index].z);

        // use position also as color
        target.push_back(m.vertices[index].x+0.5f);
        target.push_back(m.vertices[index].y+0.5f);
        target.push_back(m.vertices[index].z+0.5f);
        target.push_back(1.0f);

        target.push_back(m.normals[index].x);
        target.push_back(m.normals[index].y);
        target.push_back(m.normals[index].z);
      }
    }
    return target;
  }

} myApp;

int main(int argc, char ** argv) {
  myApp.run();
  return EXIT_SUCCESS;
}
