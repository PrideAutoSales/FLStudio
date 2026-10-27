#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "FetModel.h"

namespace ParamIDs
{
    inline constexpr auto input = "input";
    inline constexpr auto output = "output";
    inline constexpr auto attack = "attack";
    inline constexpr auto release = "release";
    inline constexpr auto ratio = "ratio";
    inline constexpr auto mix = "mix";
    inline constexpr auto meter = "meter";
}

class CruzFETAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzFETAudioProcessor();
    ~CruzFETAudioProcessor() override = default;

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

    // Read by the editor for the meter.
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> outputDb { -100.0f };

private:
    juce::AudioProcessorValueTreeState state;
    std::atomic<float>* input = nullptr;
    std::atomic<float>* output = nullptr;
    std::atomic<float>* attack = nullptr;
    std::atomic<float>* release = nullptr;
    std::atomic<float>* ratio = nullptr;
    std::atomic<float>* mix = nullptr;

    cruz::FetModel model;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DryWetMixer<float> dryWet { 64 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain { 1.0f }, outputGain { 1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzFETAudioProcessor)
};
