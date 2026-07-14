in vec2 texCoords;

uniform sampler2D diffuseBuffer;
uniform sampler2D specularBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D positionBuffer;
uniform sampler2DShadow shadowMap;
uniform vec4 lightPosition;
uniform mat4 viewToShadow;
uniform int showDebugBuffers;

out vec4 color;

float shadowVisibility(vec3 position, vec3 N, vec3 L) {
  vec4 shadowCoord = viewToShadow * vec4(position, 1.0);
  vec3 projected = shadowCoord.xyz / shadowCoord.w;

  if(projected.x < 0.0 || projected.x > 1.0 ||
     projected.y < 0.0 || projected.y > 1.0 ||
     projected.z < 0.0 || projected.z > 1.0) {
    return 1.0;
  }

  float bias = 0.0005;
  vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
  float visibility = 0.0;

  for(int y = -1; y <= 1; ++y) {
    for(int x = -1; x <= 1; ++x) {
      vec2 offset = vec2(x, y) * texelSize;
      visibility += texture(shadowMap, vec3(projected.xy + offset, projected.z - bias));
    }
  }

  return visibility / 9.0;
}

vec3 shadePixel(vec2 uv) {
  vec4 positionSample = texture(positionBuffer, uv);
  if(positionSample.a <= 0.0) return vec3(0.0);

  vec3 position = positionSample.xyz;
  vec3 diffuse = texture(diffuseBuffer, uv).rgb;
  vec4 specularSample = texture(specularBuffer, uv);
  vec3 specular = specularSample.rgb;
  float shininess = max(1.0, specularSample.a * 128.0);
  vec3 N = normalize(texture(normalBuffer, uv).rgb * 2.0 - vec3(1.0));

  vec3 L = normalize(lightPosition.xyz - position);
  vec3 V = normalize(-position);
  vec3 R = reflect(-L, N);

  float diffuseWeight = max(dot(N, L), 0.0);
  float specularWeight = 0.0;
  float visibility = 1.0;
  if(diffuseWeight > 0.0) {
    visibility = shadowVisibility(position, N, L);
    specularWeight = pow(max(dot(V, R), 0.0), shininess);
  }

  vec3 ambient = 0.08 * diffuse;
  return ambient + visibility * (diffuseWeight * diffuse + specularWeight * specular);
}

vec3 showBuffer(vec2 uv) {
  bool right = texCoords.x >= 0.5;
  bool top = texCoords.y >= 0.5;
  vec2 localUV = fract(texCoords * 2.0);

  if(!right && top) {
    return texture(diffuseBuffer, localUV).rgb;
  }

  if(right && top) {
    return texture(specularBuffer, localUV).rgb;
  }

  if(!right && !top) {
    return texture(normalBuffer, localUV).rgb;
  }

  vec3 position = texture(positionBuffer, localUV).xyz;
  return vec3(position.xy * 0.01 + vec2(0.5), clamp(-position.z / 160.0, 0.0, 1.0));
}

void main() {
  if(showDebugBuffers != 0) {
    color = vec4(showBuffer(texCoords), 1.0);
  } else {
    color = vec4(shadePixel(texCoords), 1.0);
  }
}
