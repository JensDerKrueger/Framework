#define _USE_MATH_DEFINES
#include "PhotonMapper.h"
#include "PointLight.h"
#include "RectAreaLight.h"
#include <algorithm>
#include <cmath>
#include <memory>

namespace {

constexpr float PHOTON_EPSILON = 0.0001f;
constexpr float PHOTON_GATHER_RADIUS = 0.35f;
constexpr float PHOTON_GRID_CELL_SIZE = PHOTON_GATHER_RADIUS * 0.5f;
constexpr float PHOTON_MIN_NORMAL_DOT = 0.05f;
constexpr float PHOTON_RADIANCE_SCALE = 45.0f;
constexpr float PHOTON_VISUALIZATION_SCALE = 20000.0f;
constexpr float PHOTON_MIN_POWER_SQ = 0.000000000001f;
constexpr int PHOTON_GRID_BIAS = 1 << 20;

Vec3 offsetRayOrigin(const Vec3& position, const Vec3& direction, const Vec3& normal)
{
    const float side = Vec3::dot(direction, normal) > 0.0f ? 1.0f : -1.0f;
    return position + normal * (PHOTON_EPSILON * side);
}

Vec3 transformDirection(const Mat4& matrix, const Vec3& direction)
{
    return (matrix * Vec4{direction, 0.0f}).xyz;
}

}

void PhotonMapper::setCamera(const Camera& camera)
{
    this->camera = camera;
}

void PhotonMapper::setScene(const Scene& scene)
{
    this->scene = scene;
}

void PhotonMapper::render(Image& img)
{
    RaySetup rs = computeRaySetup(img);

    int numSamples = numSamplesX * numSamplesY;
    for (uint32_t y = 0; y < img.height; ++y)
    {
        for (uint32_t x = 0; x < img.width; ++x)
        {
            Vec3 color;
            if (numSamples == 1)
            {
                Ray r = computeRay(float(x), float(y), rs);
                color = traceRay(r);
            }
            else
            {
                for (int sY = 0; sY < numSamplesY; ++sY)
                {
                    for (int sX = 0; sX < numSamplesX; ++sX)
                    {
                        Ray r = computeRay(x + sX / ((float)numSamplesX), y + sY / ((float)numSamplesY), rs);
                        color = color + traceRay(r);
                    }
                }
                color = color / float(numSamples);
            }
            img.setNormalizedValue(x, y, 0, color.r);
            img.setNormalizedValue(x, y, 1, color.g);
            img.setNormalizedValue(x, y, 2, color.b);
            img.setValue(x, y, 3, 255);
        }
    }
}

void PhotonMapper::preparePhotonEmission(size_t photonCount)
{
    photons.clear();
    photonGrid = PhotonGrid{};
    photons.reserve(photonCount);
    targetPhotonCount = photonCount;
    initializePhotonGrid();
}

bool PhotonMapper::emitPhotons(size_t photonPathCount)
{
    if (photons.size() >= targetPhotonCount)
        return true;

    std::shared_ptr<const LightSource> light = scene.getLight(0);
    std::shared_ptr<const RectAreaLight> areaLight = std::dynamic_pointer_cast<const RectAreaLight>(light);
    std::shared_ptr<const PointLight> pointLight = std::dynamic_pointer_cast<const PointLight>(light);
    if (!areaLight && !pointLight)
        return true;

    for (size_t i = 0; i < photonPathCount && photons.size() < targetPhotonCount; ++i)
    {
        const float photonCount = float(std::max<size_t>(targetPhotonCount, 1));
        if (areaLight)
        {
            tracePhotonPath(areaLight->samplePosition(), areaLight->sampleEmissionDirection(), areaLight->getDiffuse() / photonCount);
        }
        else
        {
            tracePhotonPath(pointLight->getPosition(), Vec3::randomUnitVector(), pointLight->getDiffuse() / photonCount);
        }
    }

    return photons.size() >= targetPhotonCount;
}

void PhotonMapper::setPhotonMap(const std::vector<Photon>& photons)
{
    this->photons = photons;
    targetPhotonCount = photons.size();
    buildPhotonGrid();
}

void PhotonMapper::setPhotonMap(const std::vector<Photon>& photons, const PhotonGrid& photonGrid)
{
    this->photons = photons;
    this->photonGrid = photonGrid;
    targetPhotonCount = photons.size();
}

const std::vector<Photon>& PhotonMapper::getPhotons() const
{
    return photons;
}

const PhotonMapper::PhotonGrid& PhotonMapper::getPhotonGrid() const
{
    return photonGrid;
}

size_t PhotonMapper::getPhotonCount() const
{
    return photons.size();
}

size_t PhotonMapper::getTargetPhotonCount() const
{
    return targetPhotonCount;
}

std::vector<float> PhotonMapper::getPhotonPointData() const
{
    std::vector<float> data;
    data.reserve(photons.size() * 7);

    for (const Photon& photon : photons)
    {
        const Vec3 color = Vec3::clamp(photon.power * PHOTON_VISUALIZATION_SCALE, 0.0f, 1.0f);
        data.push_back(photon.position.x);
        data.push_back(photon.position.y);
        data.push_back(photon.position.z);
        data.push_back(color.r);
        data.push_back(color.g);
        data.push_back(color.b);
        data.push_back(1.0f);
    }

    return data;
}

Vec3 PhotonMapper::traceRay(const Ray& r) const
{
    const Mat4 inverseModel = Mat4::inverse(scene.getModel());
    const Vec3 localDirection = Vec3::normalize(transformDirection(inverseModel, r.getDirection()));
    if (localDirection.sqlength() == 0.0f)
        return scene.getBackgroundcolor();

    const Ray localRay{inverseModel * r.getOrigin(), localDirection};
    return Vec3::clamp(traceLocalRay(localRay, recDepth), 0.0f, 1.0f);
}

Vec3 PhotonMapper::traceLocalRay(const Ray& localRay, int depth) const
{
    if (depth == 0)
        return scene.getBackgroundcolor();

    const std::optional<Intersection> intersection = scene.intersect(localRay, false);
    if (!intersection)
        return scene.getBackgroundcolor();

    const Material material = intersection->getMaterial();
    if (material.emits())
        return material.getEmission();

    const Vec3 hitPosition = localRay.getPosOnRay(intersection->getT());
    const Vec3 normal = intersection->getNormal();

    Vec3 reflectionColor;
    if (material.reflects()) {
        const Vec3 reflectionDirection = Vec3::reflect(localRay.getDirection(), normal);
        const Ray reflectionRay{offsetRayOrigin(hitPosition, reflectionDirection, normal), reflectionDirection};
        reflectionColor = traceLocalRay(reflectionRay, depth - 1);
    }

    Vec3 refractionColor;
    bool totalInternalReflection = false;
    if (material.refracts()) {
        const std::optional<Vec3> refractionDirection = Vec3::refract(localRay.getDirection(), normal, *material.getIndexOfRefraction());
        if (refractionDirection) {
            const Ray refractionRay{offsetRayOrigin(hitPosition, *refractionDirection, normal), *refractionDirection};
            refractionColor = traceLocalRay(refractionRay, depth - 1);
        } else {
            totalInternalReflection = true;
        }
    }

    const Vec3 diffuseColor = getDiffuseColor(*intersection);
    Vec3 localColor;
    if (diffuseColor.sqlength() > 0.0f)
        localColor = estimatePhotonRadiance(localRay, *intersection);

    const float cosI = Vec3::dot(localRay.getDirection(), normal);
    float localWeight = 0.0f;
    float reflectionWeight = 0.0f;
    float refractionWeight = 0.0f;
    if (material.refracts()) {
        localWeight = material.getLocalRefectivity();
        if (totalInternalReflection) {
            reflectionWeight = 1.0f - localWeight;
        } else {
            reflectionWeight = material.getReflectivity(cosI);
            refractionWeight = 1.0f - reflectionWeight;
            reflectionWeight = (1.0f - localWeight) * reflectionWeight;
            refractionWeight = (1.0f - localWeight) * refractionWeight;
        }
    } else if (material.reflects()) {
        reflectionWeight = material.getReflectivity(cosI);
        localWeight = 1.0f - reflectionWeight;
    } else {
        localWeight = 1.0f;
    }

    return localColor * localWeight + reflectionColor * reflectionWeight + refractionColor * refractionWeight;
}

Ray PhotonMapper::computeRay(float x, float y, const RaySetup& rs) const
{
    Vec3 dir{ rs.bottomLeft + rs.dX * x + rs.dY * y };
    dir = Vec3::normalize(dir);
    return Ray{ rs.rayOrigin, dir };
}

RaySetup PhotonMapper::computeRaySetup(const Image& img) const
{
    RaySetup rs;

    Vec3 forwardDir = camera.getViewDir();
    Vec3 upDir = camera.getUpDir();
    float openingAngle = float(camera.getFoV() * M_PI/180.0);
    rs.rayOrigin = camera.getEyePoint();

    float aspectRatio = ((float)img.width) / ((float)img.height);

    Vec3 rightDir = Vec3::cross(forwardDir, upDir);

    Vec3 rowVector = rightDir * (tan(openingAngle / 2.0f) * aspectRatio);
    Vec3 columnVector = upDir * (tan(openingAngle / 2.0f));

    rs.dX = rowVector * 2.0f / (float)img.width;
    rs.dY = columnVector * 2.0f / (float)img.height;

    rs.bottomLeft = forwardDir - columnVector - rowVector;

    return rs;
}

void PhotonMapper::tracePhotonPath(const Vec3& origin, const Vec3& direction, const Vec3& power)
{
    // Start the photon slightly away from the light source. Without this offset,
    // the first intersection test could hit the emitting surface itself because
    // of floating point roundoff.
    Ray ray{origin + direction * PHOTON_EPSILON, direction};
    Vec3 photonPower = power;

    // Follow one photon through the scene. A photon path is terminated when it
    // leaves the scene, hits a light source, becomes numerically irrelevant, or
    // reaches the maximum bounce count.
    for (int bounce = 0; bounce < photonTraceDepth && photons.size() < targetPhotonCount; ++bounce)
    {
        const std::optional<Intersection> intersection = scene.intersect(ray, false);
        if (!intersection)
            return;

        const Vec3 hitPosition = ray.getPosOnRay(intersection->getT());
        const Vec3 normal = intersection->getNormal();
        const Material material = intersection->getMaterial();

        if (material.emits())
            return;

        // Glass surfaces do not store photons in our photon map. They only
        // redirect the photon. We use Schlick reflectivity as a probability:
        // with probability R the photon reflects, otherwise it refracts.
        if (material.refracts())
        {
            const float cosI = Vec3::dot(ray.getDirection(), normal);
            const float reflectionProbability = material.getReflectivity(cosI);
            const Vec3 reflectionDirection = Vec3::reflect(ray.getDirection(), normal);
            const std::optional<Vec3> refractionDirection = Vec3::refract(ray.getDirection(), normal, *material.getIndexOfRefraction());

            Vec3 nextDirection = reflectionDirection;
            if (refractionDirection && Vec3::random().x >= reflectionProbability)
                nextDirection = *refractionDirection;

            ray = Ray{offsetRayOrigin(hitPosition, nextDirection, normal), nextDirection};
            continue;
        }

        // Perfect mirror-like surfaces also do not store photons. They change
        // the direction deterministically and tint the photon by the specular
        // material color.
        if (material.reflects())
        {
            const Vec3 nextDirection = Vec3::reflect(ray.getDirection(), normal);
            photonPower = photonPower * material.getSpecular();
            if (photonPower.sqlength() < PHOTON_MIN_POWER_SQ)
                return;

            ray = Ray{offsetRayOrigin(hitPosition, nextDirection, normal), nextDirection};
            continue;
        }

        const Vec3 diffuseColor = getDiffuseColor(*intersection);

        // A diffuse surface is where the photon map records incoming flux. We
        // store position, power, incoming direction, and the surface normal so
        // the gathering pass can later reject photons from the wrong side.
        if (diffuseColor.sqlength() > 0.0f)
            storePhoton(Photon{hitPosition, photonPower, ray.getDirection(), normal});

        // After a diffuse bounce, the surface albedo absorbs part of the photon
        // power. Very weak photons are terminated because their contribution is
        // below the numerical threshold used in this exercise.
        photonPower = photonPower * diffuseColor;
        if (photonPower.sqlength() < PHOTON_MIN_POWER_SQ)
            return;

        // Continue the photon in a random direction over the visible hemisphere.
        // This is the light-path equivalent of a diffuse bounce.
        const Vec3 nextDirection = sampleDiffuseDirection(normal);
        ray = Ray{offsetRayOrigin(hitPosition, nextDirection, normal), nextDirection};
    }
}

void PhotonMapper::storePhoton(const Photon& photon)
{
    const size_t photonIndex = photons.size();
    photons.push_back(photon);
    insertPhotonInGrid(photon.position, photonIndex);
}

Vec3 PhotonMapper::estimatePhotonRadiance(const Ray& localRay, const Intersection& intersection) const
{
    // If no photons have been emitted yet, no indirect illumination can be
    // reconstructed from the photon map.
    if (photons.empty() || photonGrid.cells.empty())
        return {};

    // The camera ray gives us the visible surface point where we want to
    // estimate outgoing radiance.
    const Vec3 position = localRay.getPosOnRay(intersection.getT());
    const Vec3 normal = intersection.getNormal();
    Vec3 powerSum;

    // The gather radius defines the small sphere around the shading point from
    // which photons contribute to the density estimate. The spatial hash grid
    // lets us inspect only cells that can overlap this sphere.
    const float radiusSq = PHOTON_GATHER_RADIUS * PHOTON_GATHER_RADIUS;
    const int minX = int(std::floor((position.x - PHOTON_GATHER_RADIUS) / PHOTON_GRID_CELL_SIZE));
    const int maxX = int(std::floor((position.x + PHOTON_GATHER_RADIUS) / PHOTON_GRID_CELL_SIZE));
    const int minY = int(std::floor((position.y - PHOTON_GATHER_RADIUS) / PHOTON_GRID_CELL_SIZE));
    const int maxY = int(std::floor((position.y + PHOTON_GATHER_RADIUS) / PHOTON_GRID_CELL_SIZE));
    const int minZ = int(std::floor((position.z - PHOTON_GATHER_RADIUS) / PHOTON_GRID_CELL_SIZE));
    const int maxZ = int(std::floor((position.z + PHOTON_GATHER_RADIUS) / PHOTON_GRID_CELL_SIZE));

    for (int z = minZ; z <= maxZ; ++z)
    {
        for (int y = minY; y <= maxY; ++y)
        {
            for (int x = minX; x <= maxX; ++x)
            {
                const auto cell = photonGrid.cells.find(photonGridKey(x, y, z));
                if (cell == photonGrid.cells.end())
                    continue;

                for (const size_t photonIndex : cell->second)
                {
                    const Photon& photon = photons[photonIndex];

                    // The hash grid only gives us candidate photons. The final
                    // radius test keeps exactly the photons inside the gather
                    // sphere.
                    const Vec3 delta = photon.position - position;
                    if (delta.sqlength() > radiusSq)
                        continue;

                    // Only photons that arrived from the front side of the
                    // currently visible surface should contribute.
                    if (Vec3::dot(photon.direction, normal) >= 0.0f)
                        continue;

                    // Reject photons stored on surfaces with very different
                    // normals. This avoids leaking light across sharp edges,
                    // for example between a wall and the floor.
                    if (Vec3::dot(photon.normal, normal) < PHOTON_MIN_NORMAL_DOT)
                        continue;

                    powerSum = powerSum + photon.power;
                }
            }
        }
    }

    // Convert the gathered photon power into a radiance estimate. This is a
    // simple density estimate: sum the photon flux in the disk/sphere around
    // the point, divide by its area, and multiply by the diffuse reflectance.
    const float gatherArea = float(M_PI) * radiusSq;
    const Vec3 diffuseColor = getDiffuseColor(intersection);
    return diffuseColor * powerSum * (PHOTON_RADIANCE_SCALE / gatherArea);
}

void PhotonMapper::buildPhotonGrid()
{
    initializePhotonGrid();

    for (size_t i = 0; i < photons.size(); ++i)
    {
        insertPhotonInGrid(photons[i].position, i);
    }
}

void PhotonMapper::initializePhotonGrid()
{
    photonGrid = PhotonGrid{};
    photonGrid.cells.reserve(targetPhotonCount);
}

void PhotonMapper::insertPhotonInGrid(const Vec3& position, size_t photonIndex)
{
    photonGrid.cells[photonGridKey(position)].push_back(photonIndex);
}

long long PhotonMapper::photonGridKey(int x, int y, int z) const
{
    const long long bx = static_cast<long long>(x + PHOTON_GRID_BIAS);
    const long long by = static_cast<long long>(y + PHOTON_GRID_BIAS);
    const long long bz = static_cast<long long>(z + PHOTON_GRID_BIAS);
    return (bx << 42) | (by << 21) | bz;
}

long long PhotonMapper::photonGridKey(const Vec3& position) const
{
    const int x = int(std::floor(position.x / PHOTON_GRID_CELL_SIZE));
    const int y = int(std::floor(position.y / PHOTON_GRID_CELL_SIZE));
    const int z = int(std::floor(position.z / PHOTON_GRID_CELL_SIZE));
    return photonGridKey(x, y, z);
}

Vec3 PhotonMapper::sampleDiffuseDirection(const Vec3& normal) const
{
    Vec3 direction = normal + Vec3::randomUnitVector();
    if (direction.sqlength() < 0.000001f)
        direction = normal;

    return Vec3::normalize(direction);
}

Vec3 PhotonMapper::getDiffuseColor(const Intersection& intersection) const
{
    return intersection.getMaterial().getDiffuse();
}
