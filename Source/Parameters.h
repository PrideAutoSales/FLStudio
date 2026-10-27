#pragma once

#include "dsp/EqEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
inline constexpr int numBands = 24;
inline constexpr auto outputGain = "outputGain";
inline constexpr auto autoGain = "autoGain";
inline constexpr auto gainScale = "gainScale";

enum class BandField
{
    used,
    bypass,
    freq,
    gain,
    q,
    shape,
    slope,
    place
};

juce::String bandId (int band, BandField field);
const juce::StringArray& shapeNames();
const juce::StringArray& slopeNames();
const juce::StringArray& placementNames();
juce::String formatFrequency (double hz);

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

    static BandRefs fromState (juce::AudioProcessorValueTreeState& state, int band);

    bool isUsed() const noexcept { return used->load() >= 0.5f; }
    bool isActive() const noexcept { return isUsed() && bypass->load() < 0.5f; }
    cruz::dsp::BandParams read() const noexcept;
    cruz::dsp::Placement placement() const noexcept;
};
} // namespace Params
