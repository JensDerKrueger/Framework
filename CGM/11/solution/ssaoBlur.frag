in vec2 texCoords;

uniform sampler2D sourceAmbientOcclusionBuffer;
uniform sampler2D positionBuffer;
uniform vec2 direction;
uniform float depthFalloff;

out vec4 color;

float gaussianWeight(vec2 offset) {
  float distanceSquared = dot(offset, offset);
  return exp(-0.5 * distanceSquared);
}

float smoothAmbientOcclusion(vec2 uv) {
  vec4 centerPosition = texture(positionBuffer, uv);
  if(centerPosition.a <= 0.0) return 1.0;

  vec2 texelSize = 1.0 / vec2(textureSize(sourceAmbientOcclusionBuffer, 0));
  float weightedAO = 0.0;
  float totalWeight = 0.0;

  // SSAO is noisy because we estimate a hemisphere integral with only a small
  // number of samples. A full 5 x 5 blur would use 25 samples per pixel. Since
  // the Gaussian part is separable, we apply this shader twice instead: first
  // horizontally and then vertically. Each pass uses only five samples.
  //
  // A normal blur would remove noise, but it would also smear dark foreground AO
  // over background pixels. The depth weight from the position buffer keeps the
  // blur edge-aware.
  for(int i = -2; i <= 2; ++i) {
    vec2 offset = direction * float(i);
    vec2 sampleUV = uv + offset * texelSize;

    vec4 samplePosition = texture(positionBuffer, sampleUV);
    if(samplePosition.a <= 0.0) continue;

    float spatialWeight = gaussianWeight(offset * 0.65);
    float depthDifference = abs(centerPosition.z - samplePosition.z);
    float depthWeight = exp(-depthDifference / depthFalloff);
    float weight = spatialWeight * depthWeight;

    weightedAO += weight * texture(sourceAmbientOcclusionBuffer, sampleUV).r;
    totalWeight += weight;
  }

  if(totalWeight <= 0.0) return texture(sourceAmbientOcclusionBuffer, uv).r;
  return weightedAO / totalWeight;
}

void main() {
  color = vec4(vec3(smoothAmbientOcclusion(texCoords)), 1.0);
}
