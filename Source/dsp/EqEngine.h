#pragma once

#include "FilterDesign.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cruz::dsp
{
struct BandTarget
{
    bool active = false; // band is in use and not bypassed
    BandParams params;
};

// The zero-latency EQ: up to 24 bands of cascaded biquads, run in double
// precision. Parameter changes glide over a few milliseconds and coefficients
// are recalculated every 32 samples while a band is moving.
class EqEngine
{
public:
    static constexpr int numBands = 24;
    static constexpr int maxChannels = 2;
    static constexpr int subBlockSize = 32;
    static constexpr double smoothingTimeSeconds = 0.015;

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        smoothing = 1.0 - std::exp (-subBlockSize / (smoothingTimeSeconds * sampleRate));

        for (auto& b : bands)
            b.snap = true;

        reset();
    }

    void reset() noexcept
    {
        for (auto& b : bands)
            b.clearState();
    }

    void setBand (int index, const BandTarget& t) noexcept
    {
        auto& b = bands[(size_t) index];

        if (t.active && ! b.target.active)
        {
            b.snap = true;
            b.clearState();
        }

        if (t.params.shape != b.target.params.shape || t.params.slopeDbPerOct != b.target.params.slopeDbPerOct)
            b.redesign = true;

        b.target = t;
    }

    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);

        for (int start = 0; start < numSamples; start += subBlockSize)
        {
            const int n = std::min (subBlockSize, numSamples - start);
            updateCoefficients();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                std::array<double, subBlockSize> buffer;
                float* data = channels[ch] + start;

                for (int i = 0; i < n; ++i)
                    buffer[(size_t) i] = data[i];

                for (auto& b : bands)
                {
                    if (! b.target.active)
                        continue;

                    for (int s = 0; s < b.design.numSections; ++s)
                    {
                        const auto& c = b.design.sections[(size_t) s];
                        auto& state = b.state[(size_t) ch][(size_t) s];

                        for (int i = 0; i < n; ++i)
                            buffer[(size_t) i] = state.process (c, buffer[(size_t) i]);
                    }
                }

                for (int i = 0; i < n; ++i)
                    data[i] = (float) buffer[(size_t) i];
            }
        }
    }

    // The design currently in use for a band (for tests and diagnostics).
    const BandDesign& currentDesign (int index) const noexcept { return bands[(size_t) index].design; }

private:
    struct Band
    {
        BandTarget target;
        double logFreq = std::log (1000.0), gainDb = 0.0, logQ = 0.0;
        bool snap = true, redesign = true;
        BandDesign design;
        std::array<std::array<BiquadState, maxSections>, maxChannels> state {};

        void clearState() noexcept
        {
            for (auto& ch : state)
                ch.fill ({});
        }
    };

    static bool glide (double& value, double target, double amount, double epsilon) noexcept
    {
        if (exactlyEqual (value, target))
            return false;

        value += (target - value) * amount;

        if (std::abs (target - value) < epsilon)
            value = target;

        return true;
    }

    void updateCoefficients() noexcept
    {
        for (auto& b : bands)
        {
            if (! b.target.active)
                continue;

            const auto& p = b.target.params;
            const double targetLogFreq = std::log (std::clamp (p.freqHz, minFreqHz, maxFreqHz));
            const double targetLogQ = std::log (std::clamp (p.q, minQ, maxQ));
            bool changed = b.redesign;

            if (b.snap)
            {
                b.logFreq = targetLogFreq;
                b.gainDb = p.gainDb;
                b.logQ = targetLogQ;
                b.snap = false;
                changed = true;
            }
            else
            {
                changed |= glide (b.logFreq, targetLogFreq, smoothing, 1.0e-4);
                changed |= glide (b.gainDb, p.gainDb, smoothing, 1.0e-3);
                changed |= glide (b.logQ, targetLogQ, smoothing, 1.0e-4);
            }

            if (changed)
            {
                const BandParams current { p.shape, std::exp (b.logFreq), b.gainDb, std::exp (b.logQ), p.slopeDbPerOct };
                const int previousSections = b.design.numSections;
                b.design = designBand (current, sampleRate);
                b.redesign = false;

                // Sections that were idle until now start from silence.
                for (auto& ch : b.state)
                    for (int s = previousSections; s < b.design.numSections; ++s)
                        ch[(size_t) s] = {};
            }
        }
    }

    double sampleRate = 48000.0;
    double smoothing = 1.0;
    std::array<Band, numBands> bands {};
};
} // namespace cruz::dsp
