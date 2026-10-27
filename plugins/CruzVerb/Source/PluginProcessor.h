#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ReverbEngine.h"

namespace ParamIDs
{
    inline constexpr auto space = "space";
    inline constexpr auto decay = "decay";
    inline constexpr auto predelay = "predelay";
    inline constexpr auto distance = "distance";
    inline constexpr auto brightness = "brightness";
    inline constexpr auto character = "character";
    inline constexpr auto width = "width";
    inline constexpr auto decayLowFreq = "decayLowFreq";
    inline constexpr auto decayLowMult = "decayLowMult";
    inline constexpr auto decayHighFreq = "decayHighFreq";
    inline constexpr auto decayHighMult = "decayHighMult";
    inline constexpr auto lowCut = "lowCut";
    inline constexpr auto highCut = "highCut";
    inline constexpr auto lowShelfFreq = "lowShelfFreq";
    inline constexpr auto lowShelfGain = "lowShelfGain";
    inline constexpr auto highShelfFreq = "highShelfFreq";
    inline constexpr auto highShelfGain = "highShelfGain";
    inline constexpr auto ducking = "ducking";
    inline constexpr auto freeze = "freeze";
    inline constexpr auto mix = "mix";
    inline constexpr auto outputGain = "outputGain";
}

// Single-producer, single-consumer buffer that carries the reverb output to the analyzer.
class AnalyzerFifo
{
public:
    static constexpr int capacity = 1 << 14;

    void push (const float* data, int num) noexcept
    {
        const auto scope = fifo.write (num);
        if (scope.blockSize1 > 0) std::copy_n (data, scope.blockSize1, buffer.data() + scope.startIndex1);
        if (scope.blockSize2 > 0) std::copy_n (data + scope.blockSize1, scope.blockSize2, buffer.data() + scope.startIndex2);
    }

    int pull (float* dest, int maxNum) noexcept
    {
        const auto scope = fifo.read (std::min (maxNum, fifo.getNumReady()));
        if (scope.blockSize1 > 0) std::copy_n (buffer.data() + scope.startIndex1, scope.blockSize1, dest);
        if (scope.blockSize2 > 0) std::copy_n (buffer.data() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> buffer {};
};

class CruzVerbAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzVerbAudioProcessor();
    ~CruzVerbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    AnalyzerFifo& getAnalyzerFifo() noexcept { return analyzerFifo; }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    cruzverb::ReverbSettings readSettings() const noexcept;
    float param (const char* id) const noexcept { return state.getRawParameterValue (id)->load(); }

    juce::AudioProcessorValueTreeState state;
    cruzverb::ReverbEngine engine;
    AnalyzerFifo analyzerFifo;
    std::atomic<double> currentSampleRate { 44100.0 };
    int currentProgram = 0;

    static constexpr int chunkSize = 256;
    std::array<float, chunkSize> wetL {}, wetR {}, monoIn {}, analyzerChunk {};
    juce::SmoothedValue<float> wetGain, dryGain, outputLevel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzVerbAudioProcessor)
};
