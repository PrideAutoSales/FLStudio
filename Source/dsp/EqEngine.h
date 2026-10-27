#pragma once

#include "FilterDesign.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cruz::dsp
{
// Which part of a stereo signal a band works on.
enum class Placement
{
    stereo,
    left,
    right,
    mid,
    side
};

inline constexpr int numPlacements = 5;

struct BandTarget
{
    bool active = false; // band is in use and not bypassed
    BandParams params;
    Placement placement = Placement::stereo;
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

        if (t.placement != b.target.placement)
            b.clearState();

        b.target = t;
    }

    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);
        const bool stereo = numChannels == 2;

        for (int start = 0; start < numSamples; start += subBlockSize)
        {
            const int n = std::min (subBlockSize, numSamples - start);
            updateCoefficients();

            std::array<double, subBlockSize> left, right;

            for (int i = 0; i < n; ++i)
            {
                left[(size_t) i] = channels[0][start + i];
                right[(size_t) i] = stereo ? channels[1][start + i] : 0.0;
            }

            for (auto& b : bands)
            {
                if (! b.target.active)
                    continue;

                // A mono signal is all mid: side bands do nothing, the rest filter it.
                if (! stereo)
                {
                    if (b.target.placement != Placement::side)
                        b.run (0, left.data(), n);
                    continue;
                }

                switch (b.target.placement)
                {
                    case Placement::stereo:
                        b.run (0, left.data(), n);
                        b.run (1, right.data(), n);
                        break;

                    case Placement::left:
                        b.run (0, left.data(), n);
                        break;

                    case Placement::right:
                        b.run (0, right.data(), n);
                        break;

                    case Placement::mid:
                    case Placement::side:
                    {
                        // Encode to mid/side, filter one of them, decode back.
                        std::array<double, subBlockSize> mid, side;
                        for (int i = 0; i < n; ++i)
                        {
                            mid[(size_t) i] = 0.5 * (left[(size_t) i] + right[(size_t) i]);
                            side[(size_t) i] = 0.5 * (left[(size_t) i] - right[(size_t) i]);
                        }

                        b.run (0, b.target.placement == Placement::mid ? mid.data() : side.data(), n);

                        for (int i = 0; i < n; ++i)
                        {
                            left[(size_t) i] = mid[(size_t) i] + side[(size_t) i];
                            right[(size_t) i] = mid[(size_t) i] - side[(size_t) i];
                        }
                        break;
                    }
                }
            }

            for (int i = 0; i < n; ++i)
            {
                channels[0][start + i] = (float) left[(size_t) i];
                if (stereo)
                    channels[1][start + i] = (float) right[(size_t) i];
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

        void run (int channel, double* buffer, int n) noexcept
        {
            for (int s = 0; s < design.numSections; ++s)
            {
                const auto& c = design.sections[(size_t) s];
                auto& st = state[(size_t) channel][(size_t) s];

                for (int i = 0; i < n; ++i)
                    buffer[i] = st.process (c, buffer[i]);
            }
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
// Level change of the whole EQ, as the average of its curve in dB over
// log-spaced points from 40 Hz to 16 kHz. Auto gain subtracts this so
// boosting or cutting does not change the overall loudness much.
template <size_t N>
double averageGainDb (const std::array<BandTarget, N>& bands, double sampleRate) noexcept
{
    constexpr int numPoints = 32;
    std::array<double, numPoints> sum {};

    for (const auto& b : bands)
    {
        if (! b.active)
            continue;

        const auto design = designBand (b.params, sampleRate);
        // A band on one channel or on mid or side moves the overall level about half as much.
        const double weight = b.placement == Placement::stereo ? 1.0 : 0.5;

        for (int i = 0; i < numPoints; ++i)
        {
            const double hz = 40.0 * std::pow (400.0, i / (numPoints - 1.0));
            sum[(size_t) i] += weight * design.magnitudeDb (std::min (2.0 * pi * hz / sampleRate, 0.9999 * pi));
        }
    }

    double total = 0.0;
    for (double v : sum)
        total += v;

    return total / numPoints;
}
} // namespace cruz::dsp
