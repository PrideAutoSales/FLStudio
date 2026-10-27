#pragma once

#include <juce_dsp/juce_dsp.h>

#include "LimiterDSP.h"
#include "Oversampler.h"

namespace cruz
{

/** Limiting characters. Each one changes how the gain recovers and how much soft clipping is used. */
enum class LimiterStyle
{
    clean,
    punch,
    dynamic,
    allround,
    loud,
    modern,
    safe,
    bus
};

inline const juce::StringArray& getStyleNames()
{
    static const juce::StringArray names { "Clean", "Punch", "Dynamic", "Allround", "Loud", "Modern", "Safe", "Bus" };
    return names;
}

inline const juce::StringArray& getOversamplingNames()
{
    static const juce::StringArray names { "Off", "2x", "4x", "8x", "16x" };
    return names;
}

struct LimiterSettings
{
    float inputGainDb = 0.0f;
    float ceilingDb = -1.0f;
    float lookaheadMs = 1.0f;
    float attackPercent = 60.0f;
    float releaseMs = 100.0f;
    float linkTransientsPercent = 75.0f;
    float linkReleasePercent = 100.0f;
    LimiterStyle style = LimiterStyle::allround;
    int oversamplingIndex = 1;   // 0 = off, 1 = 2x ... 4 = 16x (true peak runs at least 2x)
    bool truePeak = true;
    bool dcFilter = false;
    bool unityGain = false;
    bool audition = false;
    int ditherBits = 0;          // 0 = off
    bool noiseShaping = true;
};

/**
    The complete limiter: input gain, optional DC filter, oversampled lookahead
    limiter with style-dependent release and soft clipping, a true-peak safety
    stage, output ceiling, delta monitoring and dither.
*/
class LimiterEngine
{
public:
    static constexpr int maxChannels = 2;
    static constexpr float maxLookaheadMs = 5.0f;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    /** Call before process() whenever parameters may have changed. Safe on the audio thread. */
    void setSettings (const LimiterSettings& settings);

    /** Total delay of the processed signal, in samples at the host rate. */
    int getLatencySamples() const noexcept { return latency; }

    void process (juce::AudioBuffer<float>& buffer);

    /** Per-sample gain reduction (dB) and gained input level from the last process() call. */
    const float* getGainReduction() const noexcept { return grTrace.data(); }
    const float* getInputLevel() const noexcept    { return inputTrace.data(); }

private:
    struct StyleShape
    {
        float fastReleaseScale, slowAttackMs, slowReleaseScale, slowAmount, softClip;
    };

    static StyleShape shapeFor (LimiterStyle style) noexcept;

    void processChunk (float* const* channels, int numChannels, int numSamples);
    void applyStructure();
    void applyTiming();

    double sampleRate = 44100.0;
    int maxBlock = 512, channelCount = 2;

    std::array<Oversampler, 5> oversamplers;
    bool prepared = false;
    LimiterStage mainStage, truePeakStage;
    std::array<TruePeakEstimator, maxChannels> truePeakEstimators;
    std::array<DelayLine, maxChannels> truePeakAlign, dryDelay;
    std::array<float, maxChannels> previousTruePeak {};
    std::array<DcBlocker, maxChannels> dcBlockers;
    std::array<Ditherer, maxChannels> ditherers;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> drive { 1.0f }, outputScale { 1.0f };

    juce::AudioBuffer<float> dryBuffer, truePeakDetector;
    std::vector<float> grTrace, inputTrace, oversampledGr, truePeakGr, driveRamp, outputRamp;

    LimiterSettings current;
    bool structureDirty = true;
    int activeOversampling = 1, lookaheadSamples = 0, truePeakLookahead = 0, latency = 0;
};

} // namespace cruz
