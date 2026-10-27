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

// Dynamic EQ: the band's gain moves by up to rangeDb while the input around
// the band's frequency is above the threshold.
struct DynamicParams
{
    bool enabled = false;
    double rangeDb = -6.0; // negative cuts when loud, positive boosts when loud
    double thresholdDb = -24.0;
    double attackMs = 10.0;
    double releaseMs = 150.0;
};

inline constexpr double minDynamicRangeDb = -30.0, maxDynamicRangeDb = 30.0;
inline constexpr double minThresholdDb = -60.0, maxThresholdDb = 0.0;
inline constexpr double minAttackMs = 1.0, maxAttackMs = 200.0;
inline constexpr double minReleaseMs = 10.0, maxReleaseMs = 2000.0;
inline constexpr double dynamicKneeDb = 6.0;

inline bool shapeUsesDynamics (Shape s) noexcept { return shapeUsesGain (s); }

// How far the band's gain moves for a detected level, in dB. Above the
// threshold the gain follows the level dB for dB (so a cutting band holds its
// part of the sound at the threshold) until the range is used up, with a soft
// knee around the threshold.
inline double dynamicGainDb (const DynamicParams& d, double levelDb) noexcept
{
    const double over = levelDb - d.thresholdDb;
    constexpr double halfKnee = 0.5 * dynamicKneeDb;
    double amount = 0.0;

    if (over >= halfKnee)
        amount = over;
    else if (over > -halfKnee)
        amount = (over + halfKnee) * (over + halfKnee) / (2.0 * dynamicKneeDb);

    return d.rangeDb < 0.0 ? std::max (d.rangeDb, -amount) : std::min (d.rangeDb, amount);
}

// The filter that picks out the part of the input a dynamic band listens to:
// a band pass for bells, low pass for low shelves, high pass for high shelves,
// and the whole signal for tilts.
inline BiquadCoeffs detectorFilter (Shape shape, double freqHz, double q, double sampleRate) noexcept
{
    if (shape != Shape::bell && shape != Shape::lowShelf && shape != Shape::highShelf)
        return {};

    const double w0 = 2.0 * pi * std::min (freqHz, 0.45 * sampleRate) / sampleRate;
    const double alpha = std::sin (w0) / (2.0 * (shape == Shape::bell ? std::clamp (q, 0.1, 40.0) : 0.7071));
    const double cw = std::cos (w0);
    const double a0 = 1.0 + alpha;
    BiquadCoeffs c;
    c.a1 = -2.0 * cw / a0;
    c.a2 = (1.0 - alpha) / a0;

    if (shape == Shape::bell)
    {
        c.b0 = alpha / a0;
        c.b1 = 0.0;
        c.b2 = -alpha / a0;
    }
    else if (shape == Shape::lowShelf)
    {
        c.b0 = c.b2 = 0.5 * (1.0 - cw) / a0;
        c.b1 = (1.0 - cw) / a0;
    }
    else
    {
        c.b0 = c.b2 = 0.5 * (1.0 + cw) / a0;
        c.b1 = -(1.0 + cw) / a0;
    }

    return c;
}

struct BandTarget
{
    bool active = false; // band is in use and not bypassed
    BandParams params;
    Placement placement = Placement::stereo;
    DynamicParams dynamics;

    bool isDynamic() const noexcept { return dynamics.enabled && shapeUsesDynamics (params.shape); }
};

// The zero-latency EQ: up to 24 bands of cascaded biquads, run in double
// precision. Parameter changes glide over a few milliseconds and coefficients
// are recalculated every 32 samples while a band is moving. Dynamic bands
// listen to the plugin's input (not to the bands before them) and recalculate
// their gain every 32 samples too.
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

        if (t.isDynamic() != b.target.isDynamic())
        {
            b.envelope = 0.0;
            b.detectorState.fill ({});
            b.dirty = true;
        }

        b.target = t;
    }

    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);
        const bool stereo = numChannels == 2;

        for (int start = 0; start < numSamples; start += subBlockSize)
        {
            const int n = std::min (subBlockSize, numSamples - start);
            updateTargets();

            std::array<double, subBlockSize> left, right, inLeft, inRight, inMid, inSide;

            for (int i = 0; i < n; ++i)
            {
                const auto k = (size_t) i;
                left[k] = inLeft[k] = channels[0][start + i];
                right[k] = inRight[k] = stereo ? channels[1][start + i] : 0.0;
                inMid[k] = 0.5 * (inLeft[k] + inRight[k]);
                inSide[k] = 0.5 * (inLeft[k] - inRight[k]);
            }

            for (auto& b : bands)
            {
                if (! b.target.active)
                    continue;

                // A mono signal is all mid: side bands do nothing, the rest filter it.
                if (! stereo)
                {
                    if (b.target.placement != Placement::side)
                    {
                        double* io[] { left.data() };
                        const double* in[] { inLeft.data() };
                        b.process (io, in, 1, n, sampleRate);
                    }
                    continue;
                }

                switch (b.target.placement)
                {
                    case Placement::stereo:
                    {
                        double* io[] { left.data(), right.data() };
                        const double* in[] { inLeft.data(), inRight.data() };
                        b.process (io, in, 2, n, sampleRate);
                        break;
                    }

                    case Placement::left:
                    case Placement::right:
                    {
                        const bool isLeft = b.target.placement == Placement::left;
                        double* io[] { isLeft ? left.data() : right.data() };
                        const double* in[] { isLeft ? inLeft.data() : inRight.data() };
                        b.process (io, in, 1, n, sampleRate);
                        break;
                    }

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

                        const bool isMid = b.target.placement == Placement::mid;
                        double* io[] { isMid ? mid.data() : side.data() };
                        const double* in[] { isMid ? inMid.data() : inSide.data() };
                        b.process (io, in, 1, n, sampleRate);

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

    // How far a dynamic band has moved its gain right now, in dB.
    double currentDynamicGainDb (int index) const noexcept { return bands[(size_t) index].appliedDynamicDb; }

    // The design currently in use for a band (for tests and diagnostics).
    const BandDesign& currentDesign (int index) const noexcept { return bands[(size_t) index].design; }

private:
    struct Band
    {
        BandTarget target;
        double logFreq = std::log (1000.0), gainDb = 0.0, logQ = 0.0;
        bool snap = true, redesign = true, dirty = true;
        BandDesign design;
        std::array<std::array<BiquadState, maxSections>, maxChannels> state {};

        // Dynamic EQ
        BiquadCoeffs detector;
        std::array<BiquadState, maxChannels> detectorState {};
        double envelope = 0.0;
        double dynamicDb = 0.0;        // smoothed gain move, updated every 32 samples
        double appliedDynamicDb = 0.0; // the gain move the coefficients were made with

        void clearState() noexcept
        {
            for (auto& ch : state)
                ch.fill ({});

            detectorState.fill ({});
            envelope = 0.0;
        }

        // Filters `numChannels` buffers in place. `inputs` is the matching
        // part of the plugin's input, which dynamic bands listen to.
        void process (double* const* buffers, const double* const* inputs, int numChannels, int n, double sampleRate) noexcept
        {
            double targetDb = 0.0;

            if (target.isDynamic())
            {
                // A peak detector that falls back at the release speed reads
                // the level; the gain then moves towards its new value at the
                // attack speed (more action) or release speed (less action).
                const auto& d = target.dynamics;
                const double attackSeconds = 0.001 * std::clamp (d.attackMs, minAttackMs, maxAttackMs);
                const double releaseSeconds = 0.001 * std::clamp (d.releaseMs, minReleaseMs, maxReleaseMs);
                const double fall = std::exp (-1.0 / (releaseSeconds * sampleRate));
                double peak = 0.0;

                for (int i = 0; i < n; ++i)
                {
                    double x = 0.0;
                    for (int ch = 0; ch < numChannels; ++ch)
                        x = std::max (x, std::abs (detectorState[(size_t) ch].process (detector, inputs[ch][i])));

                    envelope = std::max (x, envelope * fall);
                    peak = std::max (peak, envelope);
                }

                targetDb = dynamicGainDb (d, 20.0 * std::log10 (peak + 1.0e-30));
                const bool moreAction = std::abs (targetDb) > std::abs (dynamicDb);
                const double coeff = std::exp (-n / ((moreAction ? attackSeconds : releaseSeconds) * sampleRate));
                dynamicDb = targetDb + coeff * (dynamicDb - targetDb);
            }
            else
            {
                dynamicDb = 0.0;
            }

            if (std::abs (dynamicDb - targetDb) < 1.0e-4)
                dynamicDb = targetDb;

            // Tiny moves are not worth new coefficients, but settling on the
            // final value always is.
            if (std::abs (dynamicDb - appliedDynamicDb) > 0.005 || (exactlyEqual (dynamicDb, targetDb) && ! exactlyEqual (dynamicDb, appliedDynamicDb)))
                dirty = true;

            if (dirty)
                updateDesign (sampleRate);

            for (int ch = 0; ch < numChannels; ++ch)
                run (ch, buffers[ch], n);
        }

        void updateDesign (double sampleRate) noexcept
        {
            const auto& p = target.params;
            const BandParams current { p.shape, std::exp (logFreq), gainDb + dynamicDb, std::exp (logQ), p.slopeDbPerOct };
            const int previousSections = design.numSections;
            design = designBand (current, sampleRate);
            appliedDynamicDb = dynamicDb;
            dirty = false;

            // Sections that were idle until now start from silence.
            for (auto& ch : state)
                for (int s = previousSections; s < design.numSections; ++s)
                    ch[(size_t) s] = {};
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

    // Moves each band's frequency, gain and Q a step towards its target. The
    // coefficients are recalculated when the band next runs.
    void updateTargets() noexcept
    {
        for (auto& b : bands)
        {
            if (! b.target.active)
                continue;

            const auto& p = b.target.params;
            const double targetLogFreq = std::log (std::clamp (p.freqHz, minFreqHz, maxFreqHz));
            const double targetLogQ = std::log (std::clamp (p.q, minQ, maxQ));
            bool changed = b.redesign;
            b.redesign = false;

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
                b.detector = detectorFilter (p.shape, std::exp (b.logFreq), std::exp (b.logQ), sampleRate);

            b.dirty |= changed;
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
