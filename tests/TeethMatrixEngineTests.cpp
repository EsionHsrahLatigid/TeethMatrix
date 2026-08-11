#include "teethmatrix/TeethMatrixEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

using teethmatrix::TeethMatrixEngine;
using teethmatrix::TeethMatrixParameters;

namespace
{
std::vector<float> renderImpulse (TeethMatrixParameters params, int samples)
{
    TeethMatrixEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.reset();

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
    {
        const auto input = i == 0 ? 0.8f : 0.0f;
        output.push_back (engine.processSample (input, -input * 0.5f).left);
    }
    return output;
}

float energyBetween (const std::vector<float>& samples, int start, int end)
{
    float energy = 0.0f;
    const auto safeStart = std::max (0, start);
    const auto safeEnd = std::min (end, static_cast<int> (samples.size()));
    for (int i = safeStart; i < safeEnd; ++i)
        energy += samples[static_cast<std::size_t> (i)] * samples[static_cast<std::size_t> (i)];
    return energy;
}

float difference (const std::vector<float>& a, const std::vector<float>& b)
{
    float total = 0.0f;
    for (std::size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        total += std::fabs (a[i] - b[i]);
    return total;
}

void testSilenceStaysSilent()
{
    TeethMatrixEngine engine;
    engine.prepare (48000.0);
    engine.reset();
    for (int i = 0; i < 8192; ++i)
    {
        const auto frame = engine.processSample (0.0f, 0.0f);
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}

void testTuneMovesCombArrival()
{
    TeethMatrixParameters shortComb;
    shortComb.tune = 0.12f;
    shortComb.feedback = 0.0f;
    shortComb.drive = 0.4f;
    shortComb.mix = 1.0f;

    auto longComb = shortComb;
    longComb.tune = 0.82f;

    const auto shortOutput = renderImpulse (shortComb, 4096);
    const auto longOutput = renderImpulse (longComb, 4096);
    assert (energyBetween (shortOutput, 30, 160) > energyBetween (longOutput, 30, 160) + 1.0e-5f);
    assert (energyBetween (longOutput, 1500, 1750) > energyBetween (shortOutput, 1500, 1750) + 1.0e-5f);
}

void testFeedbackCreatesLongTail()
{
    TeethMatrixParameters dead;
    dead.tune = 0.24f;
    dead.feedback = 0.0f;
    dead.drive = 0.32f;
    dead.mix = 1.0f;

    auto ringing = dead;
    ringing.feedback = 0.95f;

    const auto deadOutput = renderImpulse (dead, 12000);
    const auto ringingOutput = renderImpulse (ringing, 12000);
    assert (energyBetween (ringingOutput, 4000, 12000) > energyBetween (deadOutput, 4000, 12000) + 1.0e-5f);
}

void testDispersionAndMotionChangeResponse()
{
    TeethMatrixParameters plain;
    plain.tune = 0.43f;
    plain.feedback = 0.82f;
    plain.dispersion = 0.0f;
    plain.motion = 0.0f;
    plain.drive = 0.35f;
    plain.mix = 1.0f;

    auto moving = plain;
    moving.dispersion = 1.0f;
    moving.motion = 1.0f;

    assert (difference (renderImpulse (plain, 8192), renderImpulse (moving, 8192)) > 0.05f);
}

void testDeterministic()
{
    TeethMatrixParameters params;
    params.tune = 0.61f;
    params.feedback = 0.87f;
    params.dispersion = 0.74f;
    params.motion = 0.63f;
    const auto a = renderImpulse (params, 8192);
    const auto b = renderImpulse (params, 8192);
    assert (a == b);
}

void testFiniteBoundedExtremeParameters()
{
    TeethMatrixParameters params;
    params.tune = 1000.0f;
    params.feedback = 1000.0f;
    params.damping = std::numeric_limits<float>::infinity();
    params.dispersion = 1000.0f;
    params.motion = 1000.0f;
    params.drive = 1000.0f;
    params.mix = 1000.0f;

    TeethMatrixEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.reset();
    for (int i = 0; i < 200000; ++i)
    {
        const auto frame = engine.processSample (1000.0f, -1000.0f);
        assert (std::isfinite (frame.left));
        assert (std::isfinite (frame.right));
        assert (frame.left >= -0.9801f && frame.left <= 0.9801f);
        assert (frame.right >= -0.9801f && frame.right <= 0.9801f);
    }
}

void testDenormalInputDoesNotLeak()
{
    TeethMatrixEngine engine;
    engine.prepare (48000.0);
    engine.reset();
    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample (1.0e-30f, -1.0e-30f);
        assert (std::fabs (frame.left) <= 1.0e-7f);
        assert (std::fabs (frame.right) <= 1.0e-7f);
    }
}
} // namespace

int main()
{
    testSilenceStaysSilent();
    testTuneMovesCombArrival();
    testFeedbackCreatesLongTail();
    testDispersionAndMotionChangeResponse();
    testDeterministic();
    testFiniteBoundedExtremeParameters();
    testDenormalInputDoesNotLeak();
    std::cout << "TeethMatrixEngineTests passed\n";
    return 0;
}
