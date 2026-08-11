#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace bitmangle
{

struct StereoFrame
{
    float left = 0.0f;
    float right = 0.0f;
};

inline float clampFinite (float value, float low, float high, float fallback) noexcept
{
    if (! std::isfinite (value))
        value = fallback;
    return std::clamp (value, low, high);
}

inline float sanitizeAudio (float value) noexcept
{
    return clampFinite (value, -16.0f, 16.0f, 0.0f);
}

inline float softClip (float input, float drive = 1.0f) noexcept
{
    const auto safeDrive = clampFinite (drive, 0.0f, 32.0f, 1.0f);
    return std::tanh (sanitizeAudio (input) * safeDrive);
}

class OnePole
{
public:
    void reset (float value = 0.0f) noexcept { state = value; }
    [[nodiscard]] float process (float input, float coefficient) noexcept
    {
        const auto safeCoefficient = clampFinite (coefficient, 0.0f, 0.99999f, 0.0f);
        state = safeCoefficient * state + (1.0f - safeCoefficient) * sanitizeAudio (input);
        return state;
    }
    [[nodiscard]] float get() const noexcept { return state; }
private:
    float state = 0.0f;
};

class Biquad
{
public:
    void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }
    void setLowPass (double sampleRate, float frequency, float quality = 0.70710678f) noexcept
    {
        const auto safeRate = std::isfinite (sampleRate) && sampleRate >= 8000.0 ? sampleRate : 44100.0;
        const auto nyquist = static_cast<float> (safeRate * 0.5);
        const auto safeFrequency = clampFinite (frequency, 5.0f, nyquist * 0.92f, 1000.0f);
        const auto safeQuality = clampFinite (quality, 0.1f, 8.0f, 0.70710678f);
        const auto omega = 2.0f * std::numbers::pi_v<float> * safeFrequency / static_cast<float> (safeRate);
        const auto sine = std::sin (omega);
        const auto cosine = std::cos (omega);
        const auto alpha = sine / (2.0f * safeQuality);
        const auto inverseA0 = 1.0f / (1.0f + alpha);
        b0 = 0.5f * (1.0f - cosine) * inverseA0;
        b1 = (1.0f - cosine) * inverseA0;
        b2 = b0;
        a1 = -2.0f * cosine * inverseA0;
        a2 = (1.0f - alpha) * inverseA0;
    }
    [[nodiscard]] float process (float input) noexcept
    {
        const auto x = sanitizeAudio (input);
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return std::isfinite (y) ? y : 0.0f;
    }
private:
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
};

class DeterministicNoise
{
public:
    void reset (std::uint32_t seed) noexcept { state = seed != 0u ? seed : 0x6d2b79f5u; }
    [[nodiscard]] std::uint32_t nextWord() noexcept
    {
        auto x = state;
        x ^= x << 13u; x ^= x >> 17u; x ^= x << 5u;
        state = x != 0u ? x : 0x6d2b79f5u;
        return state;
    }
    [[nodiscard]] float nextFloat() noexcept
    {
        return static_cast<float> (static_cast<double> (nextWord()) / 2147483648.0 - 1.0);
    }
private:
    std::uint32_t state = 0x6d2b79f5u;
};

} // namespace bitmangle
