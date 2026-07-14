in vec3 worldPosition;
in vec3 worldNormal;
in vec3 materialColor;

uniform vec3 lightPosition;
uniform vec3 viewPosition;
uniform vec3 ambientLight;
uniform vec3 diffuseLight;
uniform vec3 specularLight;
uniform float shininess;

out vec4 fragmentColor;

void main() {
  vec3 N = normalize(worldNormal);
  vec3 L = normalize(lightPosition - worldPosition);
  vec3 V = normalize(viewPosition - worldPosition);
  vec3 R = reflect(-L, N);

  vec3 ambient = ambientLight * materialColor;

  float diffuseFactor = max(dot(N, L), 0.0);
  vec3 diffuse = diffuseLight * materialColor * diffuseFactor;

  float specularFactor = pow(max(dot(V, R), 0.0), shininess);
  vec3 specular = specularLight * specularFactor;

  fragmentColor = vec4(ambient + diffuse + specular, 1.0);
}
