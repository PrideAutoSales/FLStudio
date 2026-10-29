#pragma once

// The Cruz Sidechain engine. Plain C++ with no JUCE dependency, so it can be
// tested on its own (see tests/EngineTest.cpp).
//
// Signal flow:
//   key (sidechain, self or ghost kick) -> key filters -> level detector
//     -> gain computer (threshold, ratio, knee, range)
//     -> shaped ballistics (attack, hold, release, each with its own bend)
//     -> gain applied to the main signal, either full band or only below the
//        split frequency, after the lookahead delay.
//
// The engine also measures what the display needs: waveform peaks of the key,
// the main input and the main output, the gain reduction, and the low-end
// energy of all three in 24 bands from 20 to 285 Hz (for the heat map).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cruzsc
{

enum class Detect { peak, rms };

struct Params
{
    float thresholdDb = -24.0f;
    float ratio = 8.0f;        // 1 to 30, where 30 means infinity
    float kneeDb = 6.0f;
    float rangeDb = 30.0f;     // the most the main signal is ever turned down
    float attackMs = 1.0f;
    float holdMs = 20.0f;
    float releaseMs = 160.0f;
    float attackBend = 0.5f;   // -1 (slow start) to 1 (fast start); 0 is a straight line
    float releaseBend = 0.3f;
    float keyGainDb = 0.0f;
    float keyHighPassHz = 20.0f;
    float keyLowPassHz = 20000.0f;
    Detect detect = Detect::peak;
    bool splitLows = false;    // duck only the lows, below splitHz
    float splitHz = 150.0f;
    float mix = 1.0f;
    float outputGain = 1.0f;   // linear
    bool listenKey = false;
    float lookaheadMs = 0.0f;
};

inline constexpr float maxLookaheadMs = 10.0f;
inline constexpr float infiniteRatio = 30.0f;
inline constexpr int numBands = 24;

// Centre of each heat-map band: six bands per octave from 20 Hz.
inline float bandHz (int band) { return 20.0f * std::pow (2.0f, (float) band / 6.0f); }

// How much the gain computer turns the signal down, in dB (0 or more), for a
// key level in dB. Shared by the audio and the transfer-curve display.
inline float gainReductionDb (float levelDb, const Params& p)
{
    const float slope = p.ratio >= infiniteRatio ? 1.0f : 1.0f - 1.0f / std::max (1.0f, p.ratio);
    const float over = levelDb - p.thresholdDb;
    const float knee = std::max (0.0f, p.kneeDb);
    float gr;

    if (2.0f * over <= -knee)
        gr = 0.0f;
    else if (knee > 0.0f && 2.0f * std::abs (over) < knee)
        gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
    else
        gr = slope * over;

    return std::clamp (gr, 0.0f, std::max (0.0f, p.rangeDb));
}

// Bends a 0..1 ramp. bend 0 is a straight line, above 0 moves fast first and
// eases in, below 0 starts slow and speeds up.
struct Bend
{
    float k = 0.0f, norm = 1.0f;

    void set (float bend)
    {
        k = std::clamp (bend, -1.0f, 1.0f) * 6.0f;
        norm = std::abs (k) < 1.0e-3f ? 1.0f : 1.0f / (1.0f - std::exp (-k));
    }

    float operator() (float x) const
    {
        x = std::clamp (x, 0.0f, 1.0f);
        if (std::abs (k) < 1.0e-3f)
            return x;
        return (1.0f - std::exp (-k * x)) * norm;
    }
};

// Moves the gain reduction towards its target along shaped segments: an
// attack ramp when more reduction is needed, then a hold, then a release ramp.
// Every segment takes its set time no matter how deep the duck is, so the
// pump keeps the same feel at any level.
class Ballistics
{
public:
    void set (float attackMs, float holdMs, float releaseMs, float attackBend, float releaseBend, float sampleRate)
    {
        attackStep = attackMs <= 0.01f ? 1.0f : 1.0f / (attackMs * 0.001f * sampleRate);
        releaseStep = 1.0f / std::max (1.0f, releaseMs * 0.001f * sampleRate);
        holdSamples = (int) std::lround (holdMs * 0.001f * sampleRate);
        attack.set (attackBend);
        release.set (releaseBend);
    }

    void reset() { g = g0 = p = 0.0f; holdLeft = 0; seg = Seg::idle; }

    float process (float target)
    {
        constexpr float eps = 1.0e-4f;

        if (target > g + eps)
        {
            if (seg != Seg::attack || p >= 1.0f)
            {
                seg = Seg::attack;
                g0 = g;
                p = 0.0f;
            }

            p = std::min (1.0f, p + attackStep);
            g = g0 + (target - g0) * attack (p);
            holdLeft = holdSamples;
            return g;
        }

        if (seg == Seg::attack)
            seg = Seg::hold;

        if (seg == Seg::hold)
        {
            if (holdLeft > 0)
            {
                --holdLeft;
                return g;
            }
            startRelease();
        }

        if (seg == Seg::idle)
        {
            if (target >= g - eps)
                return g = target;
            startRelease();
        }

        p = std::min (1.0f, p + releaseStep);
        g = target + (g0 - target) * (1.0f - release (p));

        if (p >= 1.0f)
        {
            seg = Seg::idle;
            g = target;
        }

        return g;
    }

    float current() const { return g; }

private:
    enum class Seg { idle, attack, hold, release };

    void startRelease()
    {
        seg = Seg::release;
        g0 = g;
        p = 0.0f;
    }

    Bend attack, release;
    float attackStep = 1.0f, releaseStep = 1.0f;
    int holdSamples = 0, holdLeft = 0;
    float g = 0.0f, g0 = 0.0f, p = 0.0f;
    Seg seg = Seg::idle;
};

namespace detail
{
    inline constexpr float pi = 3.14159265358979f;

    // Topology-preserving state variable filter (Zavalishin), 12 dB/oct.
    struct Svf
    {
        float g = 0, k = 1.414f, a1 = 0, a2 = 0, a3 = 0, s1 = 0, s2 = 0;

        void set (float hz, float sr, float q = 0.7071f)
        {
            hz = std::clamp (hz, 5.0f, sr * 0.45f);
            g = std::tan (pi * hz / sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void reset() { s1 = s2 = 0; }

        // Returns the low-pass output; band and high are written to the refs.
        float tick (float x, float& band, float& high)
        {
            const float v3 = x - s2;
            const float v1 = a1 * s1 + a2 * v3;
            const float v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            band = v1;
            high = x - k * v1 - v2;
            return v2;
        }

        float lowPass (float x) { float b, h; return tick (x, b, h); }
        float highPass (float x) { float b, h; tick (x, b, h); return h; }
        float bandPassUnity (float x) { float b, h; tick (x, b, h); return k * b; }
    };

    // Linkwitz-Riley 24 dB/oct split: the two halves add back up flat.
    struct Crossover
    {
        Svf lp1, lp2, hp1, hp2;

        void set (float hz, float sr) { for (auto* f : { &lp1, &lp2, &hp1, &hp2 }) f->set (hz, sr); }
        void reset() { for (auto* f : { &lp1, &lp2, &hp1, &hp2 }) f->reset(); }

        void split (float x, float& low, float& high)
        {
            low = lp2.lowPass (lp1.lowPass (x));
            high = hp2.highPass (hp1.highPass (x));
        }
    };

    struct DelayLine
    {
        std::vector<float> buf;
        int size = 1, writePos = 0;

        void prepare (int samples) { size = std::max (4, samples); buf.assign ((size_t) size, 0.0f); writePos = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

        // Writes x and returns what was written `delay` samples ago.
        float process (float x, int delay)
        {
            buf[(size_t) writePos] = x;
            int r = writePos - delay;
            if (r < 0) r += size;
            if (++writePos >= size) writePos = 0;
            return buf[(size_t) r];
        }
    };
} // namespace detail

// Measures how much energy one signal has in each low band. It works on a
// copy decimated to about 6 kHz, which keeps 72 band filters cheap.
class LowBands
{
public:
    void prepare (float sampleRate)
    {
        decimation = std::max (1, (int) std::lround (sampleRate / 6000.0f));
        const float rate = sampleRate / (float) decimation;
        aa1.set (600.0f, sampleRate);
        aa2.set (600.0f, sampleRate);

        for (int b = 0; b < numBands; ++b)
            bands[(size_t) b].set (bandHz (b), rate, 6.0f);

        smooth = std::exp (-1.0f / (0.025f * rate));
        reset();
    }

    void reset()
    {
        aa1.reset();
        aa2.reset();
        for (auto& f : bands) f.reset();
        energy.fill (0.0f);
        counter = 0;
    }

    void push (float x)
    {
        const float y = aa2.lowPass (aa1.lowPass (x));

        if (++counter < decimation)
            return;
        counter = 0;

        for (int b = 0; b < numBands; ++b)
        {
            const float v = bands[(size_t) b].bandPassUnity (y);
            auto& e = energy[(size_t) b];
            e = v * v + (e - v * v) * smooth;
        }
    }

    // Level of each band in dB, where a full-scale sine reads 0 dB.
    void read (float* dbOut) const
    {
        for (int b = 0; b < numBands; ++b)
            dbOut[b] = 10.0f * std::log10 (energy[(size_t) b] * 2.0f + 1.0e-12f);
    }

private:
    detail::Svf aa1, aa2;
    std::array<detail::Svf, numBands> bands {};
    std::array<float, numBands> energy {};
    float smooth = 0.0f;
    int decimation = 1, counter = 0;
};

// One slice of what the display shows, made every few milliseconds.
struct Frame
{
    float keyMin = 0, keyMax = 0, inMin = 0, inMax = 0, outMin = 0, outMax = 0;
    float grDb = 0;       // deepest reduction during the slice
    float keyLevelDb = -100;
    std::array<float, numBands> keyBands {}, inBands {}, outBands {};
};

// A stand-in kick for tempo mode: a short sine that drops from 150 to 50 Hz.
class GhostKick
{
public:
    void prepare (float sampleRate) { sr = sampleRate; reset(); }
    void reset() { t = 1.0e9f; phase = 0.0f; }
    void trigger() { t = 0.0f; phase = 0.0f; }

    float next()
    {
        if (t > 0.4f)
            return 0.0f;

        const float hz = 50.0f + 100.0f * std::exp (-t * 40.0f);
        phase += hz / sr;
        if (phase >= 1.0f) phase -= 1.0f;
        const float amp = std::exp (-t * 9.0f) * std::min (1.0f, t * 2000.0f);
        t += 1.0f / sr;
        return 0.9f * amp * std::sin (2.0f * detail::pi * phase);
    }

private:
    float sr = 48000.0f, t = 1.0e9f, phase = 0.0f;
};

class SidechainEngine
{
public:
    void prepare (float sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        const int maxDelay = (int) std::ceil (maxLookaheadMs * 0.001f * sr) + 8;
        for (auto& d : delays) d.prepare (maxDelay);
        keyBands.prepare (sr);
        inBands.prepare (sr);
        outBands.prepare (sr);
        hopSamples = std::max (32, (int) std::lround (sr / 187.5f));
        peakRelease = std::exp (-1.0f / (0.025f * sr));
        rmsSmooth = std::exp (-1.0f / (0.010f * sr));
        applyParams();
        reset();
    }

    void reset()
    {
        for (auto& d : delays) d.reset();
        for (auto& f : keyHp) f.reset();
        for (auto& f : keyLp) f.reset();
        for (auto& x : split) x.reset();
        for (auto& x : drySplit) x.reset();
        ballistics.reset();
        keyBands.reset();
        inBands.reset();
        outBands.reset();
        env = 0.0f;
        frame = {};
        hopCount = 0;
        mixNow = params.mix;
        outNow = params.outputGain;
    }

    void setParams (const Params& p)
    {
        params = p;
        applyParams();
    }

    const Params& getParams() const { return params; }
    int getLatencySamples() const { return lookahead; }
    float getGainReductionDb() const { return ballistics.current(); }
    float getKeyLevelDb() const { return keyLevelDb; }

    // io: main channels (1 or 2), processed in place. key: 1 or 2 channels,
    // or null for none. sink(const Frame&) is called whenever a display
    // slice is complete.
    template <typename Sink>
    void process (float* const* io, int numChannels, const float* const* key, int keyChannels, int numSamples, Sink&& sink)
    {
        numChannels = std::clamp (numChannels, 1, 2);
        keyChannels = key == nullptr ? 0 : std::clamp (keyChannels, 1, 2);
        const float smoothCoef = 1.0f - std::exp (-1.0f / (0.02f * sr));
        const float keyGain = std::pow (10.0f, params.keyGainDb / 20.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            // Key: filter each channel, then detect the louder one.
            float keyFiltered = 0.0f, keyMono = 0.0f, keyAbs = 0.0f;

            for (int c = 0; c < keyChannels; ++c)
            {
                const float raw = key[c][i];
                keyMono += raw;
                const float f = keyLp[(size_t) c].lowPass (keyHp[(size_t) c].highPass (raw)) * keyGain;
                keyFiltered += f;
                keyAbs = std::max (keyAbs, std::abs (f));
            }

            if (keyChannels > 0)
            {
                keyMono /= (float) keyChannels;
                keyFiltered /= (float) keyChannels;
            }

            float level;
            if (params.detect == Detect::peak)
            {
                env = keyAbs > env ? keyAbs : keyAbs + (env - keyAbs) * peakRelease;
                level = env;
            }
            else
            {
                const float sq = keyAbs * keyAbs;
                env = sq + (env - sq) * rmsSmooth;
                level = std::sqrt (env) * 1.41421356f; // a sine reads its peak level
            }

            keyLevelDb = 20.0f * std::log10 (level + 1.0e-9f);
            const float gr = ballistics.process (gainReductionDb (keyLevelDb, params));
            const float gain = std::pow (10.0f, -gr / 20.0f);

            mixNow += (params.mix - mixNow) * smoothCoef;
            outNow += (params.outputGain - outNow) * smoothCoef;

            float inMono = 0.0f, outMono = 0.0f;

            for (int c = 0; c < numChannels; ++c)
            {
                const float x = io[c][i];
                inMono += x;
                const float delayed = delays[(size_t) c].process (x, lookahead);

                float wet;
                if (params.splitLows)
                {
                    float lo, hi;
                    split[(size_t) c].split (delayed, lo, hi);
                    wet = lo * gain + hi;
                    // The crossover shifts phase, so the dry path goes through
                    // it too to keep the mix knob from combing.
                    float dl, dh;
                    drySplit[(size_t) c].split (delayed, dl, dh);
                    const float dry = dl + dh;
                    wet = dry + (wet - dry) * mixNow;
                }
                else
                {
                    wet = delayed + (delayed * gain - delayed) * mixNow;
                }

                float y = params.listenKey ? keyFiltered : wet;
                y *= outNow;
                io[c][i] = y;
                outMono += y;
            }

            inMono /= (float) numChannels;
            outMono /= (float) numChannels;

            keyBands.push (keyMono);
            inBands.push (inMono);
            outBands.push (outMono);

            frame.keyMin = std::min (frame.keyMin, keyMono);
            frame.keyMax = std::max (frame.keyMax, keyMono);
            frame.inMin = std::min (frame.inMin, inMono);
            frame.inMax = std::max (frame.inMax, inMono);
            frame.outMin = std::min (frame.outMin, outMono);
            frame.outMax = std::max (frame.outMax, outMono);
            frame.grDb = std::max (frame.grDb, gr);
            frame.keyLevelDb = std::max (frame.keyLevelDb, keyLevelDb);

            if (++hopCount >= hopSamples)
            {
                keyBands.read (frame.keyBands.data());
                inBands.read (frame.inBands.data());
                outBands.read (frame.outBands.data());
                sink (static_cast<const Frame&> (frame));
                frame = {};
                hopCount = 0;
            }
        }
    }

private:
    void applyParams()
    {
        ballistics.set (params.attackMs, params.holdMs, params.releaseMs, params.attackBend, params.releaseBend, sr);
        for (auto& f : keyHp) f.set (params.keyHighPassHz, sr);
        for (auto& f : keyLp) f.set (params.keyLowPassHz, sr);
        for (auto& x : split) x.set (params.splitHz, sr);
        for (auto& x : drySplit) x.set (params.splitHz, sr);
        lookahead = (int) std::lround (std::clamp (params.lookaheadMs, 0.0f, maxLookaheadMs) * 0.001f * sr);
    }

    Params params;
    float sr = 48000.0f;
    int lookahead = 0;

    std::array<detail::DelayLine, 2> delays;
    std::array<detail::Svf, 2> keyHp, keyLp;
    std::array<detail::Crossover, 2> split, drySplit;
    Ballistics ballistics;
    LowBands keyBands, inBands, outBands;

    float env = 0.0f, keyLevelDb = -100.0f, peakRelease = 0.0f, rmsSmooth = 0.0f;
    float mixNow = 1.0f, outNow = 1.0f;

    Frame frame;
    int hopSamples = 256, hopCount = 0;
};

} // namespace cruzsc
