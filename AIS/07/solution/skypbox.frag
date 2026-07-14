in vec3 texCoordsInterpolated;

uniform samplerCube skybox;

out vec4 color;

void main() {
  color = texture(skybox, texCoordsInterpolated);
}
