#pragma once

#include "bitmangle/BitMangleDspPrimitives.h"

namespace bitmangle
{

struct BitMangleParameters
{
    float bits = 8.0f;
    float reduce = 0.45f;
    float curve = 0.30f;
    float dither = 0.20f;
    float antiAlias = 0.45f;
    float motion = 0.35f;
    float mix = 1.0f;
};

class BitMangleEngine
{
public:
    BitMangleEngine();
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    void setParameters (const BitMangleParameters& parameters) noexcept;
    [[nodiscard]] StereoFrame processSample (float inputLeft, float inputRight) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

private:
    void updateFilters() noexcept;
    [[nodiscard]] float quantize (float input, float& error, float phaseOffset) noexcept;
    [[nodiscard]] StereoFrame sanitizeFrame (float left, float right) const noexcept;

    BitMangleParameters params;
    double sampleRate = 44100.0;
    int holdSamples = 1;
    int holdCountdown = 0;
    float levels = 255.0f;
    float heldLeft = 0.0f;
    float heldRight = 0.0f;
    float errorLeft = 0.0f;
    float errorRight = 0.0f;
    float motionPhase = 0.0f;
    Biquad preLeft;
    Biquad preRight;
    DeterministicNoise noise;
};

} // namespace bitmangle
