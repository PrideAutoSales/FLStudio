#pragma once

#include "dsp/Convolver.h"

#include <juce_core/juce_core.h>

// Runs the natural phase and linear phase modes. A background thread turns
// the current band settings into kernels whenever they change (including
// dynamic bands moving), and the audio thread convolves with the newest
// kernels, crossfading from the previous ones.
class PhaseModeRunner final : private juce::Thread
{
public:
    static constexpr int numBands = cruz::dsp::EqEngine::numBands;
    using Targets = std::array<cruz::dsp::BandTarget, numBands>;
    using DynamicGains = std::array<double, numBands>;

    PhaseModeRunner();
    ~PhaseModeRunner() override;

    // Not real-time safe. Allocates for the sample rate and, in a kernel
    // mode, builds the first kernels straight away.
    void prepare (double sampleRate, int numChannels, cruz::dsp::PhaseMode, int resolution,
                  const Targets&, const DynamicGains&);
    void release();

    // Audio thread. Switching mode mutes the output until the first kernels
    // for the new mode are ready, then fades it back in.
    void setMode (cruz::dsp::PhaseMode, int resolution) noexcept;
    cruz::dsp::PhaseMode getMode() const noexcept { return mode; }
    int getResolution() const noexcept { return resolution; }

    // Audio thread. Filters the channels in place with the current kernels.
    void process (float* const* channels, int numChannels, int numSamples, const Targets&, const DynamicGains&) noexcept;

    // Total delay of the current mode, in samples.
    int latencySamples() const noexcept;

private:
    struct Request
    {
        Targets targets {};
        DynamicGains dynamicDb {};
        cruz::dsp::PhaseMode mode = cruz::dsp::PhaseMode::zeroLatency;
        int resolution = 1;
        double sampleRate = 48000.0;
        bool mono = false;
        juce::uint64 generation = 0;
    };

    struct Slot
    {
        cruz::dsp::KernelSpectra spectra;
        cruz::dsp::PhaseMode mode = cruz::dsp::PhaseMode::zeroLatency;
        int resolution = 0;
        bool mono = false;
    };

    void run() override;
    void designInto (const Request&, Slot&);
    static bool sameDesign (const Request&, const Request&) noexcept;
    void postRequest (const Targets&, const DynamicGains&, bool mono) noexcept;
    void installPendingKernels (bool mono) noexcept;

    cruz::dsp::KernelDesigner designer;
    cruz::dsp::EqKernels kernels;
    cruz::dsp::Convolver convolver;
    std::array<Slot, 3> slots;

    // Slot indices: the audio thread owns active and previous; the designer
    // fills a slot that is neither and hands it over through pending.
    std::atomic<int> activeSlot { -1 }, previousSlot { -1 }, pendingSlot { -1 };

    juce::SpinLock requestLock;
    Request request, lastDesigned;
    std::atomic<bool> redesignRequested { false };

    cruz::dsp::PhaseMode mode = cruz::dsp::PhaseMode::zeroLatency;
    int resolution = 1;
    double sampleRate = 48000.0;
    float fadeGain = 1.0f, fadeStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaseModeRunner)
};
