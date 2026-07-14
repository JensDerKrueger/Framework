in vec2 texCoords;

uniform sampler2D diffuseBuffer;
uniform sampler2D specularBuffer;
uniform sampler2D normalBuffer;
uniform sampler2D positionBuffer;
uniform sampler2D ambientOcclusionBuffer;
uniform vec4 lightPosition;
uniform int showDebugBuffers;
uniform int showAmbientOcclusionBuffer;

out vec4 color;

vec3 shadePixel(vec2 uv) {
  vec4 positionSample = texture(positionBuffer, uv);
  if(positionSample.a <= 0.0) return vec3(0.0);

  vec3 position = positionSample.xyz;
  vec3 diffuse = texture(diffuseBuffer, uv).rgb;
  // The SSAO pass writes 1 for open areas and lower values for locally occluded
  // regions. The power curve increases the visual contrast so the contact
  // shadows remain visible after lighting is applied.
  float ambientOcclusion = pow(clamp(texture(ambientOcclusionBuffer, uv).r, 0.0, 1.0), 2.2);
  vec4 specularSample = texture(specularBuffer, uv);
  vec3 specular = specularSample.rgb;
  float shininess = max(1.0, specularSample.a * 128.0);
  vec3 N = normalize(texture(normalBuffer, uv).rgb * 2.0 - vec3(1.0));

  vec3 L = normalize(lightPosition.xyz - position);
  vec3 V = normalize(-position);
  vec3 R = reflect(-L, N);

  float diffuseWeight = max(dot(N, L), 0.0);
  float specularWeight = 0.0;
  if(diffuseWeight > 0.0) {
    specularWeight = 0.35 * pow(max(dot(V, R), 0.0), shininess);
  }

  // Ambient light is the most direct use of SSAO: locally hidden regions receive
  // less indirect/environment light.
  vec3 ambient = 0.14 * diffuse * ambientOcclusion;
  // We also use the AO value as a soft contact term for direct Phong light. This
  // is not physically exact, but it makes the local darkening visible in the
  // final teaching example.
  float contactOcclusion = mix(0.25, 1.0, ambientOcclusion);
  vec3 directLight = contactOcclusion * (diffuseWeight * diffuse + specularWeight * specular);
  return ambient + directLight;
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
  if(showAmbientOcclusionBuffer != 0) {
    color = vec4(vec3(texture(ambientOcclusionBuffer, texCoords).r), 1.0);
  } else if(showDebugBuffers != 0) {
    color = vec4(showBuffer(texCoords), 1.0);
  } else {
    color = vec4(shadePixel(texCoords), 1.0);
  }
}
