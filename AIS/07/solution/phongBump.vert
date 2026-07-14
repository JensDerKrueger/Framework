in vec3 vertexPosition;
in vec3 vertexNormal;
in vec3 vertexTangent;
in vec3 vertexBinormal;
in vec2 vertexTexCoords;

uniform mat4 MVP; // model-view-projection Matrix
uniform mat4 M; // model matrix
uniform mat4 Mit; // model inverse transpose Matrix
uniform mat4 worldToShadow;

out vec3 posWorldSpaceInterpolated;
out vec3 normalWorldSpaceInterpolated;
out vec3 tangentWorldSpaceInterpolated;
out vec3 binormWorldSpaceInterpolated;
out vec2 texCoordsInterpolated;
out vec4 shadowPos;

void main() {
  gl_Position = MVP * vec4(vertexPosition, 1.0);
  posWorldSpaceInterpolated    = vec3(M * vec4(vertexPosition, 1.0));
  
  normalWorldSpaceInterpolated = normalize((Mit * vec4(vertexNormal, 0.0)).xyz);
  tangentWorldSpaceInterpolated = normalize((Mit * vec4(vertexTangent, 0.0)).xyz);;
  binormWorldSpaceInterpolated = normalize((Mit * vec4(vertexBinormal, 0.0)).xyz);;
  texCoordsInterpolated = vertexTexCoords;
  shadowPos = worldToShadow * M * vec4(vertexPosition, 1.0);
}
