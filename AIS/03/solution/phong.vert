in vec3 vertexPosition;  // vertex position in object/model space
in vec3 vertexNormal;    // vertex normal in object/model space

uniform mat4 MVP; // projection-model-view Matrix
uniform mat4 MV;
uniform mat4 MVit; // transpose(inverse(modelView))

out vec4 C;
out vec3 posViewSpace;
out vec3 normalViewSpace;

void main()
{
  gl_Position = MVP * vec4(vertexPosition, 1);
  posViewSpace = vec3(MV * vec4(vertexPosition, 1));
  normalViewSpace = normalize((MVit * vec4(vertexNormal, 0)).xyz);
}
