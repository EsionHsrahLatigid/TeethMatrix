#include "teethmatrix/TeethMatrixEngine.h"

#include <algorithm>
#include <cmath>

namespace teethmatrix
{
namespace
{
constexpr float ceiling = 0.98f;
constexpr float twoPi = 6.2831853071795864769f;
}

TeethMatrixEngine::TeethMatrixEngine()
    : delayLeft (std::make_unique<DelayBuffer>()),
      delayRight (std::make_unique<DelayBuffer>())
{
    prepare (44100.0);
    reset();
}

void TeethMatrixEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    reset();
}

void TeethMatrixEngine::reset() noexcept
{
    delayLeft->fill (0.0f);
    delayRight->fill (0.0f);
    writeIndex = 0;
    dampLeft = 0.0f;
    dampRight = 0.0f;
    allpassLeft = 0.0f;
    allpassRight = 0.0f;
    motionPhase = 0.0f;
}

void TeethMatrixEngine::setParameters (const TeethMatrixParameters& parameters) noexcept
{
    params.tune = clampFinite (parameters.tune, 0.0f, 1.0f, TeethMatrixParameters {}.tune);
    params.feedback = clampFinite (parameters.feedback, 0.0f, 1.0f, TeethMatrixParameters {}.feedback);
    params.damping = clampFinite (parameters.damping, 0.0f, 1.0f, TeethMatrixParameters {}.damping);
    params.dispersion = clampFinite (parameters.dispersion, 0.0f, 1.0f, TeethMatrixParameters {}.dispersion);
    params.motion = clampFinite (parameters.motion, 0.0f, 1.0f, TeethMatrixParameters {}.motion);
    params.drive = clampFinite (parameters.drive, 0.0f, 1.0f, TeethMatrixParameters {}.drive);
    params.mix = clampFinite (parameters.mix, 0.0f, 1.0f, TeethMatrixParameters {}.mix);
}

StereoFrame TeethMatrixEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto dryLeft = sanitizeAudio (inputLeft);
    const auto dryRight = sanitizeAudio (inputRight);

    motionPhase += (0.045f + params.motion * 0.31f) / static_cast<float> (sampleRate);
    if (motionPhase >= 1.0f)
        motionPhase -= 1.0f;

    const auto polarity = params.tune < 0.5f ? -1.0f : 1.0f;
    const auto wetLeft = processSide (dryLeft, *delayLeft, dampLeft, allpassLeft, polarity, 0.0f);
    const auto wetRight = processSide (dryRight, *delayRight, dampRight, allpassRight, -polarity, 0.37f);
    writeIndex = (writeIndex + 1) % maxDelaySamples;

    const auto dry = 1.0f - params.mix;
    return sanitizeFrame (dryLeft * dry + wetLeft * params.mix,
                          dryRight * dry + wetRight * params.mix);
}

void TeethMatrixEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;
    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample (left[i], right[i]);
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

float TeethMatrixEngine::readDelay (const DelayBuffer& buffer, float delaySamples) const noexcept
{
    const auto integral = static_cast<int> (delaySamples);
    const auto fraction = delaySamples - static_cast<float> (integral);
    auto indexA = writeIndex - integral;
    while (indexA < 0)
        indexA += maxDelaySamples;
    const auto indexB = (indexA - 1 + maxDelaySamples) % maxDelaySamples;
    return buffer[static_cast<std::size_t> (indexA)] * (1.0f - fraction)
         + buffer[static_cast<std::size_t> (indexB)] * fraction;
}

float TeethMatrixEngine::processSide (float input,
                                      DelayBuffer& buffer,
                                      float& damped,
                                      float& allpassState,
                                      float polarity,
                                      float phaseOffset) noexcept
{
    const auto baseSamples = 18.0f + params.tune * params.tune * 2400.0f;
    const auto motion = std::sin ((motionPhase + phaseOffset) * twoPi) * params.motion * (0.4f + baseSamples * 0.018f);
    const auto delayed = readDelay (buffer, std::clamp (baseSamples + motion, 2.0f, static_cast<float> (maxDelaySamples - 4)));
    const auto dampAmount = 0.05f + params.damping * 0.9f;
    damped += dampAmount * (delayed - damped);

    const auto allpassGain = 0.08f + params.dispersion * 0.62f;
    const auto dispersed = -allpassGain * damped + allpassState;
    allpassState = damped + allpassGain * dispersed;

    const auto feedback = params.feedback * 0.88f * polarity;
    const auto driven = boundedDrive (input + dispersed * feedback, 1.0f + params.drive * 5.0f);
    buffer[static_cast<std::size_t> (writeIndex)] = sanitizeAudio (driven);

    return boundedDrive (dispersed + driven * params.drive * 0.28f, 1.0f + params.drive * 2.0f);
}

float TeethMatrixEngine::sanitizeAudio (float value) const noexcept
{
    return clampFinite (value, -8.0f, 8.0f, 0.0f);
}

StereoFrame TeethMatrixEngine::sanitizeFrame (float left, float right) const noexcept
{
    auto safeLeft = boundedDrive (left, 1.02f + params.drive * 0.72f);
    auto safeRight = boundedDrive (right, 1.02f + params.drive * 0.72f);
    if (std::fabs (safeLeft) < 1.0e-20f)
        safeLeft = 0.0f;
    if (std::fabs (safeRight) < 1.0e-20f)
        safeRight = 0.0f;
    return { std::clamp (safeLeft, -ceiling, ceiling),
             std::clamp (safeRight, -ceiling, ceiling) };
}

} // namespace teethmatrix
