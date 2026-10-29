#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "SidechainEngine.h"

namespace ParamIDs
{
    inline constexpr auto trigger = "trigger";
    inline constexpr auto rate = "rate";
    inline constexpr auto detect = "detect";
    inline constexpr auto threshold = "threshold";
    inline constexpr auto ratio = "ratio";
    inline constexpr auto knee = "knee";
    inline constexpr auto range = "range";
    inline constexpr auto attack = "attack";
    inline constexpr auto hold = "hold";
    inline constexpr auto release = "release";
    inline constexpr auto attackBend = "attackBend";
    inline constexpr auto releaseBend = "releaseBend";
    inline constexpr auto lookahead = "lookahead";
    inline constexpr auto keyGain = "keyGain";
    inline constexpr auto keyHighPass = "keyHighPass";
    inline constexpr auto keyLowPass = "keyLowPass";
    inline constexpr auto splitLows = "splitLows";
    inline constexpr auto splitFreq = "splitFreq";
    inline constexpr auto listen = "listen";
    inline constexpr auto mix = "mix";
    inline constexpr auto output = "output";
}

enum class TriggerSource { sidechain, self, tempo };

namespace TempoRates
{
    // Length of each rate in quarter notes, matching the order of names().
    inline constexpr double beats[] = { 4.0, 2.0, 1.0, 0.5, 0.25 };
    inline juce::StringArray names() { return { "1/1", "1/2", "1/4", "1/8", "1/16" }; }
}

class CruzSidechainAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzSidechainAudioProcessor();
    ~CruzSidechainAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    // The engine settings as the knobs have them now (also used to draw curves).
    cruzsc::Params readParams() const;

    // For the display. popFrames copies up to maxFrames finished display
    // slices into dest and returns how many it copied.
    int popFrames (cruzsc::Frame* dest, int maxFrames);
    float getKeyLevelDb() const noexcept { return keyLevelDb.load(); }
    float getGainReductionDb() const noexcept { return grDb.load(); }
    float getHostBpm() const noexcept { return hostBpm.load(); }
    bool isSidechainConnected() const noexcept { return sidechainConnected.load(); }
    bool hasSidechainSignal() const noexcept { return sidechainSignal.load(); }
    bool isHostPlaying() const noexcept { return hostPlaying.load(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    void makeGhostKicks (int numSamples);

    juce::AudioProcessorValueTreeState state;
    cruzsc::SidechainEngine engine;
    cruzsc::GhostKick ghost;

    juce::AudioBuffer<float> keyScratch;
    double sampleRateNow = 48000.0;
    juce::int64 lastBeatIndex = -1;
    int silentSamples = 0;

    std::vector<cruzsc::Frame> frames;
    juce::AbstractFifo fifo { 1024 };

    std::atomic<float> keyLevelDb { -100.0f }, grDb { 0.0f }, hostBpm { 120.0f };
    std::atomic<bool> sidechainConnected { false }, sidechainSignal { false }, hostPlaying { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzSidechainAudioProcessor)
};
