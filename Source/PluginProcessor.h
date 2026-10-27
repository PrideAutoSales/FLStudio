#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamIDs
{
    inline constexpr auto outputGain = "outputGain";
}

class CruzEQAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzEQAudioProcessor();
    ~CruzEQAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState state;
    std::atomic<float>* outputGainDb = nullptr;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessor)
};
