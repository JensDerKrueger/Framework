in vec3 vertexPosition; 

uniform mat4 MVP;

out vec3 texCoordsInterpolated;

void main() {
	gl_Position = MVP * vec4(vertexPosition, 1.0);
	texCoordsInterpolated = vec3(vertexPosition.x, vertexPosition.y, vertexPosition.z);
}
