#include "PhaseModeRunner.h"

using namespace cruz::dsp;

PhaseModeRunner::PhaseModeRunner()
    : juce::Thread ("Cruz EQ kernel designer")
{
}

PhaseModeRunner::~PhaseModeRunner()
{
    stopThread (2000);
}

void PhaseModeRunner::prepare (double newSampleRate, int numChannels, PhaseMode newMode, int newResolution,
                               const Targets& targets, const DynamicGains& dynamicDb)
{
    stopThread (2000);

    sampleRate = newSampleRate;
    mode = newMode;
    resolution = newResolution;

    // Room for the longest kernel at this sample rate, so switching modes
    // later never allocates on the audio thread.
    const int maxPartitions = Convolver::partitionsFor (kernelLength (PhaseMode::linear, numLinearResolutions - 1, sampleRate));

    for (auto& slot : slots)
        slot.spectra.allocate (maxPartitions, Convolver::bins);

    convolver.prepare (maxPartitions, (int) (0.03 * sampleRate));
    convolver.setKernels (nullptr);
    activeSlot = -1;
    previousSlot = -1;
    pendingSlot = -1;

    request.targets = targets;
    request.dynamicDb = dynamicDb;
    request.mode = mode;
    request.resolution = resolution;
    request.sampleRate = sampleRate;
    request.mono = numChannels < 2;
    ++request.generation;
    lastDesigned = request;

    if (mode != PhaseMode::zeroLatency)
    {
        designInto (request, slots[0]);
        activeSlot = 0;
        convolver.setKernels (&slots[0].spectra);
    }

    fadeGain = 1.0f;
    fadeStep = 0.0f;
    startThread (juce::Thread::Priority::low);
}

void PhaseModeRunner::release()
{
    stopThread (2000);
}

void PhaseModeRunner::setMode (PhaseMode newMode, int newResolution) noexcept
{
    mode = newMode;
    resolution = newResolution;

    convolver.reset();
    convolver.setKernels (nullptr);
    activeSlot = -1;
    previousSlot = -1;
    pendingSlot = -1;

    fadeGain = 0.0f;
    fadeStep = 0.0f;
    redesignRequested = true;
}

int PhaseModeRunner::latencySamples() const noexcept
{
    if (mode == PhaseMode::zeroLatency)
        return 0;

    return kernelLatency (mode, kernelLength (mode, resolution, sampleRate), sampleRate) + Convolver::blockSize;
}

void PhaseModeRunner::process (float* const* channels, int numChannels, int numSamples,
                               const Targets& targets, const DynamicGains& dynamicDb) noexcept
{
    const bool mono = numChannels < 2;
    postRequest (targets, dynamicDb, mono);
    installPendingKernels (mono);
    convolver.process (channels, numChannels, numSamples);

    if (! convolver.isFading())
        previousSlot = -1;

    // Silent until the first kernels for a new mode arrive, then fade in.
    if (convolver.currentKernels() == nullptr)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            std::fill (channels[ch], channels[ch] + numSamples, 0.0f);
        return;
    }

    if (fadeGain < 1.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            fadeGain = std::min (1.0f, fadeGain + fadeStep);
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= fadeGain;
        }
    }
}

void PhaseModeRunner::postRequest (const Targets& targets, const DynamicGains& dynamicDb, bool mono) noexcept
{
    const juce::SpinLock::ScopedTryLockType lock (requestLock);

    if (! lock.isLocked())
        return;

    request.targets = targets;
    request.dynamicDb = dynamicDb;
    request.mode = mode;
    request.resolution = resolution;
    request.sampleRate = sampleRate;
    request.mono = mono;
    ++request.generation;
}

void PhaseModeRunner::installPendingKernels (bool mono) noexcept
{
    if (convolver.isFading())
        return;

    const int slot = pendingSlot.exchange (-1);
    if (slot < 0)
        return;

    // Kernels made for an earlier mode are dropped.
    const auto& s = slots[(size_t) slot];
    if (s.mode != mode || (mode == PhaseMode::linear && s.resolution != resolution) || s.mono != mono)
        return;

    const bool first = convolver.currentKernels() == nullptr;
    previousSlot = activeSlot.load();
    activeSlot = slot;
    convolver.setKernels (&s.spectra);

    if (first)
        fadeStep = 1.0f / (float) (0.01 * sampleRate);
}

void PhaseModeRunner::designInto (const Request& r, Slot& slot)
{
    designer.design (r.targets, r.dynamicDb, r.mode, r.resolution, r.sampleRate, kernels);
    Convolver::makeSpectra (kernels, r.mono, slot.spectra);
    slot.mode = r.mode;
    slot.resolution = r.resolution;
    slot.mono = r.mono;
}

bool PhaseModeRunner::sameDesign (const Request& a, const Request& b) noexcept
{
    if (a.mode != b.mode || a.mono != b.mono || ! exactlyEqual (a.sampleRate, b.sampleRate)
        || (a.mode == PhaseMode::linear && a.resolution != b.resolution))
        return false;

    for (size_t i = 0; i < a.targets.size(); ++i)
    {
        const auto& x = a.targets[i];
        const auto& y = b.targets[i];

        if (x.active != y.active)
            return false;

        if (! x.active)
            continue;

        if (x.placement != y.placement || x.isDynamic() != y.isDynamic() || x.params.shape != y.params.shape
            || x.params.slopeDbPerOct != y.params.slopeDbPerOct || ! exactlyEqual (x.params.freqHz, y.params.freqHz)
            || ! exactlyEqual (x.params.gainDb, y.params.gainDb) || ! exactlyEqual (x.params.q, y.params.q))
            return false;

        // Dynamic bands are redesigned once their gain has moved audibly.
        if (x.isDynamic() && std::abs (a.dynamicDb[i] - b.dynamicDb[i]) > 0.05)
            return false;
    }

    return true;
}

void PhaseModeRunner::run()
{
    Request local;
    juce::uint64 seen = 0;

    while (! threadShouldExit())
    {
        wait (10);

        // Wait until the audio thread has taken the last kernels.
        if (pendingSlot.load() >= 0)
            continue;

        {
            const juce::SpinLock::ScopedLockType lock (requestLock);
            if (request.generation == seen)
                continue;

            local = request;
        }

        seen = local.generation;

        // After a mode change the kernels are always rebuilt, even for a
        // setting designed before.
        const bool forced = redesignRequested.exchange (false);

        if (local.mode == PhaseMode::zeroLatency || (! forced && sameDesign (local, lastDesigned)))
            continue;

        int slot = -1;
        for (int i = 0; i < (int) slots.size() && slot < 0; ++i)
            if (i != activeSlot.load() && i != previousSlot.load())
                slot = i;

        if (slot < 0)
            continue;

        designInto (local, slots[(size_t) slot]);
        lastDesigned = local;

        int expected = -1;
        pendingSlot.compare_exchange_strong (expected, slot);
    }
}
