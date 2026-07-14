#include <GLEnv.h>
#include <Mat4.h>
#include <GLAppKeyTranslation.h>
#include <GLProgram.h>


#include <iostream>
#include <sstream>

GLuint vbo[2];
GLuint vao[2];
GLuint orangeProgram;
GLuint greenProgram;

constexpr float triangle1[] = {
  -0.5f,  0.5f, 0.0f, // top left
  0.5f,  0.5f, 0.0f,  // top right
  0.5f, -0.5f, 0.0f  // bottom right
};

constexpr float triangle2[] = {
  -0.5f,  0.5f, 0.0f, // top left
  0.5f, -0.5f, 0.0f, // bottom right
  -0.5f, -0.5f, 0.0f  // bottom left
};

const GLchar* vertexShaderSource{
R"(in vec3 vPos;
void main()
{
  gl_Position = vec4(vPos, 1.0);
}
)"
};

const GLchar* orangeShaderSource{
R"(out vec4 fragColor;
void main()
{
  fragColor = vec4(0.9f, 0.5f, 0.2f, 1.0);
}
)"
};

const GLchar* greenShaderSource{
R"(
out vec4 fragColor;
void main()
{
  fragColor = vec4(0.2f, 0.9f, 0.2f, 1.0);
}
)"
};

static void draw(void* arg=nullptr) {
  GL( glClearColor(0.0f, 0.0f, 0.0f, 1.0f) );
  GL( glClear(GL_COLOR_BUFFER_BIT) );

  GL( glBindVertexArray(vao[0]) );
  GL( glUseProgram(orangeProgram) );
  GL( glDrawArrays(GL_TRIANGLES, 0, 3) );

  GL( glBindVertexArray(vao[1]) );
  GL( glUseProgram(greenProgram) );
  GL( glDrawArrays(GL_TRIANGLES, 0, 3) );
  GL( glBindVertexArray(0) );
}

static void setupShaders() {
  // create the vertex shader
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  const std::string fullSourceV = GLProgram::getShaderPreamble() + vertexShaderSource;
  const GLchar* c_shaderCodeV = fullSourceV.c_str();
  GL( glShaderSource(vertexShader, 1, &c_shaderCodeV, NULL) );
  GL( glCompileShader(vertexShader) );
  checkAndThrowShader(vertexShader);

  // create the orange fragment shader
  GLuint fragmentShaderOrange = glCreateShader(GL_FRAGMENT_SHADER);
  const std::string fullSourceFO = GLProgram::getShaderPreamble() + orangeShaderSource;
  const GLchar* c_shaderCodeFO = fullSourceFO.c_str();
  GL( glShaderSource(fragmentShaderOrange, 1, &c_shaderCodeFO, NULL) );
  GL( glCompileShader(fragmentShaderOrange) );
  checkAndThrowShader(fragmentShaderOrange);

  // create the green fragment shader
  GLuint fragmentShaderGreen = glCreateShader(GL_FRAGMENT_SHADER);
  const std::string fullSourceFG = GLProgram::getShaderPreamble() + greenShaderSource;
  const GLchar* c_shaderCodeFG = fullSourceFG.c_str();
  GL( glShaderSource(fragmentShaderGreen, 1, &c_shaderCodeFG, NULL) );
  GL( glCompileShader(fragmentShaderGreen) );
  checkAndThrowShader(fragmentShaderGreen);

  // link shaders into programs
  orangeProgram = glCreateProgram();
  GL( glAttachShader(orangeProgram, vertexShader) );
  GL( glAttachShader(orangeProgram, fragmentShaderOrange) );
  GL( glLinkProgram(orangeProgram) );
  checkAndThrowProgram(orangeProgram);

  greenProgram = glCreateProgram();
  GL( glAttachShader(greenProgram, vertexShader) );
  GL( glAttachShader(greenProgram, fragmentShaderGreen) );
  GL( glLinkProgram(greenProgram) );
  checkAndThrowProgram(greenProgram);

  GL( glDeleteShader(vertexShader) );
  GL( glDeleteShader(fragmentShaderOrange) );
  GL( glDeleteShader(fragmentShaderGreen) );
}

static void setupGeometry() {
  GL( glGenBuffers(2, vbo) );

  // define VAO for triangle
  GL( glGenVertexArrays(2, vao) );
  GL( glBindVertexArray(vao[0]) );

  // upload vertex positions to VBO
  GL( glBindBuffer(GL_ARRAY_BUFFER, vbo[0]) );
  GL( glBufferData(GL_ARRAY_BUFFER, sizeof(triangle1), triangle1, GL_STATIC_DRAW) );

  GL( glEnableVertexAttribArray(0) );
  GL( glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0) );

  GL( glBindVertexArray(vao[1]) );
  GL( glBindBuffer(GL_ARRAY_BUFFER, vbo[1]) );
  GL( glBufferData(GL_ARRAY_BUFFER, sizeof(triangle2), triangle2, GL_STATIC_DRAW) );

  GL( glEnableVertexAttribArray(0) );
  GL( glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0) );

  GL( glBindVertexArray(0) );
}


#ifndef __EMSCRIPTEN__
static void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                        int mods) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}

static void sizeCallback(GLFWwindow* window, int width, int height) {
  int w, h;
  glfwGetFramebufferSize(window, &w, &h);
  GL( glViewport(0, 0, w, h) );
}

int main(int argc, char** argv) {
  GLEnv glEnv{800,600,1,"My First OpenGL Program",true,false};
  glEnv.setKeyCallback(keyCallback);
  glEnv.setResizeCallback(sizeCallback);
  setupShaders();
  setupGeometry();
  while (!glEnv.shouldClose()) {
    draw();
    glEnv.endOfFrame();
  }
  return EXIT_SUCCESS;
}
#else
int main(int argc, char** argv) {
  GLEnv glEnv{800,600,1,"My First OpenGL Program",true,false};
  setupShaders();
  setupGeometry();
  emscripten_set_main_loop_arg(draw, nullptr, 0, 1);
  while (!glEnv.shouldClose()) {
    draw();
    glEnv.endOfFrame();
  }
  return EXIT_SUCCESS;
}
#endif
