in vec3 entryPoint;
out vec4 result;

uniform sampler3D volume;
uniform sampler1D transfer;

uniform float oversampling;
uniform vec3 cameraPosInTextureSpace;
uniform vec3 minBounds;
uniform vec3 maxBounds;
uniform vec3 voxelCount;

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

vec4 tracePrimaryRay(vec3 rayStart, vec3 rayDirection) {
  float rayLength = distanceToVolumeExit(rayStart, rayDirection);
  float sampleEstimate = dot(abs(rayDirection * rayLength), voxelCount);
  int sampleCount = max(1, int(ceil(sampleEstimate * oversampling)));
  float stepLength = rayLength / float(sampleCount);
  float opacityCorrection = 100.0 * stepLength;
  vec3 delta = rayDirection * stepLength;

  vec3 currentPoint = rayStart;
  vec4 accumulated = vec4(0.0);
  for (int i = 0; i < sampleCount; ++i) {
    float volumeValue = texture(volume, currentPoint).r;
    vec4 current = transferFunction(volumeValue);
    current.a = 1.0 - pow(1.0 - current.a, opacityCorrection);
    accumulated = under(current, accumulated);
    currentPoint += delta;
    if (accumulated.a > 0.95) break;
  }

  return accumulated;
}

void main() {
  vec3 rayDirection = normalize(entryPoint-cameraPosInTextureSpace);
  result = tracePrimaryRay(entryPoint, rayDirection);
}
