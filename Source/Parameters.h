#pragma once

#include "dsp/EqEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
inline constexpr int numBands = 24;
inline constexpr auto outputGain = "outputGain";
inline constexpr auto autoGain = "autoGain";
inline constexpr auto gainScale = "gainScale";
inline constexpr auto phaseMode = "phaseMode";
inline constexpr auto resolution = "lpResolution";

enum class BandField
{
    used,
    bypass,
    freq,
    gain,
    q,
    shape,
    slope,
    place,
    dynamic,
    range,
    threshold,
    attack,
    release
};

juce::String bandId (int band, BandField field);
const juce::StringArray& shapeNames();
const juce::StringArray& slopeNames();
const juce::StringArray& placementNames();
const juce::StringArray& phaseModeNames();
const juce::StringArray& resolutionNames();
juce::String formatFrequency (double hz);

// The threshold's top position means "Auto": the threshold follows the band's
// own level, so a dynamic band works without setting one.
inline bool isAutoThreshold (float thresholdDb) noexcept { return thresholdDb >= (float) cruz::dsp::maxThresholdDb - 0.05f; }

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Lock-free views of one band's parameters, safe to read from any thread.
struct BandRefs
{
    std::atomic<float>* used = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* freq = nullptr;
    std::atomic<float>* gain = nullptr;
    std::atomic<float>* q = nullptr;
    std::atomic<float>* shape = nullptr;
    std::atomic<float>* slope = nullptr;
    std::atomic<float>* place = nullptr;
    std::atomic<float>* dynamic = nullptr;
    std::atomic<float>* range = nullptr;
    std::atomic<float>* threshold = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;

    static BandRefs fromState (juce::AudioProcessorValueTreeState& state, int band);

    bool isUsed() const noexcept { return used->load() >= 0.5f; }
    bool isActive() const noexcept { return isUsed() && bypass->load() < 0.5f; }
    cruz::dsp::BandParams read() const noexcept;
    cruz::dsp::Placement placement() const noexcept;
    cruz::dsp::DynamicParams dynamics() const noexcept;
};
} // namespace Params
