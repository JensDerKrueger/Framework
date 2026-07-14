in vec3 vertexPosition;
in vec2 texCoords;

uniform mat4 MVP;

out vec2 tc;

void main() {
  gl_Position = MVP * vec4(vertexPosition, 1.0);
  tc = texCoords;
}
