in vec2 texCoords;

uniform sampler2D normalBuffer;
uniform sampler2D positionBuffer;
uniform mat4 projectionMatrix;
uniform float radius;
uniform float bias;
uniform float intensity;
uniform float maxMipLevel;
uniform int sampleCount;

out vec4 color;

float randomAngle(vec2 uv) {
  return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453) * 6.2831853;
}

vec3 sampleVector(int index) {
  const vec3 samples[24] = vec3[24](
    vec3( 0.5381,  0.1856,  0.4319), vec3( 0.1379,  0.2486,  0.4430),
    vec3( 0.3371,  0.5679,  0.0057), vec3(-0.6999, -0.0451,  0.0019),
    vec3( 0.0689, -0.1598,  0.8547), vec3( 0.0560,  0.0069,  0.1843),
    vec3(-0.0147,  0.1402,  0.0762), vec3( 0.0100, -0.1924,  0.0344),
    vec3(-0.3577, -0.5301,  0.4358), vec3(-0.3169,  0.1063,  0.0158),
    vec3( 0.0103, -0.5869,  0.0046), vec3(-0.0897, -0.4940,  0.3287),
    vec3( 0.7119, -0.0154,  0.0918), vec3(-0.0533,  0.0596,  0.5411),
    vec3( 0.0352, -0.0631,  0.5460), vec3(-0.4776,  0.2847,  0.0271),
    vec3(-0.1180, -0.3927,  0.3734), vec3(-0.3410,  0.2451,  0.1749),
    vec3( 0.1970,  0.3141,  0.5117), vec3( 0.3093, -0.1031,  0.1460),
    vec3(-0.0160,  0.1939,  0.4713), vec3( 0.1035, -0.4749,  0.3292),
    vec3( 0.2112, -0.3250,  0.0761), vec3(-0.2600, -0.2680,  0.1580)
  );

  return normalize(samples[index]);
}

vec3 rotateAroundZ(vec3 v, float angle) {
  float s = sin(angle);
  float c = cos(angle);
  return vec3(c * v.x - s * v.y, s * v.x + c * v.y, v.z);
}

mat3 hemisphereBasis(vec3 N, float angle) {
  // The hard-coded sample vectors are defined around the positive z-axis. For a
  // surface point we need the same pattern around the actual surface normal.
  // This builds a tangent/bitangent/normal basis and uses it to rotate samples
  // from local hemisphere coordinates into view-space directions.
  vec3 randomVector = vec3(cos(angle), sin(angle), 0.5);
  vec3 tangent = randomVector - N * dot(randomVector, N);
  if(dot(tangent, tangent) < 0.0001) {
    vec3 helper = abs(N.z) < 0.9 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
    tangent = normalize(cross(helper, N));
  } else {
    tangent = normalize(tangent);
  }
  vec3 bitangent = normalize(cross(N, tangent));
  return mat3(tangent, bitangent, N);
}

float estimateAmbientOcclusion(vec2 uv) {
  vec4 positionSample = texture(positionBuffer, uv);
  if(positionSample.a <= 0.0) return 1.0;

  vec3 position = positionSample.xyz;
  vec3 N = normalize(texture(normalBuffer, uv).rgb * 2.0 - vec3(1.0));
  float angle = randomAngle(uv);
  mat3 basis = hemisphereBasis(N, angle);
  float occlusion = 0.0;
  int usedSamples = 0;

  // For the current visible point we test a set of nearby positions in the
  // hemisphere above the surface. If the G-buffer contains another surface in
  // front of such a test position, that nearby surface blocks part of the local
  // ambient light and therefore contributes to occlusion.
  for(int i = 0; i < 24; ++i) {
    if(i >= sampleCount) break;

    // Small samples are more important for contact shadows; larger samples
    // probe wider cavities. The quadratic distribution places more samples near
    // the shaded point and fewer farther away.
    float scale = float(i + 1) / float(sampleCount);
    scale = mix(0.2, 1.0, scale * scale);
    vec3 direction = normalize(basis * rotateAroundZ(sampleVector(i), angle));
    float mipLevel = floor(scale * maxMipLevel);

    // Move from the current view-space point along the sample direction, then
    // project that sample position back into screen coordinates. The projected
    // coordinates tell us where to look in the position buffer.
    vec3 samplePosition = position + direction * radius * scale;
    vec4 clipPosition = projectionMatrix * vec4(samplePosition, 1.0);
    vec2 sampleUV = clipPosition.xy / clipPosition.w * 0.5 + vec2(0.5);

    if(sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
       sampleUV.y < 0.0 || sampleUV.y > 1.0) {
      continue;
    }

    vec4 blockerSample = textureLod(positionBuffer, sampleUV, mipLevel);
    if(blockerSample.a <= 0.0) continue;

    // Compare the expected sample depth with the actual depth stored in the
    // position buffer. In this view-space convention larger z values are closer
    // to the camera. If the stored surface is closer than the sample position,
    // it blocks this sample direction.
    float rangeWeight = smoothstep(0.0, 1.0, radius / abs(position.z - blockerSample.z));
    if(blockerSample.z >= samplePosition.z + bias) {
      occlusion += rangeWeight;
    }
    usedSamples++;
  }

  if(usedSamples == 0) return 1.0;
  return clamp(1.0 - intensity * occlusion / float(usedSamples), 0.0, 1.0);
}

void main() {
  float ambientOcclusion = estimateAmbientOcclusion(texCoords);
  color = vec4(vec3(ambientOcclusion), 1.0);
}
