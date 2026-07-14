in vec3 entryPoint;
out vec4 result;

uniform sampler3D volume;
uniform sampler1D transfer;

uniform float oversampling;
uniform vec3 cameraPosInTextureSpace;
uniform vec3 minBounds;
uniform vec3 maxBounds;
uniform vec3 voxelCount;
uniform vec3 lightDirectionInTextureSpace;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform float lightIntensity;
uniform float shadowDensityScale;
uniform float shadowAlphaThreshold;
uniform int shadowStepCount;

vec4 transferFunction(float v) {
  return texture(transfer, v);
}

vec4 under(vec4 current, vec4 last) {
  last.rgb = last.rgb + (1.0-last.a) * current.a * current.rgb;
  last.a   = last.a + (1.0-last.a) * current.a;
  return last;
}

float distanceToVolumeExit(vec3 rayStart, vec3 rayDirection) {
  float farAway = 1.0e20;
  float xDistance = rayDirection.x > 0.0
    ? (maxBounds.x - rayStart.x) / rayDirection.x
    : rayDirection.x < 0.0 ? (minBounds.x - rayStart.x) / rayDirection.x : farAway;
  float yDistance = rayDirection.y > 0.0
    ? (maxBounds.y - rayStart.y) / rayDirection.y
    : rayDirection.y < 0.0 ? (minBounds.y - rayStart.y) / rayDirection.y : farAway;
  float zDistance = rayDirection.z > 0.0
    ? (maxBounds.z - rayStart.z) / rayDirection.z
    : rayDirection.z < 0.0 ? (minBounds.z - rayStart.z) / rayDirection.z : farAway;

  return max(min(xDistance, min(yDistance, zDistance)), 0.0);
}

float traceShadowRay(vec3 rayStart, vec3 rayDirection) {
  int sampleCount = max(shadowStepCount, 1);
  float rayLength = distanceToVolumeExit(rayStart, rayDirection);
  float stepLength = rayLength / float(sampleCount);
  vec3 delta = rayDirection * stepLength;
  vec3 currentPoint = rayStart + delta * 0.5;
  float opticalDepth = 0.0;

  for (int i = 0; i < sampleCount; ++i) {
    float volumeValue = texture(volume, currentPoint).r;
    float shadowDensity = transferFunction(volumeValue).a;
    opticalDepth += shadowDensity * stepLength;
    currentPoint += delta;
  }

  return exp(-shadowDensityScale * opticalDepth);
}

vec3 estimateLighting(vec3 position, float alpha) {
  vec3 lightDirection = normalize(lightDirectionInTextureSpace);
  float shadowTransmittance = alpha > shadowAlphaThreshold
    ? traceShadowRay(position, lightDirection)
    : 1.0;
  return ambientColor + lightColor * lightIntensity * shadowTransmittance;
}

void main() {
  vec3 rayDirection = normalize(entryPoint-cameraPosInTextureSpace);

  float rayLength = distanceToVolumeExit(entryPoint, rayDirection);
  float sampleEstimate = dot(abs(rayDirection * rayLength), voxelCount);
  int sampleCount = max(1, int(ceil(sampleEstimate * oversampling)));
  float stepLength = rayLength / float(sampleCount);
  float opacityCorrection = 100.0 * stepLength;
  vec3 delta = rayDirection * stepLength;

  vec3 currentPoint = entryPoint;
  vec4 accumulated = vec4(0.0);
  for (int i = 0; i < sampleCount; ++i) {
    float volumeValue = texture(volume, currentPoint).r;
    vec4 current = transferFunction(volumeValue);
    current.rgb *= estimateLighting(currentPoint, current.a);
    current.a = 1.0 - pow(1.0 - current.a, opacityCorrection);
    accumulated = under(current, accumulated);
    currentPoint += delta;
    if (accumulated.a > 0.95) break;
  }

  result = accumulated;
}
