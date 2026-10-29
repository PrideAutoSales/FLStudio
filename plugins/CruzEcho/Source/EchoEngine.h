#pragma once

// The Cruz Echo sound engine. Plain C++ with no JUCE dependency, so it can be
// tested on its own (see tests/EngineTest.cpp).
//
// Each echo "chain" is two delay stages in a loop: A feeds B, B feeds back to A.
// With no groove both stages have the same time, so the repeats land on the grid.
// Groove makes A longer and B shorter by the same amount, which swings every
// other repeat. In ping-pong mode A plays on the left and B on the right.
//
// Every stage colours what it writes (filters, saturation, lo-fi, diffusion),
// so each repeat is coloured once, like a signal going round a tape loop.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cruzecho
{

enum class Style { digital, analog, tape, space, lofi };
enum class Mode { single, stereo, pingPong };

struct Params
{
    float delayMs = 375.0f;    // echo time, 1 to 4000 ms
    float feedback = 0.35f;    // 0 to 1.1 (above 1 runs away into the saturation)
    float groove = 0.0f;       // 0 to 1, where 1 is full triplet swing
    float lowCutHz = 20.0f;    // the style may cut harder than this
    float highCutHz = 20000.0f;
    float saturation = 0.25f;  // 0 to 1
    float wobble = 0.3f;       // 0 to 1, wow and flutter amount
    float width = 1.0f;        // 0 (mono) to 2 (extra wide)
    float duck = 0.0f;         // 0 to 1, how much the echoes dip while you play
    float mix = 0.3f;          // 0 dry to 1 wet
    float outputGain = 1.0f;   // linear
    Style style = Style::tape;
    Mode mode = Mode::single;
};

inline constexpr float maxDelayMs = 4000.0f;

struct StyleSettings
{
    float minLowCutHz, maxHighCutHz;
    float wowMs, flutterMs, wowHz, flutterHz;
    float glideSeconds;  // how slowly the time follows the knob (tape-style pitch bend)
    float driveDb;       // drive at full saturation
    int shape;           // 0 clean, 1 soft, 2 tape (asymmetric)
    bool diffuse, lofi;
};

inline const StyleSettings& settingsFor (Style s)
{
    static const StyleSettings table[] = {
        // lowcut, highcut, wow, flutter, wowHz, flutHz, glide, drive, shape, diffuse, lofi
        { 10.0f, 20000.0f, 0.6f, 0.05f, 0.35f, 5.0f, 0.06f, 12.0f, 0, false, false }, // digital
        { 60.0f,  3200.0f, 0.5f, 0.15f, 0.50f, 9.0f, 0.25f, 18.0f, 1, false, false }, // analog
        { 45.0f,  8500.0f, 1.6f, 0.22f, 0.70f, 7.5f, 0.35f, 20.0f, 2, false, false }, // tape
        { 90.0f,  5000.0f, 2.6f, 0.35f, 0.55f, 6.0f, 0.45f, 20.0f, 2, true,  false }, // space
        { 300.0f, 3000.0f, 1.2f, 0.30f, 0.90f, 11.0f, 0.20f, 16.0f, 1, false, true  }, // lo-fi
    };
    return table[static_cast<int> (s)];
}

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
        float tick (float x, float& high)
        {
            const float v3 = x - s2;
            const float v1 = a1 * s1 + a2 * v3;
            const float v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            high = x - k * v1 - v2;
            return v2;
        }

        float lowPass (float x) { float h; return tick (x, h); }
        float highPass (float x) { float h; tick (x, h); return h; }
    };

    struct Allpass
    {
        std::vector<float> buf;
        int pos = 0;

        void prepare (int length) { buf.assign ((size_t) std::max (1, length), 0.0f); pos = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

        float tick (float x, float coeff)
        {
            const float delayed = buf[(size_t) pos];
            const float v = x + coeff * delayed;
            buf[(size_t) pos] = v;
            if (++pos >= (int) buf.size())
                pos = 0;
            return delayed - coeff * v;
        }
    };

    struct DelayLine
    {
        std::vector<float> buf;
        int size = 1, writePos = 0;

        void prepare (int samples) { size = std::max (4, samples); buf.assign ((size_t) size, 0.0f); writePos = 0; }
        void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

        void write (float x)
        {
            buf[(size_t) writePos] = x;
            if (++writePos >= size)
                writePos = 0;
        }

        float at (int i) const
        {
            if (i < 0) i += size;
            else if (i >= size) i -= size;
            return buf[(size_t) i];
        }

        // Four-point Hermite read, delay in samples (at least 2).
        float read (float delay) const
        {
            float rp = (float) writePos - delay;
            while (rp < 0.0f)
                rp += (float) size;
            const int i = (int) rp;
            const float f = rp - (float) i;
            const float xm1 = at (i - 1), x0 = at (i), x1 = at (i + 1), x2 = at (i + 2);
            const float c1 = 0.5f * (x1 - xm1);
            const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
            const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
            return ((c3 * f + c2) * f + c1) * f + x0;
        }
    };

    // Colours one repeat: filters, drive, lo-fi crush and (for Space) diffusion.
    struct Colour
    {
        Svf lowCut, highCut, highCut2;
        Allpass ap1, ap2;
        float dcX = 0, dcY = 0, dcCoeff = 0.9995f;
        float held = 0, holdPhase = 0;

        void prepare (float sr, int seed)
        {
            ap1.prepare ((int) (sr * (0.0071f + 0.0013f * (float) seed)));
            ap2.prepare ((int) (sr * (0.0113f + 0.0017f * (float) seed)));
            dcCoeff = std::exp (-2.0f * pi * 8.0f / sr);
            reset();
        }

        void reset()
        {
            lowCut.reset(); highCut.reset(); highCut2.reset();
            ap1.reset(); ap2.reset();
            dcX = dcY = held = holdPhase = 0;
        }

        void setFilters (float lowHz, float highHz, float sr)
        {
            lowCut.set (lowHz, sr);
            highCut.set (highHz, sr);
            highCut2.set (highHz * 1.15f, sr, 0.54f);
        }

        static float shapeSample (float x, int shape)
        {
            switch (shape)
            {
                case 0:  return 2.0f * std::tanh (0.5f * x);
                case 1:  return std::tanh (x);
                default: return std::tanh (x + 0.18f) - 0.17808f; // tanh(0.18), keeps silence at zero
            }
        }

        // 8-bit mu-law, like the companding converters in early digital delays.
        // Quiet echoes keep their detail and fade out naturally instead of being
        // gated by a coarse linear quantiser; loud ones pick up the grit.
        static float companded (float x)
        {
            constexpr float mu = 255.0f, steps = 127.0f;
            const float invLog = 1.0f / std::log1p (mu);
            const float c = std::log1p (mu * std::min (std::abs (x), 1.0f)) * invLog;
            const float q = std::round (c * steps) / steps;
            return std::copysign (std::expm1 (q / invLog) / mu, x);
        }

        float process (float x, const StyleSettings& s, float drive, float holdStep)
        {
            x = lowCut.highPass (x);

            if (s.lofi)
            {
                holdPhase += holdStep;
                if (holdPhase >= 1.0f)
                {
                    holdPhase -= 1.0f;
                    held = companded (x);
                }
                x = held;
            }

            x = shapeSample (x * drive, s.shape) / drive;
            x = highCut2.lowPass (highCut.lowPass (x));

            if (s.diffuse)
                x = 0.6f * x + 0.4f * ap2.tick (ap1.tick (x, 0.55f), 0.5f);

            // DC blocker, mostly for the asymmetric tape curve.
            const float y = x - dcX + dcCoeff * dcY;
            dcX = x;
            dcY = y;
            return y;
        }
    };

    // Slow wow, fast flutter and a random drift, in milliseconds of delay change.
    struct Wobbler
    {
        float wowPhase = 0, flutterPhase = 0, drift = 0, driftTarget = 0, driftCoeff = 0;
        int driftCounter = 0;
        uint32_t rng = 1;

        void prepare (float sr, uint32_t seed, float phaseOffset)
        {
            rng = seed * 2654435761u + 1u;
            wowPhase = phaseOffset;
            flutterPhase = phaseOffset * 1.7f;
            drift = driftTarget = 0;
            driftCounter = 0;
            driftCoeff = 1.0f - std::exp (-1.0f / (0.25f * sr));
        }

        float nextRandom()
        {
            rng = rng * 1664525u + 1013904223u;
            return (float) (rng >> 8) / 8388608.0f - 1.0f; // -1..1
        }

        float tick (float wowMs, float flutterMs, float wowInc, float flutterInc, int driftPeriod)
        {
            wowPhase += wowInc;
            if (wowPhase > 2.0f * pi) wowPhase -= 2.0f * pi;
            flutterPhase += flutterInc;
            if (flutterPhase > 2.0f * pi) flutterPhase -= 2.0f * pi;

            if (--driftCounter <= 0)
            {
                driftCounter = driftPeriod;
                driftTarget = nextRandom();
            }
            drift += driftCoeff * (driftTarget - drift);

            return wowMs * (0.7f * std::sin (wowPhase) + 0.3f * drift)
                 + flutterMs * std::sin (flutterPhase);
        }
    };

    struct Chain
    {
        DelayLine a, b;
        Colour colourA, colourB;
        Wobbler wobble;
        float lastA = 0, lastB = 0;

        void reset()
        {
            a.reset(); b.reset();
            colourA.reset(); colourB.reset();
            lastA = lastB = 0;
        }
    };

    struct Smoothed
    {
        float value = 0, target = 0;
        void snap (float v) { value = target = v; }
        float next (float coeff) { value += coeff * (target - value); return value; }
    };
} // namespace detail

class EchoEngine
{
public:
    void prepare (double sampleRate, int /*maxBlockSize*/)
    {
        sr = (float) sampleRate;

        // Room for the longest time plus full groove, wobble and some margin.
        const int length = (int) std::ceil (sr * (maxDelayMs * (1.0f + grooveMax) + 50.0f) / 1000.0f) + 8;

        for (int c = 0; c < 2; ++c)
        {
            auto& ch = chains[c];
            ch.a.prepare (length);
            ch.b.prepare (length);
            ch.colourA.prepare (sr, c * 2);
            ch.colourB.prepare (sr, c * 2 + 1);
            ch.wobble.prepare (sr, (uint32_t) (c + 7), c * 1.9f);
        }

        paramCoeff = 1.0f - std::exp (-1.0f / (0.02f * sr));
        envAttack = 1.0f - std::exp (-1.0f / (0.003f * sr));
        envRelease = 1.0f - std::exp (-1.0f / (0.25f * sr));
        prepared = true;
        reset();
    }

    void reset()
    {
        for (auto& ch : chains)
            ch.reset();

        env = 0;
        wetPeak = 0;
        lastMode = params.mode;
        updateTargets();

        for (auto* v : { &delayA, &delayB, &feedback, &wetGain, &dryGain, &outGain, &width, &duck })
            v->snap (v->target);
    }

    void setParams (const Params& p)
    {
        params = p;

        if (! prepared)
            return;

        if (params.mode != lastMode)
        {
            // Old echoes from the other routing would play from the wrong side.
            for (auto& ch : chains)
                ch.reset();
            lastMode = params.mode;
        }

        updateTargets();
    }

    // Processes in place. numChannels is 1 or 2.
    void process (float* const* channels, int numChannels, int numSamples)
    {
        processImpl (channels, numChannels, numSamples);
    }

    float getWetPeak() const noexcept { return wetPeak; }

    static constexpr float grooveMax = 1.0f / 3.0f; // 2:1 long/short, a triplet shuffle

private:
    void updateTargets()
    {
        updateFilters();

        const float base = std::clamp (params.delayMs, 1.0f, maxDelayMs) * sr / 1000.0f;
        const float swing = std::clamp (params.groove, 0.0f, 1.0f) * grooveMax;
        delayA.target = base * (1.0f + swing);
        delayB.target = base * (1.0f - swing);
        feedback.target = std::clamp (params.feedback, 0.0f, 1.1f);
        wetGain.target = std::sin (std::clamp (params.mix, 0.0f, 1.0f) * 0.5f * detail::pi);
        dryGain.target = std::cos (std::clamp (params.mix, 0.0f, 1.0f) * 0.5f * detail::pi);
        outGain.target = params.outputGain;
        width.target = std::clamp (params.width, 0.0f, 2.0f);
        duck.target = std::clamp (params.duck, 0.0f, 1.0f);
    }

    void processImpl (float* const* channels, int numChannels, int numSamples)
    {
        const auto& s = settingsFor (params.style);
        const float drive = std::pow (10.0f, s.driveDb * std::clamp (params.saturation, 0.0f, 1.0f) / 20.0f);
        const float wobbleAmount = 2.0f * std::clamp (params.wobble, 0.0f, 1.0f);
        const float wowSamples = s.wowMs * wobbleAmount * sr / 1000.0f;
        const float flutterSamples = s.flutterMs * wobbleAmount * sr / 1000.0f;
        const float wowInc = 2.0f * detail::pi * s.wowHz / sr;
        const float flutterInc = 2.0f * detail::pi * s.flutterHz / sr;
        const int driftPeriod = (int) (0.4f * sr);
        const float glideCoeff = 1.0f - std::exp (-1.0f / (s.glideSeconds * sr));
        const float holdStep = 7500.0f / sr;

        const bool stereoIO = numChannels > 1;
        const Mode mode = stereoIO ? params.mode : Mode::single;
        const int activeChains = mode == Mode::stereo ? 2 : 1;

        float* left = channels[0];
        float* right = stereoIO ? channels[1] : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float dryL = left[i];
            const float dryR = stereoIO ? right[i] : dryL;
            const float mono = 0.5f * (dryL + dryR);

            const float dA = delayA.next (glideCoeff);
            const float dB = delayB.next (glideCoeff);
            const float fb = feedback.next (paramCoeff);

            float wetL = 0, wetR = 0;

            for (int c = 0; c < activeChains; ++c)
            {
                auto& ch = chains[c];
                const float mod = ch.wobble.tick (wowSamples, flutterSamples, wowInc, flutterInc, driftPeriod);

                const float outA = ch.a.read (std::max (2.0f, dA + mod));
                const float outB = ch.b.read (std::max (2.0f, dB + 0.8f * mod));

                const float in = mode == Mode::stereo ? (c == 0 ? dryL : dryR) : mono;
                ch.a.write (ch.colourA.process (in + fb * outB, s, drive, holdStep));
                ch.b.write (ch.colourB.process (fb * outA, s, drive, holdStep));

                if (mode == Mode::pingPong)
                {
                    wetL = outA;
                    wetR = outB;
                }
                else if (mode == Mode::stereo)
                {
                    (c == 0 ? wetL : wetR) = outA + outB;
                }
                else
                {
                    wetL = wetR = outA + outB;
                }
            }

            // Stereo width on the echoes only.
            const float w = width.next (paramCoeff);
            const float mid = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR) * w;
            wetL = mid + side;
            wetR = mid - side;

            // Ducking: the echoes dip while the input is loud and swell back in the gaps.
            const float level = std::abs (mono);
            env += (level > env ? envAttack : envRelease) * (level - env);
            const float duckGain = 1.0f / (1.0f + duck.next (paramCoeff) * 12.0f * env);

            const float wg = wetGain.next (paramCoeff) * duckGain;
            const float dg = dryGain.next (paramCoeff);
            const float og = outGain.next (paramCoeff);

            if (stereoIO)
            {
                left[i] = og * (dg * dryL + wg * wetL);
                right[i] = og * (dg * dryR + wg * wetR);
            }
            else
            {
                left[i] = og * (dg * dryL + wg * mid);
            }

            wetPeak = std::max (wetPeak * 0.99995f, std::abs (wg * mid));
        }
    }

    void updateFilters()
    {
        const auto& s = settingsFor (params.style);
        const float low = std::max (params.lowCutHz, s.minLowCutHz);
        const float high = std::min (params.highCutHz, s.maxHighCutHz);

        for (auto& ch : chains)
        {
            ch.colourA.setFilters (low, high, sr);
            ch.colourB.setFilters (low, high, sr);
        }
    }

    Params params;
    detail::Chain chains[2];
    detail::Smoothed delayA, delayB, feedback, wetGain, dryGain, outGain, width, duck;
    Mode lastMode = Mode::single;
    float sr = 48000.0f, paramCoeff = 0.001f, envAttack = 0.01f, envRelease = 0.001f, env = 0;
    float wetPeak = 0;
    bool prepared = false;
};

} // namespace cruzecho
