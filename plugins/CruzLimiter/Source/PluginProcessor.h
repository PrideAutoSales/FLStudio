#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "LimiterEngine.h"
#include "LoudnessMeter.h"

namespace ParamIDs
{
    inline constexpr auto gain = "gain";
    inline constexpr auto ceiling = "ceiling";
    inline constexpr auto style = "style";
    inline constexpr auto lookahead = "lookahead";
    inline constexpr auto attack = "attack";
    inline constexpr auto release = "release";
    inline constexpr auto linkTransients = "linkTransients";
    inline constexpr auto linkRelease = "linkRelease";
    inline constexpr auto oversampling = "oversampling";
    inline constexpr auto truePeak = "truePeak";
    inline constexpr auto dcFilter = "dcFilter";
    inline constexpr auto unityGain = "unityGain";
    inline constexpr auto audition = "audition";
    inline constexpr auto dither = "dither";
    inline constexpr auto noiseShaping = "noiseShaping";
}

/** One column of the scrolling display: levels in dBFS and gain reduction in dB. */
struct MeterFrame
{
    float inputDb = -100.0f;
    float outputDb = -100.0f;
    float reductionDb = 0.0f;
};

class CruzLimiterAudioProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int framesPerSecond = 100;

    CruzLimiterAudioProcessor();
    ~CruzLimiterAudioProcessor() override = default;

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

    //==============================================================================
    // Metering, read by the editor on the message thread.

    /** Copies up to maxFrames new display frames into dest and returns how many were copied. */
    int readFrames (MeterFrame* dest, int maxFrames);

    /** Highest output sample level (linear) per channel since the last call. */
    float takeOutputPeak (int channel) noexcept { return outputPeaks[(size_t) juce::jlimit (0, 1, channel)].exchange (0.0f); }
    /** Largest gain reduction (dB) since the last call. */
    float takeReduction() noexcept { return reductionPeak.exchange (0.0f); }

    float getTruePeakMaxDb() const noexcept { return juce::Decibels::gainToDecibels (truePeakMax.load(), -100.0f); }
    const cruz::LoudnessMeter& getLoudness() const noexcept { return loudness; }
    void resetMeters() noexcept;

private:
    cruz::LimiterSettings readSettings() const;
    void pushFrame() noexcept;

    juce::AudioProcessorValueTreeState state;

    struct RawParams
    {
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* ceiling = nullptr;
        std::atomic<float>* style = nullptr;
        std::atomic<float>* lookahead = nullptr;
        std::atomic<float>* attack = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* linkTransients = nullptr;
        std::atomic<float>* linkRelease = nullptr;
        std::atomic<float>* oversampling = nullptr;
        std::atomic<float>* truePeak = nullptr;
        std::atomic<float>* dcFilter = nullptr;
        std::atomic<float>* unityGain = nullptr;
        std::atomic<float>* audition = nullptr;
        std::atomic<float>* dither = nullptr;
        std::atomic<float>* noiseShaping = nullptr;
    } params;

    cruz::LimiterEngine engine;
    cruz::LoudnessMeter loudness;
    std::array<cruz::TruePeakEstimator, 2> truePeakMeters;
    int blockLimit = 512;

    // Display frames, handed to the editor through a lock-free FIFO.
    static constexpr int fifoSize = 1024;
    juce::AbstractFifo fifo { fifoSize };
    std::array<MeterFrame, fifoSize> frames;
    int frameLength = 480, frameCount = 0;
    float frameInput = 0.0f, frameOutput = 0.0f, frameReduction = 0.0f;

    std::array<std::atomic<float>, 2> outputPeaks {};
    std::atomic<float> reductionPeak { 0.0f }, truePeakMax { 0.0f };
    std::atomic<bool> meterResetRequested { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzLimiterAudioProcessor)
};
