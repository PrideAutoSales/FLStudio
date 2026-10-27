#pragma once

// Building blocks for the Cruz Limiter engine. Everything here is
// allocation-free after prepare(), so it is safe to run on the audio thread.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Oversampler.h"

namespace cruz
{

inline float gainToDb (float g) noexcept     { return 20.0f * std::log10 (g); }
inline float dbToGain (float db) noexcept    { return std::exp (db * 0.11512925464970228f); } // ln(10)/20

//==============================================================================
/** Fixed-length delay. A delay of 0 passes samples straight through. */
class DelayLine
{
public:
    void prepare (int maxDelay)
    {
        buffer.assign ((size_t) maxDelay + 1, 0.0f);
        setDelay (0);
    }

    void setDelay (int newDelay) noexcept
    {
        delay = std::clamp (newDelay, 0, (int) buffer.size() - 1);
        clear();
    }

    /** Changes the delay length but fills the line with a value instead of silence. */
    void setDelayAndFill (int newDelay, float value) noexcept
    {
        delay = std::clamp (newDelay, 0, (int) buffer.size() - 1);
        std::fill (buffer.begin(), buffer.end(), value);
        writePos = 0;
    }

    void clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    int getDelay() const noexcept { return delay; }

    float process (float input) noexcept
    {
        if (delay == 0)
            return input;

        const auto size = (int) buffer.size();
        buffer[(size_t) writePos] = input;
        auto readPos = writePos - delay;
        if (readPos < 0)
            readPos += size;
        if (++writePos == size)
            writePos = 0;
        return buffer[(size_t) readPos];
    }

private:
    std::vector<float> buffer;
    int delay = 0, writePos = 0;
};

//==============================================================================
/** Running maximum over the last `window` samples (monotonic queue, O(1) amortised). */
class SlidingMax
{
public:
    void prepare (int maxWindow)
    {
        capacity = maxWindow + 1;
        values.assign ((size_t) capacity, 0.0f);
        stamps.assign ((size_t) capacity, 0);
        reset();
    }

    void reset() noexcept
    {
        head = 0;
        count = 0;
        now = 0;
    }

    float process (float value, int window) noexcept
    {
        while (count > 0 && values[(size_t) back()] <= value)
            --count;

        const auto slot = (head + count) % capacity;
        values[(size_t) slot] = value;
        stamps[(size_t) slot] = now;
        ++count;

        while (stamps[(size_t) head] <= now - window)
        {
            head = (head + 1) % capacity;
            --count;
        }

        ++now;
        return values[(size_t) head];
    }

private:
    int back() const noexcept { return (head + count - 1) % capacity; }

    std::vector<float> values;
    std::vector<int64_t> stamps;
    int capacity = 1, head = 0, count = 0;
    int64_t now = 0;
};

//==============================================================================
/** Moving average (box filter). The running sum is rebuilt on every wrap so it never drifts. */
class BoxAverage
{
public:
    void prepare (int maxLength)
    {
        buffer.assign ((size_t) std::max (1, maxLength), 0.0f);
        setLength (1, 0.0f);
    }

    void setLength (int newLength, float fillValue) noexcept
    {
        length = std::clamp (newLength, 1, (int) buffer.size());
        std::fill (buffer.begin(), buffer.begin() + length, fillValue);
        sum = (double) fillValue * length;
        pos = 0;
    }

    float process (float input) noexcept
    {
        sum += (double) input - (double) buffer[(size_t) pos];
        buffer[(size_t) pos] = input;

        if (++pos == length)
        {
            pos = 0;
            double exact = 0.0;
            for (int i = 0; i < length; ++i)
                exact += buffer[(size_t) i];
            sum = exact;
        }

        return (float) std::max (0.0, sum / length);
    }

private:
    std::vector<float> buffer;
    double sum = 0.0;
    int length = 1, pos = 0;
};

//==============================================================================
/** Release and linking behaviour of a limiter stage. */
struct StageTiming
{
    float fastReleaseCoef = 0.0f;   // per-sample decay of the fast release envelope (dB domain)
    float slowAttackCoef = 0.0f;    // how quickly the slow envelope charges up
    float slowReleaseCoef = 0.0f;   // how quickly the slow envelope lets go
    float slowAmount = 0.0f;        // 0 = only fast release, 1 = full program-dependent hold
    float linkTransients = 1.0f;    // 0..1, linking of the instant peak requirement
    float linkRelease = 1.0f;       // 0..1, linking of the release envelopes
};

/**
    Lookahead brickwall limiter stage for up to two channels.

    The gain path, in dB of gain reduction:
      required GR -> channel link -> delay -> sliding max (attack window)
      -> fast/slow release -> release link -> two cascaded moving averages.

    The sliding max window equals the combined length of the two averages and the
    audio is delayed by exactly the lookahead, so the smoothed gain is always fully
    down before a peak reaches the output. No sample leaves above the threshold.
*/
class LimiterStage
{
public:
    static constexpr int maxChannels = 2;

    /** `maxHold` is the longest time (in samples) the gain may be held after a peak. */
    void prepare (int maxLookahead, int maxHold = 0)
    {
        maxLength = std::max (1, maxLookahead);
        maxHoldLength = std::max (0, maxHold);
        for (int c = 0; c < maxChannels; ++c)
        {
            audioDelay[c].prepare (maxLength);
            grDelay[c].prepare (maxLength + 1);
            hold[c].prepare (maxLength + maxHoldLength + 1);
            boxA[c].prepare (maxLength + 1);
            boxB[c].prepare (maxLength + 1);
        }
        setLookahead (0, 1);
    }

    /** Sets lookahead and attack (both in samples). Clears the audio delay. */
    void setLookahead (int lookaheadSamples, int attackSamples) noexcept
    {
        lookahead = std::clamp (lookaheadSamples, 0, maxLength);
        attack = std::clamp (attackSamples, 1, std::max (1, lookahead));
        for (int c = 0; c < maxChannels; ++c)
        {
            audioDelay[c].setDelay (lookahead);
            fast[c] = slow[c] = smoothed[c] = 0.0f;
        }
        applyAttack (0.0f);
    }

    /** Changes only the attack window, keeping audio and gain continuous. */
    void setAttack (int attackSamples) noexcept
    {
        const auto newAttack = std::clamp (attackSamples, 1, std::max (1, lookahead));
        if (newAttack == attack)
            return;

        attack = newAttack;
        applyAttack (-1.0f);
    }

    void setTiming (const StageTiming& t) noexcept { timing = t; }

    /**
        Keeps the gain fully down for this many samples after each peak before
        releasing. Holding the gain steady around a peak stops the gain change
        itself from creating new inter-sample overshoots.
    */
    void setHold (int samples) noexcept { holdLength = std::clamp (samples, 0, maxHoldLength); }

    int getLatency() const noexcept { return lookahead; }
    int getAttack() const noexcept { return attack; }

    void reset() noexcept { setLookahead (lookahead, attack); }

    /**
        Limits `audio` in place so that no sample exceeds `threshold`.
        `detector` holds the level the gain must react to (for example true-peak
        estimates); pass nullptr to use the audio itself.
        `grOut`, when given, receives the largest gain reduction per sample in dB.
    */
    void process (float* const* audio, const float* const* detector, int numChannels,
                  int numSamples, float threshold, float* grOut) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);
        const auto invThreshold = 1.0f / threshold;

        for (int i = 0; i < numSamples; ++i)
        {
            float req[maxChannels] {};
            float maxReq = 0.0f;

            for (int c = 0; c < numChannels; ++c)
            {
                const auto level = detector != nullptr ? detector[c][i] : std::abs (audio[c][i]);
                const auto over = level * invThreshold;
                req[c] = over > 1.0f ? gainToDb (over) : 0.0f;
                maxReq = std::max (maxReq, req[c]);
            }

            float env[maxChannels] {};
            float maxEnv = 0.0f;

            for (int c = 0; c < numChannels; ++c)
            {
                const auto linked = req[c] + timing.linkTransients * (maxReq - req[c]);
                const auto held = hold[c].process (grDelay[c].process (linked), attack + holdLength);

                fast[c] = std::max (held, fast[c] * timing.fastReleaseCoef);

                const auto coef = fast[c] > slow[c] ? timing.slowAttackCoef : timing.slowReleaseCoef;
                slow[c] = fast[c] + coef * (slow[c] - fast[c]);

                env[c] = std::max (fast[c], slow[c] * timing.slowAmount);
                maxEnv = std::max (maxEnv, env[c]);
            }

            float maxGr = 0.0f;

            for (int c = 0; c < numChannels; ++c)
            {
                const auto linked = env[c] + timing.linkRelease * (maxEnv - env[c]);
                const auto gr = boxB[c].process (boxA[c].process (linked));
                smoothed[c] = gr;

                const auto delayed = audioDelay[c].process (audio[c][i]);
                audio[c][i] = gr > 1.0e-6f ? delayed * dbToGain (-gr) : delayed;
                maxGr = std::max (maxGr, gr);
            }

            if (grOut != nullptr)
                grOut[i] = maxGr;
        }
    }

private:
    void applyAttack (float fill) noexcept
    {
        // Two moving averages of lengths a and b give an S-shaped gain ramp; with
        // a + b - 1 == attack and the GR delayed by lookahead - attack + 1, the
        // total delay of the gain path matches the audio delay.
        const auto a = (attack + 1) / 2;
        const auto b = attack - a + 1;

        for (int c = 0; c < maxChannels; ++c)
        {
            const auto value = fill < 0.0f ? smoothed[c] : fill;
            grDelay[c].setDelayAndFill (lookahead - attack + 1, value); // 0 when lookahead is 0
            hold[c].reset();
            boxA[c].setLength (a, value);
            boxB[c].setLength (b, value);
            fast[c] = std::max (fast[c], value);
        }
    }

    DelayLine audioDelay[maxChannels], grDelay[maxChannels];
    SlidingMax hold[maxChannels];
    BoxAverage boxA[maxChannels], boxB[maxChannels];
    float fast[maxChannels] {}, slow[maxChannels] {}, smoothed[maxChannels] {};
    StageTiming timing;
    int maxLength = 1, maxHoldLength = 0, holdLength = 0, lookahead = 0, attack = 1;
};

//==============================================================================
/**
    Estimates inter-sample peaks with an 8x polyphase windowed-sinc interpolator.

    Each call returns the largest interpolated magnitude between input samples
    n - delay and n - delay + 1, where n is the newest input sample.
*/
class TruePeakEstimator
{
public:
    static constexpr int phases = 8;
    static constexpr int tapsPerPhase = 64;
    static constexpr int numTaps = phases * tapsPerPhase;
    /** Interpolated points lie between samples n - delay and n - delay + 1. */
    static constexpr int delay = tapsPerPhase / 2;

    TruePeakEstimator()
    {
        // Kaiser-windowed sinc, cut off at the original Nyquist frequency.
        constexpr double pi = 3.14159265358979323846;
        constexpr double beta = 8.0;
        const double centre = (numTaps - 1) * 0.5;

        auto bessel0 = [] (double x)
        {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 32; ++k)
            {
                term *= (x / (2.0 * k)) * (x / (2.0 * k));
                sum += term;
            }
            return sum;
        };

        std::array<double, numTaps> h {};
        for (int k = 0; k < numTaps; ++k)
        {
            const auto t = (k - centre) / phases;
            const auto sinc = std::abs (t) < 1.0e-12 ? 1.0 : std::sin (pi * t) / (pi * t);
            const auto r = (k - centre) / centre;
            const auto window = bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (beta);
            h[(size_t) k] = sinc * window;
        }

        for (int p = 0; p < phases; ++p)
        {
            double sum = 0.0;
            for (int m = 0; m < tapsPerPhase; ++m)
                sum += h[(size_t) (m * phases + p)];
            for (int m = 0; m < tapsPerPhase; ++m)
                coefs[(size_t) p][(size_t) m] = (float) (h[(size_t) (m * phases + p)] / sum);
        }

        reset();
    }

    void reset() noexcept
    {
        history.fill (0.0f);
        pos = 0;
    }

    float process (float input) noexcept
    {
        // History is stored twice so a contiguous window is always available.
        history[(size_t) pos] = history[(size_t) (pos + tapsPerPhase)] = input;

        float peak = 0.0f;
        for (int p = 0; p < phases; ++p)
        {
            float acc = 0.0f;
            // newest sample pairs with tap 0
            CRUZ_VECTORISE_LOOP
            for (int m = 0; m < tapsPerPhase; ++m)
                acc += coefs[(size_t) p][(size_t) m] * history[(size_t) (pos + tapsPerPhase - m)];
            peak = std::max (peak, std::abs (acc));
        }

        if (++pos == tapsPerPhase)
            pos = 0;

        return peak;
    }

private:
    std::array<std::array<float, tapsPerPhase>, phases> coefs {};
    std::array<float, tapsPerPhase * 2> history {};
    int pos = 0;
};

//==============================================================================
/** One-pole DC blocking filter (about 5 Hz). */
class DcBlocker
{
public:
    void prepare (double sampleRate) noexcept
    {
        coef = (float) std::exp (-2.0 * 3.14159265358979323846 * 5.0 / sampleRate);
        reset();
    }

    void reset() noexcept { x1 = y1 = 0.0f; }

    float process (float x) noexcept
    {
        const auto y = x - x1 + coef * y1;
        x1 = x;
        y1 = y;
        return y;
    }

private:
    float coef = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

//==============================================================================
/** TPDF dither with optional second-order noise shaping. */
class Ditherer
{
public:
    void reset() noexcept { e1 = e2 = 0.0f; }

    float process (float x, int bits, bool shaped) noexcept
    {
        const auto scale = (float) (1 << (bits - 1));
        const auto lsb = 1.0f / scale;

        const auto shapedInput = shaped ? x - (1.5f * e1 - 0.6f * e2) : x;
        const auto noise = (nextRandom() + nextRandom() - 1.0f) * lsb; // triangular, +-1 LSB
        const auto quantised = std::round ((shapedInput + noise) * scale) * lsb;

        e2 = e1;
        e1 = quantised - shapedInput;
        return quantised;
    }

private:
    float nextRandom() noexcept
    {
        seed = seed * 1664525u + 1013904223u;
        return (float) (seed >> 8) * (1.0f / 16777216.0f);
    }

    uint32_t seed = 22222u;
    float e1 = 0.0f, e2 = 0.0f;
};

} // namespace cruz
