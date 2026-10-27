#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "EchoEngine.h"

namespace ParamIDs
{
    inline constexpr auto sync = "sync";
    inline constexpr auto timeNote = "timeNote";
    inline constexpr auto timeMs = "timeMs";
    inline constexpr auto feel = "feel";
    inline constexpr auto feedback = "feedback";
    inline constexpr auto groove = "groove";
    inline constexpr auto style = "style";
    inline constexpr auto mode = "mode";
    inline constexpr auto lowCut = "lowCut";
    inline constexpr auto highCut = "highCut";
    inline constexpr auto saturation = "saturation";
    inline constexpr auto wobble = "wobble";
    inline constexpr auto width = "width";
    inline constexpr auto duck = "duck";
    inline constexpr auto mix = "mix";
    inline constexpr auto output = "output";
}

namespace NoteValues
{
    // Lengths in quarter notes, matching the order of names().
    inline constexpr float beats[] = { 1.0f / 16, 1.0f / 12, 1.0f / 8, 3.0f / 16, 1.0f / 6, 1.0f / 4, 3.0f / 8,
                                       1.0f / 3, 1.0f / 2, 3.0f / 4, 2.0f / 3, 1.0f, 1.5f, 4.0f / 3, 2.0f, 3.0f, 4.0f };

    inline juce::StringArray names()
    {
        return { "1/64", "1/32T", "1/32", "1/32D", "1/16T", "1/16", "1/16D", "1/8T", "1/8",
                 "1/8D", "1/4T", "1/4", "1/4D", "1/2T", "1/2", "1/2D", "1/1" };
    }
}

class CruzEchoAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzEchoAudioProcessor();
    ~CruzEchoAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 20.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // For the display: the echo time in use right now, the host tempo and the echo level.
    float getEchoTimeMs() const noexcept { return echoTimeMs.load(); }
    float getHostBpm() const noexcept { return hostBpm.load(); }
    float getWetLevel() const noexcept { return wetLevel.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    cruzecho::Params readParams();

    juce::AudioProcessorValueTreeState state;
    cruzecho::EchoEngine engine;

    std::atomic<float>* sync = nullptr;
    std::atomic<float>* timeNote = nullptr;
    std::atomic<float>* timeMs = nullptr;
    std::atomic<float>* feel = nullptr;
    std::atomic<float>* feedback = nullptr;
    std::atomic<float>* groove = nullptr;
    std::atomic<float>* style = nullptr;
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* lowCut = nullptr;
    std::atomic<float>* highCut = nullptr;
    std::atomic<float>* saturation = nullptr;
    std::atomic<float>* wobble = nullptr;
    std::atomic<float>* width = nullptr;
    std::atomic<float>* duck = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* output = nullptr;

    std::atomic<float> echoTimeMs { 500.0f }, hostBpm { 120.0f }, wetLevel { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEchoAudioProcessor)
};
