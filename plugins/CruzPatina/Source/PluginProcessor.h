#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <map>

#include "PatinaDSP.h"

namespace ParamIDs
{
    inline constexpr auto magnitude = "magnitude";
    inline constexpr auto inputGain = "inputGain";
    inline constexpr auto outputGain = "outputGain";
    inline constexpr auto mix = "mix";
    inline constexpr auto tone = "tone";
    inline constexpr auto width = "width";

    inline constexpr auto noiseOn = "noiseOn";
    inline constexpr auto noiseAmount = "noiseAmount";
    inline constexpr auto noiseType = "noiseType";
    inline constexpr auto noiseTone = "noiseTone";
    inline constexpr auto noiseFollow = "noiseFollow";

    inline constexpr auto wobbleOn = "wobbleOn";
    inline constexpr auto wobbleWow = "wobbleWow";
    inline constexpr auto wobbleRate = "wobbleRate";
    inline constexpr auto wobbleFlutter = "wobbleFlutter";
    inline constexpr auto wobbleStereo = "wobbleStereo";

    inline constexpr auto distortOn = "distortOn";
    inline constexpr auto distortDrive = "distortDrive";
    inline constexpr auto distortType = "distortType";
    inline constexpr auto distortTone = "distortTone";
    inline constexpr auto distortMix = "distortMix";

    inline constexpr auto digitalOn = "digitalOn";
    inline constexpr auto digitalMix = "digitalMix";
    inline constexpr auto digitalRate = "digitalRate";
    inline constexpr auto digitalBits = "digitalBits";

    inline constexpr auto spaceOn = "spaceOn";
    inline constexpr auto spaceMix = "spaceMix";
    inline constexpr auto spaceSize = "spaceSize";
    inline constexpr auto spaceDecay = "spaceDecay";

    inline constexpr auto magneticOn = "magneticOn";
    inline constexpr auto magneticSaturation = "magneticSaturation";
    inline constexpr auto magneticWear = "magneticWear";
    inline constexpr auto magneticDropouts = "magneticDropouts";
}

class CruzPatinaAudioProcessor final : public juce::AudioProcessor
{
public:
    CruzPatinaAudioProcessor();
    ~CruzPatinaAudioProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getState() noexcept { return state; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::StringArray getNoiseTypeNames() { return { "Hiss", "Vinyl", "Hum" }; }
    static juce::StringArray getDistortTypeNames() { return { "Tube", "Tape", "Fuzz", "Hard" }; }

private:
    float get (const char* id) const noexcept { return params.at (id)->load(); }
    bool isOn (const char* id) const noexcept { return get (id) >= 0.5f; }
    void processChunk (float* const* channels, int numChannels, int numSamples);
    void updateTiltFilters();

    static constexpr int maxChunk = 512;

    juce::AudioProcessorValueTreeState state;
    std::map<juce::String, std::atomic<float>*> params;
    int currentProgram = 0;

    double sampleRate = 44100.0;
    int numActiveChannels = 2;

    patina::NoiseModule noise;
    patina::WobbleModule wobble;
    patina::DistortModule distort;
    patina::DigitalModule digital;
    patina::SpaceModule space;
    patina::MagneticModule magnetic;
    patina::IntegerDelay dryDelay;

    std::array<patina::Svf, patina::maxChannels> tiltLow, tiltHigh;
    float lastTilt = 1000.0f;

    juce::AudioBuffer<float> dryBuffer;
    patina::Smoothed inputGainSmoothed, outputGainSmoothed, mixSmoothed, widthSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzPatinaAudioProcessor)
};
