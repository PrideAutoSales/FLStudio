#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "TapeDSP.h"

namespace ParamIDs
{
    inline constexpr auto input = "input";
    inline constexpr auto output = "output";
    inline constexpr auto autoGain = "autoGain";
    inline constexpr auto speed = "speed";
    inline constexpr auto formula = "formula";
    inline constexpr auto calibration = "calibration";
    inline constexpr auto emphasis = "emphasis";
    inline constexpr auto bias = "bias";
    inline constexpr auto wow = "wow";
    inline constexpr auto flutter = "flutter";
    inline constexpr auto hiss = "hiss";
    inline constexpr auto mix = "mix";
}

class CruzTapeAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzTapeAudioProcessor();
    ~CruzTapeAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Output level (RMS, linear) of the last block, for the VU meters.
    float getOutputLevel (int channel) const noexcept { return outputLevel[(size_t) juce::jlimit (0, 1, channel)].load(); }

    // Lets the editor spin the reels only while audio is running.
    juce::uint32 getLastProcessTime() const noexcept { return lastProcessTime.load(); }

private:
    cruztape::Settings readSettings() const;
    void processChunk (juce::AudioBuffer<float>& buffer, int start, int numSamples);

    juce::AudioProcessorValueTreeState state;

    struct RawParams
    {
        std::atomic<float>* input = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* autoGain = nullptr;
        std::atomic<float>* speed = nullptr;
        std::atomic<float>* formula = nullptr;
        std::atomic<float>* calibration = nullptr;
        std::atomic<float>* emphasis = nullptr;
        std::atomic<float>* bias = nullptr;
        std::atomic<float>* wow = nullptr;
        std::atomic<float>* flutter = nullptr;
        std::atomic<float>* hiss = nullptr;
        std::atomic<float>* mix = nullptr;
    } raw;

    std::array<cruztape::TapeChannel, 2> channels { cruztape::TapeChannel { 0x1234567u }, cruztape::TapeChannel { 0x7654321u } };
    cruztape::Transport transport;
    std::array<cruztape::ModDelay, 2> wowDelays;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // The dry signal is delayed by the plugin's latency so Mix stays in time.
    juce::AudioBuffer<float> dryDelay;
    int dryWritePos = 0;
    int latency = 0;

    juce::AudioBuffer<float> wetBuffer;
    int maxBlockSize = 0;

    cruztape::Settings lastSettings;
    bool needsConfigure = true;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain { 1.0f }, outputGain { 1.0f };
    juce::SmoothedValue<float> mixAmount { 1.0f }, hissGain { 0.0f }, wowAmount { 0.0f }, flutterAmount { 0.0f };

    std::array<std::atomic<float>, 2> outputLevel {};
    std::atomic<juce::uint32> lastProcessTime { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzTapeAudioProcessor)
};
