#pragma once

#include "AnalyzerFifo.h"
#include "Parameters.h"
#include "dsp/EqEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

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
    const Params::BandRefs& getBand (int index) const noexcept { return bands[(size_t) index]; }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }
    // Gain auto gain is applying right now, in dB (0 when it is off).
    float getAutoGainDb() const noexcept { return autoGainDb.load(); }

    AnalyzerFifo preEqFifo, postEqFifo;

private:
    juce::AudioProcessorValueTreeState state;
    std::array<Params::BandRefs, Params::numBands> bands;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* autoGainOn = nullptr;
    std::atomic<float>* gainScale = nullptr;
    std::array<cruz::dsp::BandTarget, Params::numBands> targets {};
    bool autoGainDirty = true;
    int autoGainCountdown = 0;
    double autoGainEstimateDb = 0.0;
    std::atomic<float> autoGainDb { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain { 1.0f };
    cruz::dsp::EqEngine eq;
    std::atomic<double> currentSampleRate { 48000.0 };

    void updateTargets() noexcept;
    float totalOutputGainDb (int numSamples) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessor)
};
