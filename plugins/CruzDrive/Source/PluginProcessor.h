#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "DriveEngine.h"

namespace DriveParams
{
    inline constexpr auto drive = "drive";
    inline constexpr auto style = "style";
    inline constexpr auto lowCut = "lowCut";
    inline constexpr auto thump = "thump";
    inline constexpr auto tone = "tone";
    inline constexpr auto highCut = "highCut";
    inline constexpr auto steep = "steep";
    inline constexpr auto punish = "punish";
    inline constexpr auto crush = "crush";
    inline constexpr auto darkness = "darkness";
    inline constexpr auto autoGain = "autoGain";
    inline constexpr auto mix = "mix";
    inline constexpr auto output = "output";
}

class CruzDriveProcessor final : public juce::AudioProcessor
{
public:
    CruzDriveProcessor();
    ~CruzDriveProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
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

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }
    const cruzdrive::Engine& getEngine() const noexcept { return engine; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    cruzdrive::Settings readSettings() const;

    juce::AudioProcessorValueTreeState state;
    cruzdrive::Engine engine;
    int currentProgram = 0;

    struct Raw
    {
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* style = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* thump = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* steep = nullptr;
        std::atomic<float>* punish = nullptr;
        std::atomic<float>* crush = nullptr;
        std::atomic<float>* darkness = nullptr;
        std::atomic<float>* autoGain = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
    } raw;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzDriveProcessor)
};
