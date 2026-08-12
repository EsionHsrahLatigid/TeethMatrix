#include "TeethMatrixPlugin.h"

#include "ProductState.h"

#if ! TEETHMATRIX_HEADLESS_TEST
#include "ParameterGridEditor.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace teethmatrix::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'T', 'T', 'M', '1' }};
constexpr int stateVersion = 1;
constexpr std::size_t presetParameterCount = 7;

constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ 0.38f, 0.58f, 0.42f, 0.47f, 0.24f, 0.36f, 0.52f }},
    {{ 0.18f, 0.64f, 0.68f, 0.22f, 0.12f, 0.28f, 0.46f }},
    {{ 0.63f, 0.78f, 0.36f, 0.82f, 0.31f, 0.57f, 0.58f }},
    {{ 0.84f, 0.88f, 0.51f, 0.66f, 0.77f, 0.74f, 0.62f }}
}};

yup::AudioParameter::Ptr makeParameter (const char* id,
                                        const char* name,
                                        int hostID,
                                        float minValue,
                                        float maxValue,
                                        float defaultValue,
                                        yup::AudioParameter::ParameterUnit unit,
                                        float smoothingMs)
{
    return yup::AudioParameterBuilder()
        .withID (id)
        .withName (name)
        .withHostID (static_cast<yup::uint32> (hostID))
        .withRange (minValue, maxValue)
        .withDefault (defaultValue)
        .withSmoothing (smoothingMs)
        .withModulatable (true)
        .withUnit (unit)
        .build();
}
} // namespace

TeethMatrixPlugin::TeethMatrixPlugin()
    : yup::AudioProcessor ("TeethMatrix",
                           yup::AudioBusLayout ({
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2),
                                                },
                                                {
                                                    yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2),
                                                }))
{
    parameters[tune] = makeParameter ("tune", "Tune", tune, 0.0f, 1.0f, presetValues[0][tune], yup::AudioParameter::ParameterUnit::Percent, 24.0f);
    parameters[feedback] = makeParameter ("feedback", "Feedback", feedback, 0.0f, 1.0f, presetValues[0][feedback], yup::AudioParameter::ParameterUnit::Percent, 22.0f);
    parameters[damping] = makeParameter ("damping", "Damping", damping, 0.0f, 1.0f, presetValues[0][damping], yup::AudioParameter::ParameterUnit::Percent, 28.0f);
    parameters[dispersion] = makeParameter ("dispersion", "Dispersion", dispersion, 0.0f, 1.0f, presetValues[0][dispersion], yup::AudioParameter::ParameterUnit::Percent, 22.0f);
    parameters[motion] = makeParameter ("motion", "Motion", motion, 0.0f, 1.0f, presetValues[0][motion], yup::AudioParameter::ParameterUnit::Percent, 32.0f);
    parameters[drive] = makeParameter ("drive", "Drive", drive, 0.0f, 1.0f, presetValues[0][drive], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[mix] = makeParameter ("mix", "Mix", mix, 0.0f, 1.0f, presetValues[0][mix], yup::AudioParameter::ParameterUnit::Percent, 20.0f);

    for (const auto& parameter : parameters)
        addParameter (parameter);

    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void TeethMatrixPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);
    engine.reset();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);

    syncParameterValuesFromParameters();
    updateEngineParameters();
    controlUpdateCountdown = 0;
    inputPeakMilli.store (0, std::memory_order_relaxed);
    outputPeakMilli.store (0, std::memory_order_relaxed);

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionSampleRate = std::isfinite (spec.sampleRate) && spec.sampleRate > 1.0 ? spec.sampleRate : 44100.0;
    auditionPhase = 0.0f;
    auditionNoise = 0x6d2b79f5u;
#endif
}

void TeethMatrixPlugin::releaseResources()
{
}

void TeethMatrixPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;
    float blockInputPeak = 0.0f;
    float blockOutputPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        advanceParameterHandles (sample);
        if (controlUpdateCountdown <= 0)
        {
            updateEngineParameters();
            controlUpdateCountdown = parameterUpdateCadenceSamples;
        }
        --controlUpdateCountdown;

        auto inputLeft = left != nullptr ? left[sample] : 0.0f;
        auto inputRight = right != nullptr ? right[sample] : inputLeft;

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
        const auto audition = renderAuditionFrame();
        inputLeft += audition.left;
        inputRight += audition.right;
#endif

        blockInputPeak = std::max (blockInputPeak, std::max (std::fabs (inputLeft), std::fabs (inputRight)));

        const auto frame = engine.processSample (inputLeft, inputRight);
        if (left != nullptr)
            left[sample] = frame.left;
        if (right != nullptr)
            right[sample] = frame.right;
        blockOutputPeak = std::max (blockOutputPeak, std::max (std::fabs (frame.left), std::fabs (frame.right)));

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;
    }

    inputPeakMilli.store (static_cast<int> (std::clamp (blockInputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f),
                          std::memory_order_relaxed);
    outputPeakMilli.store (static_cast<int> (std::clamp (blockOutputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f),
                           std::memory_order_relaxed);
    context.midi.clear();
}

void TeethMatrixPlugin::flush()
{
    engine.reset();
    controlUpdateCountdown = 0;
    inputPeakMilli.store (0, std::memory_order_relaxed);
    outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionPhase = 0.0f;
    auditionNoise = 0x6d2b79f5u;
#endif
}

bool TeethMatrixPlugin::acceptsMidi() const noexcept
{
    return false;
}

bool TeethMatrixPlugin::producesMidi() const noexcept
{
    return false;
}

int TeethMatrixPlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void TeethMatrixPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}

int TeethMatrixPlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String TeethMatrixPlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void TeethMatrixPlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result TeethMatrixPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0;
    const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
    if (result.failed())
        return result;

    currentPreset.store (loadedPreset, std::memory_order_relaxed);
    return yup::Result::ok();
}

yup::Result TeethMatrixPlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed));
}

bool TeethMatrixPlugin::hasEditor() const
{
#if TEETHMATRIX_HEADLESS_TEST
    return false;
#else
    return true;
#endif
}

yup::AudioProcessorEditor* TeethMatrixPlugin::createEditor()
{
#if TEETHMATRIX_HEADLESS_TEST
    return nullptr;
#else
    return new ParameterGridEditor (*this,
                                    "TeethMatrix",
                                    "Fractional feedback comb with standalone-only audition.",
                                    0xfff2f2f0u);
#endif
}

float TeethMatrixPlugin::getInputPeakLevel() const noexcept
{
    return static_cast<float> (inputPeakMilli.load (std::memory_order_relaxed)) * 0.001f;
}

float TeethMatrixPlugin::getOutputPeakLevel() const noexcept
{
    return static_cast<float> (outputPeakMilli.load (std::memory_order_relaxed)) * 0.001f;
}

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
void TeethMatrixPlugin::setAuditionEnabled (bool shouldBeEnabled) noexcept
{
    auditionEnabled.store (shouldBeEnabled ? 1 : 0, std::memory_order_relaxed);
}

bool TeethMatrixPlugin::isAuditionEnabled() const noexcept
{
    return auditionEnabled.load (std::memory_order_relaxed) != 0;
}

void TeethMatrixPlugin::setAuditionType (int type) noexcept
{
    auditionType.store (std::clamp (type, 0, 1), std::memory_order_relaxed);
}

int TeethMatrixPlugin::getAuditionType() const noexcept
{
    return auditionType.load (std::memory_order_relaxed);
}
#endif

void TeethMatrixPlugin::advanceParameterHandles (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
    {
        parameterHandles[i].advanceToSample (samplePosition);
        currentParameterValues[i] = parameterHandles[i].getNextValue();
    }
}

void TeethMatrixPlugin::syncParameterValuesFromParameters() noexcept
{
    for (std::size_t i = 0; i < parameters.size(); ++i)
        currentParameterValues[i] = parameters[i]->getValue();
}

void TeethMatrixPlugin::updateEngineParameters() noexcept
{
    teethmatrix::TeethMatrixParameters engineParameters;
    engineParameters.tune = currentParameterValues[tune];
    engineParameters.feedback = currentParameterValues[feedback];
    engineParameters.damping = currentParameterValues[damping];
    engineParameters.dispersion = currentParameterValues[dispersion];
    engineParameters.motion = currentParameterValues[motion];
    engineParameters.drive = currentParameterValues[drive];
    engineParameters.mix = currentParameterValues[mix];
    engine.setParameters (engineParameters);
}

#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
StereoFrame TeethMatrixPlugin::renderAuditionFrame() noexcept
{
    if (auditionEnabled.load (std::memory_order_relaxed) == 0)
        return {};

    auditionPhase += 96.0f / static_cast<float> (auditionSampleRate);
    if (auditionPhase >= 1.0f)
        auditionPhase -= 1.0f;

    auditionNoise ^= auditionNoise << 13u;
    auditionNoise ^= auditionNoise >> 17u;
    auditionNoise ^= auditionNoise << 5u;
    if (auditionNoise == 0u)
        auditionNoise = 0x6d2b79f5u;

    const auto type = auditionType.load (std::memory_order_relaxed);
    const auto noise = static_cast<float> (static_cast<double> (auditionNoise) / 2147483648.0 - 1.0);
    const auto pulse = auditionPhase < 0.18f ? 1.0f : -0.55f;
    const auto saw = auditionPhase * 2.0f - 1.0f;
    const auto source = type == 0 ? saw * 0.22f + noise * 0.035f : pulse * 0.18f + noise * 0.055f;
    return { source, source * 0.93f };
}
#endif

} // namespace teethmatrix::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new teethmatrix::plugin::TeethMatrixPlugin();
}
