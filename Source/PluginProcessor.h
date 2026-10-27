#pragma once

#include "AnalyzerFifo.h"
#include "Parameters.h"
#include "PhaseModeRunner.h"
#include "dsp/EqEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class CruzEQAudioProcessor final : public juce::AudioProcessor,
                                   private juce::AsyncUpdater
{
public:
    CruzEQAudioProcessor();
    ~CruzEQAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
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
    // How far a dynamic band is moving its gain right now, in dB.
    float getDynamicGainDb (int band) const noexcept { return dynamicGainDb[(size_t) band].load(); }

    AnalyzerFifo preEqFifo, postEqFifo;

private:
    juce::AudioProcessorValueTreeState state;
    std::array<Params::BandRefs, Params::numBands> bands;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* autoGainOn = nullptr;
    std::atomic<float>* gainScale = nullptr;
    std::atomic<float>* phaseModeParam = nullptr;
    std::atomic<float>* resolutionParam = nullptr;
    std::array<cruz::dsp::BandTarget, Params::numBands> targets {};
    bool autoGainDirty = true;
    int autoGainCountdown = 0;
    double autoGainEstimateDb = 0.0;
    std::atomic<float> autoGainDb { 0.0f };
    std::array<std::atomic<float>, Params::numBands> dynamicGainDb {};
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outputGain { 1.0f };
    cruz::dsp::EqEngine eq;
    PhaseModeRunner phaseModes;
    PhaseModeRunner::DynamicGains currentDynamicDb {};
    std::atomic<int> pendingLatency { 0 };
    std::atomic<double> currentSampleRate { 48000.0 };

    void handleAsyncUpdate() override;
    cruz::dsp::PhaseMode requestedPhaseMode() const noexcept;
    int requestedResolution() const noexcept;
    void updateTargets() noexcept;
    float totalOutputGainDb (int numSamples) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessor)
};
