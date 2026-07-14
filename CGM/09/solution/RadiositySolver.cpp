#include "RadiositySolver.h"

#include "Intersection.h"
#include "Ray.h"
#include "Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace {

constexpr float pi = 3.14159265358979323846f;

float channelValue(const Vec3& value, int channel) {
	return value.e[size_t(channel)];
}

float formFactorEstimate(const RadiosityPatch& receiver, const RadiosityPatch& emitter) {
	// The form factor F_ij describes which fraction of the energy leaving
	// patch j (the emitter in this function) arrives at patch i (the receiver).
	// We use a simple center-to-center approximation:
	//
	// F_ij ~= A_j cos(theta_i) cos(theta_j) / (pi r^2)
	//
	// This is not an exact area integral, but it is compact, easy to implement,
	// and works well enough for small patches.
	const Vec3 offset = emitter.center - receiver.center;
	const float distanceSquared = offset.sqlength();
	if (distanceSquared <= 0.0f)
		return 0.0f;

	// direction points from the receiver toward the emitter. This is the
	// direction of incoming light at the receiver patch.
	const Vec3 direction = Vec3::normalize(offset);

	// cos(theta_i): angle between receiver normal and incoming direction.
	// cos(theta_j): angle between emitter normal and the opposite direction,
	// because light leaves the emitter toward the receiver.
	const float receiverCosine = Vec3::dot(receiver.normal, direction);
	const float emitterCosine = Vec3::dot(emitter.normal, direction * -1.0f);

	// If either cosine is negative, one of the two patches faces away from the
	// other. In that case no diffuse energy is exchanged in this approximation.
	if (receiverCosine <= 0.0f || emitterCosine <= 0.0f)
		return 0.0f;

	// The emitter area A_j scales the amount of visible emitting surface, while
	// the inverse-square term reduces the contribution with distance.
	return emitter.area * receiverCosine * emitterCosine / (pi * distanceSquared);
}

} // namespace

RadiositySolver::RadiositySolver(size_t iterationCount)
	: iterationCount(iterationCount)
{
}

void RadiositySolver::setIterationCount(size_t iterationCount) {
	this->iterationCount = iterationCount;
}

void RadiositySolver::solve(const Scene& scene, std::vector<RadiosityPatch>& patches) {
	prepareFormFactorComputation(patches);
	while (!computeFormFactors(scene, patches, std::numeric_limits<size_t>::max())) {
	}
	finishSolve(patches);
}

void RadiositySolver::prepareFormFactorComputation(const std::vector<RadiosityPatch>& patches) {
	const size_t patchCount = patches.size();
	formFactors.resize(patchCount, patchCount, 0.0f);
	currentReceiver = 0;
	currentEmitter = 0;
	currentRowSum = 0.0f;
	formFactorComputationPrepared = true;
	formFactorComputationFinished = patchCount == 0;
}

bool RadiositySolver::computeFormFactors(const Scene& scene,
                                         const std::vector<RadiosityPatch>& patches,
                                         size_t maxFormFactors) {
	if (!formFactorComputationPrepared)
		prepareFormFactorComputation(patches);

	if (formFactorComputationFinished)
		return true;

	const size_t patchCount = patches.size();
	size_t computedFormFactors = 0;
	maxFormFactors = std::max<size_t>(1, maxFormFactors);

	// The computation is written as an incremental double loop. Each call
	// continues where the previous call stopped, so the application can draw a
	// progress frame between batches of form-factor entries.
	while (currentReceiver < patchCount && computedFormFactors < maxFormFactors) {
		if (currentEmitter < patchCount) {
			// A patch does not exchange energy with itself in this simple model.
			// We also require visibility; otherwise walls or objects between
			// the two patch centers should block the transfer.
			if (currentReceiver != currentEmitter && visible(scene, patches[currentReceiver], patches[currentEmitter])) {
				const float formFactor = formFactorEstimate(patches[currentReceiver], patches[currentEmitter]);
				formFactors(currentReceiver, currentEmitter) = formFactor;
				currentRowSum += formFactor;
			}

			++currentEmitter;
			++computedFormFactors;
			continue;
		}

		// The center-to-center approximation can overestimate a row, especially
		// for coarse patches. Clamping each row below one keeps the transport
		// matrix energy stable for this exercise implementation.
		if (currentRowSum > 0.95f) {
			const float scale = 0.95f / currentRowSum;
			for (size_t emitter = 0; emitter < patchCount; ++emitter)
				formFactors(currentReceiver, emitter) *= scale;
		}

		++currentReceiver;
		currentEmitter = 0;
		currentRowSum = 0.0f;
	}

	formFactorComputationFinished = currentReceiver >= patchCount;
	return formFactorComputationFinished;
}

void RadiositySolver::finishSolve(std::vector<RadiosityPatch>& patches) const {
	if (!formFactorComputationFinished)
		throw std::runtime_error("Cannot finish radiosity solve before form factors are complete.");

	for (int channel = 0; channel < 3; ++channel) {
		const LargeVector radiosity = solveChannel(formFactors, patches, channel);
		for (size_t i = 0; i < patches.size(); ++i)
			patches[i].radiosity.e[size_t(channel)] = radiosity[i];
	}
}

float RadiositySolver::formFactorProgress() const {
	const size_t rowCount = formFactors.rows();
	const size_t columnCount = formFactors.columns();
	if (rowCount == 0 || columnCount == 0)
		return formFactorComputationFinished ? 1.0f : 0.0f;

	const size_t finishedEntries = currentReceiver * columnCount + currentEmitter;
	const size_t totalEntries = rowCount * columnCount;
	return std::min(1.0f, float(finishedEntries) / float(totalEntries));
}

size_t RadiositySolver::completedFormFactorRows() const {
	return currentReceiver;
}

size_t RadiositySolver::totalFormFactorRows() const {
	return formFactors.rows();
}

LargeVector RadiositySolver::solveChannel(const LargeMatrix& formFactors,
                                          const std::vector<RadiosityPatch>& patches,
                                          int channel) const {
	const size_t patchCount = patches.size();
	LargeMatrix transport(patchCount, patchCount, 0.0f);
	LargeVector emission(patchCount, 0.0f);
	LargeVector current(patchCount, 0.0f);

	for (size_t receiver = 0; receiver < patchCount; ++receiver) {
		const float reflectance = channelValue(patches[receiver].reflectance, channel);
		emission[receiver] = channelValue(patches[receiver].emission, channel);
		current[receiver] = emission[receiver];
		for (size_t emitter = 0; emitter < patchCount; ++emitter)
			transport(receiver, emitter) = reflectance * formFactors(receiver, emitter);
	}

	for (size_t iteration = 0; iteration < iterationCount; ++iteration) {
		LargeVector next = transport * current;
		for (size_t i = 0; i < patchCount; ++i)
			next[i] += emission[i];

		if (next.maxAbsDifference(current) < 0.0001f)
			return next;

		current = next;
	}

	return current;
}

bool RadiositySolver::visible(const Scene& scene,
                              const RadiosityPatch& from,
                              const RadiosityPatch& to) const {
	// Visibility is also approximated at patch centers. We trace one ray from
	// the center of the receiver patch to the center of the emitter patch.
	const Vec3 segment = to.center - from.center;
	const float distance = segment.length();
	if (distance <= visibilityEpsilon)
		return false;

	const Vec3 direction = segment / distance;

	// Start the ray slightly away from the first patch. Without this epsilon,
	// numerical roundoff can make the ray intersect the patch it starts from.
	const Ray ray{from.center + direction * visibilityEpsilon, direction};
	const std::optional<Intersection> intersection = scene.intersect(ray);

	// If the closest hit is farther away than the target patch center, nothing
	// blocks the line segment between the two patch centers.
	return !intersection || intersection->getT() >= distance - visibilityEpsilon * 2.0f;
}
