#pragma once

#include "dsp/FilterDesign.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace Params
{
inline constexpr int numBands = 24;
inline constexpr auto outputGain = "outputGain";

enum class BandField
{
    used,
    bypass,
    freq,
    gain,
    q,
    shape,
    slope
};

juce::String bandId (int band, BandField field);
const juce::StringArray& shapeNames();
const juce::StringArray& slopeNames();
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

    static BandRefs fromState (juce::AudioProcessorValueTreeState& state, int band);

    bool isUsed() const noexcept { return used->load() >= 0.5f; }
    bool isActive() const noexcept { return isUsed() && bypass->load() < 0.5f; }
    cruz::dsp::BandParams read() const noexcept;
};
} // namespace Params
