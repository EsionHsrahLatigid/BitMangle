#include "bitmangle/BitMangleEngine.h"

#include <algorithm>
#include <cmath>

namespace bitmangle
{

BitMangleEngine::BitMangleEngine() { prepare (44100.0); reset(); }

void BitMangleEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    updateFilters();
    reset();
}

void BitMangleEngine::reset() noexcept
{
    holdCountdown = 0;
    heldLeft = 0.0f; heldRight = 0.0f;
    errorLeft = 0.0f; errorRight = 0.0f;
    motionPhase = 0.0f;
    preLeft.reset(); preRight.reset();
    noise.reset (0x4269744du);
}

void BitMangleEngine::setParameters (const BitMangleParameters& p) noexcept
{
    params.bits = clampFinite (p.bits, 2.0f, 16.0f, BitMangleParameters {}.bits);
    params.reduce = clampFinite (p.reduce, 0.0f, 1.0f, BitMangleParameters {}.reduce);
    params.curve = clampFinite (p.curve, 0.0f, 1.0f, BitMangleParameters {}.curve);
    params.dither = clampFinite (p.dither, 0.0f, 1.0f, BitMangleParameters {}.dither);
    params.antiAlias = clampFinite (p.antiAlias, 0.0f, 1.0f, BitMangleParameters {}.antiAlias);
    params.motion = clampFinite (p.motion, 0.0f, 1.0f, BitMangleParameters {}.motion);
    params.mix = clampFinite (p.mix, 0.0f, 1.0f, BitMangleParameters {}.mix);
    holdSamples = 1 + static_cast<int> (std::round (params.reduce * params.reduce * 95.0f));
    levels = std::max (3.0f, std::exp2 (std::round (params.bits)) - 1.0f);
    updateFilters();
}

void BitMangleEngine::updateFilters() noexcept
{
    const auto cutoff = static_cast<float> (sampleRate) * (0.015f + 0.42f * (1.0f - params.reduce)) * (1.0f - 0.65f * params.antiAlias);
    preLeft.setLowPass (sampleRate, cutoff, 0.7071f);
    preRight.setLowPass (sampleRate, cutoff * 0.97f, 0.7071f);
}

float BitMangleEngine::quantize (float input, float& error, float phaseOffset) noexcept
{
    // Dither and grid motion are part of the distortion process, not a noise
    // generator. Keep an exactly silent/reset signal exactly silent so hosted
    // inserts and a disabled standalone audition source never self-excite.
    if (std::fabs (input) < 1.0e-12f && std::fabs (error) < 1.0e-12f)
    {
        error = 0.0f;
        return 0.0f;
    }

    const auto shaped = input + error * params.dither * 0.65f;
    const auto sign = shaped < 0.0f ? -1.0f : 1.0f;
    const auto magnitude = std::fabs (shaped);
    const auto curved = params.curve < 0.001f ? magnitude : std::log1p (magnitude * (1.0f + params.curve * 15.0f)) / std::log1p (1.0f + params.curve * 15.0f);
    const auto lsb = 2.0f / levels;
    const auto tpdf = (noise.nextFloat() - noise.nextFloat()) * 0.5f * lsb * params.dither;
    const auto movingGrid = std::sin (motionPhase + phaseOffset) * params.motion * lsb * 0.49f;
    const auto quantized = std::round ((sign * curved + tpdf + movingGrid) * levels) / levels;
    const auto expandedMag = params.curve < 0.001f ? std::fabs (quantized) : (std::exp (std::fabs (quantized) * std::log1p (1.0f + params.curve * 15.0f)) - 1.0f) / (1.0f + params.curve * 15.0f);
    const auto output = (quantized < 0.0f ? -expandedMag : expandedMag);
    error = std::clamp (shaped - output, -0.25f, 0.25f);
    return sanitizeAudio (output);
}

StereoFrame BitMangleEngine::processSample (float inputLeft, float inputRight) noexcept
{
    const auto dryLeft = sanitizeAudio (inputLeft);
    const auto dryRight = sanitizeAudio (inputRight);
    const auto filteredLeft = params.antiAlias > 0.0001f ? preLeft.process (dryLeft) : dryLeft;
    const auto filteredRight = params.antiAlias > 0.0001f ? preRight.process (dryRight) : dryRight;
    motionPhase += 0.00035f + params.motion * 0.004f;
    if (motionPhase > 6.2831853f) motionPhase -= 6.2831853f;
    if (holdCountdown <= 0)
    {
        heldLeft = quantize (filteredLeft, errorLeft, 0.0f);
        heldRight = quantize (filteredRight, errorRight, 1.5707963f);
        holdCountdown = holdSamples;
    }
    --holdCountdown;
    return sanitizeFrame (dryLeft * (1.0f - params.mix) + heldLeft * params.mix,
                          dryRight * (1.0f - params.mix) + heldRight * params.mix);
}

void BitMangleEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0) return;
    for (int i = 0; i < numSamples; ++i) { const auto f = processSample (left[i], right[i]); left[i] = f.left; right[i] = f.right; }
}

StereoFrame BitMangleEngine::sanitizeFrame (float left, float right) const noexcept
{
    auto l = std::clamp (softClip (left, 1.02f), -0.980f, 0.980f);
    auto r = std::clamp (softClip (right, 1.02f), -0.980f, 0.980f);
    if (std::fabs (l) < 1.0e-20f) l = 0.0f;
    if (std::fabs (r) < 1.0e-20f) r = 0.0f;
    return { l, r };
}

} // namespace bitmangle
