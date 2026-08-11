#pragma once

#include "teethmatrix/TeethMatrixDspPrimitives.h"

#include <array>

namespace teethmatrix
{

struct TeethMatrixParameters
{
    float tune = 0.38f;
    float feedback = 0.58f;
    float damping = 0.42f;
    float dispersion = 0.47f;
    float motion = 0.24f;
    float drive = 0.36f;
    float mix = 0.52f;
};

class TeethMatrixEngine
{
public:
    TeethMatrixEngine();

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters (const TeethMatrixParameters& parameters) noexcept;
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    static constexpr int maxDelaySamples = 65536;

    struct ClampedParameters
    {
        float tune = 0.38f;
        float feedback = 0.58f;
        float damping = 0.42f;
        float dispersion = 0.47f;
        float motion = 0.24f;
        float drive = 0.36f;
        float mix = 0.52f;
    };

    [[nodiscard]] float readDelay (const std::array<float, maxDelaySamples>& buffer, float delaySamples) const noexcept;
    [[nodiscard]] float processSide (float input,
                                     std::array<float, maxDelaySamples>& buffer,
                                     float& damped,
                                     float& allpassState,
                                     float polarity,
                                     float phaseOffset) noexcept;
    [[nodiscard]] float sanitizeAudio (float value) const noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    ClampedParameters params;
    double sampleRate = 44100.0;
    std::array<float, maxDelaySamples> delayLeft {};
    std::array<float, maxDelaySamples> delayRight {};
    int writeIndex = 0;
    float dampLeft = 0.0f;
    float dampRight = 0.0f;
    float allpassLeft = 0.0f;
    float allpassRight = 0.0f;
    float motionPhase = 0.0f;
};

} // namespace teethmatrix
