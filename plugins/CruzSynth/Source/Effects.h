#pragma once

#include "Parameters.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace cs
{
// The effects rack. Modules run in a user-chosen order.
class FxRack
{
public:
    explicit FxRack (juce::AudioProcessorValueTreeState& state);

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, double bpm);

    // Order is a list of FxModule values. Safe to call from any thread.
    void setOrder (const std::array<int, kNumFx>& order);
    std::array<int, kNumFx> getOrder() const;
    static juce::String orderToString (const std::array<int, kNumFx>& order);
    static std::array<int, kNumFx> orderFromString (const juce::String& s);

private:
    struct Compressor
    {
        float env = 0.0f;
        float process (float x, float thr, float ratio, float att, float rel, float upward) noexcept;
    };

    void distortion (juce::AudioBuffer<float>& b);
    void chorus (juce::AudioBuffer<float>& b);
    void phaser (juce::AudioBuffer<float>& b);
    void delay (juce::AudioBuffer<float>& b, double bpm);
    void reverb (juce::AudioBuffer<float>& b);
    void compressor (juce::AudioBuffer<float>& b);
    void eq (juce::AudioBuffer<float>& b);

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<juce::uint32> packedOrder { 0 };
    double sampleRate = 48000.0;

    std::atomic<float>* p (const char* pid) { return apvts.getRawParameterValue (pid); }

    // distortion
    float distLp[2] {}, distHeld[2] {};
    int distCounter = 0;

    juce::dsp::Chorus<float> chorusFx;
    juce::dsp::Phaser<float> phaserFx;

    std::vector<float> delayBuf[2];
    int delayWrite = 0;
    float delaySmoothed = 0.0f, delayLp[2] {};

    juce::dsp::Reverb reverbFx;

    Compressor comp[3][2];
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit;

    juce::dsp::IIR::Filter<float> lowShelf[2], highShelf[2];
    float eqCache[4] { -1.0f, -1.0f, -1.0f, -1.0f };
};
} // namespace cs
