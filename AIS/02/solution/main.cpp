#include <fstream>

#include <GLApp.h>

class MyGLApp : public GLApp {
private:
  const float degreePreSecond{ 45.0f };
  float angle{0.0f};

  Mat4 projection{};
  GLuint program{};
  GLint modelViewMatrixUniform{-1};
  GLint projectionMatrixUniform{-1};

  GLuint vbos[2]{ };
  GLuint vaos[3]{ };

  constexpr static float sqrt3{ 1.7320508076f };

  constexpr static GLfloat vertexPositions[] = {
    0.0f, sqrt3, 0.0f,
    -1.0f,  0.0f, 0.0f,
    1.0f,  0.0f, 0.0f
  };

  constexpr static GLfloat vertexColors[] = {
    1.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 1.0f,
    1.0f, 1.0f, 0.0f,

    1.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 1.0f,
    0.0f, 1.0f, 0.0f,

    1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 1.0f
  };

  public:
  MyGLApp() : GLApp(800,600,4,"Solution 02 - Triforce") {}

  virtual void init() override {
    setupShaders();
    setupGeometry();
  }

  virtual void animate(double animationTime) override {
    angle = degreePreSecond * animationTime;
  }

  virtual void draw() override {
    // render upper triangle
    GL(glUseProgram(program));
    Mat4 modelView = Mat4::translation(0, sqrt3 / 2, 0);
    modelView = modelView * Mat4::rotationX(angle);
    modelView = modelView * Mat4::translation(0.0f, -sqrt3 / 2, 0.0f);
    GL(glUniformMatrix4fv(modelViewMatrixUniform, 1, GL_TRUE, modelView));
    GL(glBindVertexArray(vaos[0]));
    GL(glDrawArrays(GL_TRIANGLES, 0, sizeof(vertexPositions) / sizeof(vertexPositions[0]) / 3));

    // render right triangle
    modelView = Mat4::translation(1, -sqrt3, 0);
    modelView = modelView * Mat4::rotationY(angle);
    GL(glUniformMatrix4fv(modelViewMatrixUniform, 1, GL_TRUE, modelView));
    GL(glBindVertexArray(vaos[1]));
    GL(glDrawArrays(GL_TRIANGLES, 0, sizeof(vertexPositions) / sizeof(vertexPositions[0]) / 3));

    // render left triangle
    modelView = Mat4::translation(-1, -sqrt3, 0);
    modelView = modelView * Mat4::translation(0, sqrt3 / 3, 0);
    modelView = modelView * Mat4::rotationZ(-angle);
    modelView = modelView * Mat4::translation(0, -sqrt3 / 3, 0);
    GL(glUniformMatrix4fv(modelViewMatrixUniform, 1, GL_TRUE, modelView));
    GL(glBindVertexArray(vaos[2]));
    GL(glDrawArrays(GL_TRIANGLES, 0, sizeof(vertexPositions) / sizeof(vertexPositions[0]) / 3));

    GL(glUseProgram(0));
  }

  virtual void resize(const Dimensions winDim, const Dimensions fbDim) override {
    GLApp::resize(winDim, fbDim);

    const float ratio = fbDim.aspect();
    projection = Mat4::ortho(-ratio * sqrt3, ratio * sqrt3, -sqrt3, sqrt3, -10.0f, 10.0f);
    GL(glUseProgram(program));
    GL(glUniformMatrix4fv(projectionMatrixUniform, 1, GL_TRUE, projection));
    GL(glUseProgram(0));
  }

  std::string loadFile(const std::string& filename) {
    std::ifstream shaderFile{ filename };
    if (!shaderFile) {
      throw GLException{ std::string("Unable to open file ") + filename };
    }
    std::string str;
    std::string fileContents;
    while (std::getline(shaderFile, str)) {
      fileContents += str + "\n";
    }
    return fileContents;
  }

  GLuint createShaderFromFile(GLenum type, const std::string& sourcePath) {
    const std::string shaderCode = loadFile(sourcePath);
    const std::string fullSource = GLProgram::getShaderPreamble() + shaderCode;
    const GLchar* c_shaderCode = fullSource.c_str();
    const GLuint s = glCreateShader(type);
    GL(glShaderSource(s, 1, &c_shaderCode, NULL));
    GL(glCompileShader(s)); checkAndThrowShader(s);
    return s;
  }

  void setupShaders() {
    const std::string vertexSrcPath = "vertexShader.vert";
    const std::string fragmentSrcPath = "fragmentShader.frag";
    const GLuint vertexShader = createShaderFromFile(GL_VERTEX_SHADER, vertexSrcPath);
    const GLuint fragmentShader = createShaderFromFile(GL_FRAGMENT_SHADER, fragmentSrcPath);

    program = glCreateProgram();
    GL(glAttachShader(program, vertexShader));
    GL(glAttachShader(program, fragmentShader));
    GL(glLinkProgram(program));
    checkAndThrowProgram(program);

    GL(glUseProgram(program));
    modelViewMatrixUniform = glGetUniformLocation(program, "modelViewMatrix");
    projectionMatrixUniform = glGetUniformLocation(program, "projectionMatrix");
    GL(glUseProgram(0));
  }

  void setupGeometry() {
    const GLint vertexPositionLocation = glGetAttribLocation(program, "vertexPosition");
    const GLint vertexColorLocation = glGetAttribLocation(program, "vertexColor");

    GL(glGenBuffers(2, vbos));
    GL(glGenVertexArrays(3, vaos));

    // create position VBO
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[0]));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertexPositions), vertexPositions, GL_STATIC_DRAW));

    // create color VBO
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[1]));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(vertexColors), vertexColors, GL_STATIC_DRAW));

    // define VAO for upper triangle
    GL(glBindVertexArray(vaos[0]));
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[0]));
    GL(glVertexAttribPointer(vertexPositionLocation, 3, GL_FLOAT, GL_FALSE, 0,
                             (void*)0));
    GL(glEnableVertexAttribArray(vertexPositionLocation));
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[1]));
    GL(glVertexAttribPointer(vertexColorLocation, 3, GL_FLOAT, GL_FALSE, 0,
                             (void*)0));
    GL(glEnableVertexAttribArray(vertexColorLocation));

    // define VAO for right triangle
    GL(glBindVertexArray(vaos[1]));
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[0]));
    GL(glVertexAttribPointer(vertexPositionLocation, 3, GL_FLOAT, GL_FALSE, 0,
                             (void*)0));
    GL(glEnableVertexAttribArray(vertexPositionLocation));
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[1]));
    GL(glVertexAttribPointer(vertexColorLocation, 3, GL_FLOAT, GL_FALSE, 0,
                             (void*)(9 * sizeof(float)) ));
    GL(glEnableVertexAttribArray(vertexColorLocation));

    // define VAO for left triangle
    GL(glBindVertexArray(vaos[2]));
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[0]));
    GL(glVertexAttribPointer(vertexPositionLocation, 3, GL_FLOAT, GL_FALSE, 0,
                             (void*)0));
    GL(glEnableVertexAttribArray(vertexPositionLocation));
    GL(glBindBuffer(GL_ARRAY_BUFFER, vbos[1]));
    GL(glVertexAttribPointer(vertexColorLocation, 3, GL_FLOAT, GL_FALSE, 0,
                             (void*)(18 * sizeof(float))));
    GL(glEnableVertexAttribArray(vertexColorLocation));

    GL(glBindVertexArray(0));
  }

  virtual void keyboard(int key, int scancode, int action, int mods) override {
    if (key == GLENV_KEY_ESCAPE && action == GLENV_PRESS)
      closeWindow();

    if (key == GLENV_KEY_SPACE && action == GLENV_PRESS)
      setAnimation(!getAnimation());

    if (key == GLENV_KEY_R && action == GLENV_PRESS)
      resetAnimation();
  }
};

int main(int argc, char** argv) {
  MyGLApp myApp;
  myApp.run();
  return EXIT_SUCCESS;
}
