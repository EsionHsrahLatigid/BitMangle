#include "BitManglePlugin.h"

#include "ProductState.h"

#if ! BITMANGLE_HEADLESS_TEST
#include "ParameterGridEditor.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace bitmangle::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'B', 'M', 'N', '1' }};
constexpr int stateVersion = 1;
constexpr std::size_t presetParameterCount = 7;
constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ 8.0f, 0.45f, 0.3f, 0.2f, 0.45f, 0.35f, 1.0f }},
    {{ 4.0f, 0.78f, 0.8f, 0.5f, 0.7f, 0.6f, 1.0f }},
    {{ 12.0f, 0.25f, 0.05f, 0.1f, 0.8f, 0.2f, 0.7f }},
    {{ 6.0f, 0.9f, 0.55f, 0.9f, 0.25f, 1.0f, 0.85f }}
}};

yup::AudioParameter::Ptr makeParameter (const char* id, const char* name, int hostID, float minValue, float maxValue, float defaultValue, yup::AudioParameter::ParameterUnit unit, float smoothingMs)
{
    return yup::AudioParameterBuilder().withID (id).withName (name).withHostID (static_cast<yup::uint32> (hostID)).withRange (minValue, maxValue).withDefault (defaultValue).withSmoothing (smoothingMs).withModulatable (true).withUnit (unit).build();
}
}

BitManglePlugin::BitManglePlugin()
    : yup::AudioProcessor ("BitMangle", yup::AudioBusLayout ({ yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Input, 2) }, { yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2) }))
{
    parameters[bits] = makeParameter ("bits", "Bits", bits, 2.0f, 16.0f, presetValues[0][bits], yup::AudioParameter::ParameterUnit::Generic, 18.0f);
    parameters[reduce] = makeParameter ("reduce", "Reduce", reduce, 0.0f, 1.0f, presetValues[0][reduce], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[curve] = makeParameter ("curve", "Curve", curve, 0.0f, 1.0f, presetValues[0][curve], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[dither] = makeParameter ("dither", "Dither", dither, 0.0f, 1.0f, presetValues[0][dither], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[antiAlias] = makeParameter ("antiAlias", "AntiAlias", antiAlias, 0.0f, 1.0f, presetValues[0][antiAlias], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[motion] = makeParameter ("motion", "Motion", motion, 0.0f, 1.0f, presetValues[0][motion], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    parameters[mix] = makeParameter ("mix", "Mix", mix, 0.0f, 1.0f, presetValues[0][mix], yup::AudioParameter::ParameterUnit::Percent, 18.0f);
    for (const auto& parameter : parameters)
        addParameter (parameter);
    syncParameterValuesFromParameters();
    updateEngineParameters();
}

void BitManglePlugin::prepareToPlay (const yup::AudioSpec& spec)
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

void BitManglePlugin::releaseResources() {}

void BitManglePlugin::processBlock (yup::AudioProcessContext<float>& context)
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
        if (controlUpdateCountdown <= 0) { updateEngineParameters(); controlUpdateCountdown = parameterUpdateCadenceSamples; }
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
        if (left != nullptr) left[sample] = frame.left;
        if (right != nullptr) right[sample] = frame.right;
        blockOutputPeak = std::max (blockOutputPeak, std::max (std::fabs (frame.left), std::fabs (frame.right)));
        for (int channel = 2; channel < numChannels; ++channel) audio.getWritePointer (channel)[sample] = 0.0f;
    }
    inputPeakMilli.store (static_cast<int> (std::clamp (blockInputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f), std::memory_order_relaxed);
    outputPeakMilli.store (static_cast<int> (std::clamp (blockOutputPeak, 0.0f, 1.0f) * 1000.0f + 0.5f), std::memory_order_relaxed);
    context.midi.clear();
}

void BitManglePlugin::flush()
{
    engine.reset(); controlUpdateCountdown = 0; inputPeakMilli.store (0, std::memory_order_relaxed); outputPeakMilli.store (0, std::memory_order_relaxed);
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
    auditionPhase = 0.0f; auditionNoise = 0x6d2b79f5u;
#endif
}

bool BitManglePlugin::acceptsMidi() const noexcept { return false; }
bool BitManglePlugin::producesMidi() const noexcept { return false; }
int BitManglePlugin::getCurrentPreset() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
void BitManglePlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size()))) return;
    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i) parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);
}
int BitManglePlugin::getNumPresets() const { return static_cast<int> (presetNames.size()); }
yup::String BitManglePlugin::getPresetName (int index) const { return yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())) ? presetNames[static_cast<std::size_t> (index)] : "Invalid Preset"; }
void BitManglePlugin::setPresetName (int index, yup::StringRef newName) { if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size()))) presetNames[static_cast<std::size_t> (index)] = newName; }
yup::Result BitManglePlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    int loadedPreset = 0; const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
    if (result.failed()) return result; currentPreset.store (loadedPreset, std::memory_order_relaxed); return yup::Result::ok();
}
yup::Result BitManglePlugin::saveStateIntoMemory (yup::MemoryBlock& data) { return saveProductState (*this, data, stateMagic, stateVersion, currentPreset.load (std::memory_order_relaxed)); }
bool BitManglePlugin::hasEditor() const
{
#if BITMANGLE_HEADLESS_TEST
    return false;
#else
    return true;
#endif
}
yup::AudioProcessorEditor* BitManglePlugin::createEditor()
{
#if BITMANGLE_HEADLESS_TEST
    return nullptr;
#else
    return new ParameterGridEditor (*this, "BitMangle", "Quantizing sample-rate reduction effect with standalone-only audition.", 0xfff2f2f0u);
#endif
}
float BitManglePlugin::getInputPeakLevel() const noexcept { return static_cast<float> (inputPeakMilli.load (std::memory_order_relaxed)) * 0.001f; }
float BitManglePlugin::getOutputPeakLevel() const noexcept { return static_cast<float> (outputPeakMilli.load (std::memory_order_relaxed)) * 0.001f; }
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
void BitManglePlugin::setAuditionEnabled (bool shouldBeEnabled) noexcept { auditionEnabled.store (shouldBeEnabled ? 1 : 0, std::memory_order_relaxed); }
bool BitManglePlugin::isAuditionEnabled() const noexcept { return auditionEnabled.load (std::memory_order_relaxed) != 0; }
void BitManglePlugin::setAuditionType (int type) noexcept { auditionType.store (std::clamp (type, 0, 1), std::memory_order_relaxed); }
int BitManglePlugin::getAuditionType() const noexcept { return auditionType.load (std::memory_order_relaxed); }
#endif
void BitManglePlugin::advanceParameterHandles (int samplePosition) noexcept
{
    for (std::size_t i = 0; i < parameterHandles.size(); ++i) { parameterHandles[i].advanceToSample (samplePosition); currentParameterValues[i] = parameterHandles[i].getNextValue(); }
}
void BitManglePlugin::syncParameterValuesFromParameters() noexcept { for (std::size_t i = 0; i < parameters.size(); ++i) currentParameterValues[i] = parameters[i]->getValue(); }
void BitManglePlugin::updateEngineParameters() noexcept
{
    bitmangle::BitMangleParameters engineParameters;
    engineParameters.bits = currentParameterValues[bits];
    engineParameters.reduce = currentParameterValues[reduce];
    engineParameters.curve = currentParameterValues[curve];
    engineParameters.dither = currentParameterValues[dither];
    engineParameters.antiAlias = currentParameterValues[antiAlias];
    engineParameters.motion = currentParameterValues[motion];
    engineParameters.mix = currentParameterValues[mix];
    engine.setParameters (engineParameters);
}
#if defined(YUP_AUDIO_PLUGIN_ENABLE_STANDALONE)
StereoFrame BitManglePlugin::renderAuditionFrame() noexcept
{
    if (auditionEnabled.load (std::memory_order_relaxed) == 0) return {};
    auditionPhase += 96.0f / static_cast<float> (auditionSampleRate);
    if (auditionPhase >= 1.0f) auditionPhase -= 1.0f;
    auditionNoise ^= auditionNoise << 13u; auditionNoise ^= auditionNoise >> 17u; auditionNoise ^= auditionNoise << 5u;
    if (auditionNoise == 0u) auditionNoise = 0x6d2b79f5u;
    const auto type = auditionType.load (std::memory_order_relaxed);
    const auto noise = static_cast<float> (static_cast<double> (auditionNoise) / 2147483648.0 - 1.0);
    const auto pulse = auditionPhase < 0.18f ? 1.0f : -0.55f;
    const auto saw = auditionPhase * 2.0f - 1.0f;
    const auto source = type == 0 ? saw * 0.22f + noise * 0.035f : pulse * 0.18f + noise * 0.055f;
    return { source, source * 0.93f };
}
#endif

} // namespace bitmangle::plugin

extern "C" yup::AudioProcessor* createPluginProcessor() { return new bitmangle::plugin::BitManglePlugin(); }
