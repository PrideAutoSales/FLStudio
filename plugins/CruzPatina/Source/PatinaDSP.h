#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// The six colour modules of Cruz Patina. Every module processes a block in place and
// crossfades its own effect in and out, so switching a module on or off never clicks.
namespace patina
{
constexpr int maxChannels = 2;
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float twoPi = juce::MathConstants<float>::twoPi;

// xorshift32: cheap, allocation free and safe to call on the audio thread.
class Random
{
public:
    explicit Random (uint32_t seed = 0x9e3779b9u) noexcept { setSeed (seed); }

    void setSeed (uint32_t seed) noexcept { state = seed != 0 ? seed : 1u; }

    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }

private:
    uint32_t state = 1;
};

// Zero-delay-feedback state variable filter (Andy Simper's trapezoidal form).
// One instance per channel.
class Svf
{
public:
    enum class Type { lowpass, highpass, bandpass, lowShelf, highShelf, bell };

    void setup (Type type, float sampleRate, float freq, float q = 0.7071f, float gainDb = 0.0f) noexcept
    {
        freq = juce::jlimit (5.0f, 0.48f * sampleRate, freq);
        const auto A = std::pow (10.0f, gainDb / 40.0f);
        auto g = std::tan (pi * freq / sampleRate);
        auto k = 1.0f / q;

        switch (type)
        {
            case Type::lowpass:   m0 = 0.0f; m1 = 0.0f;               m2 = 1.0f; break;
            case Type::highpass:  m0 = 1.0f; m1 = -k;                 m2 = -1.0f; break;
            case Type::bandpass:  m0 = 0.0f; m1 = k;                  m2 = 0.0f; break;
            case Type::lowShelf:  g /= std::sqrt (A);
                                  m0 = 1.0f; m1 = k * (A - 1.0f);     m2 = A * A - 1.0f; break;
            case Type::highShelf: g *= std::sqrt (A);
                                  m0 = A * A; m1 = k * (1.0f - A) * A; m2 = 1.0f - A * A; break;
            case Type::bell:      k = 1.0f / (q * A);
                                  m0 = 1.0f; m1 = k * (A * A - 1.0f); m2 = 0.0f; break;
        }

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float process (float v0) noexcept
    {
        const auto v3 = v0 - ic2;
        const auto v1 = a1 * ic1 + a2 * v3;
        const auto v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

private:
    float a1 = 0, a2 = 0, a3 = 0, m0 = 1, m1 = 0, m2 = 0;
    float ic1 = 0, ic2 = 0;
};

inline float onePoleCoefficient (float freq, float sampleRate) noexcept
{
    return 1.0f - std::exp (-twoPi * freq / sampleRate);
}

using Smoothed = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

//==============================================================================
// Integer delay used to keep the dry signal lined up with the processed one.
class IntegerDelay
{
public:
    void prepare (int maxDelay)
    {
        for (auto& b : buffers)
            b.assign ((size_t) juce::nextPowerOfTwo (maxDelay + 1), 0.0f);
        mask = (int) buffers[0].size() - 1;
        reset();
    }

    void setDelay (int samples) noexcept { delay = juce::jlimit (0, mask, samples); }

    void reset() noexcept
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
    }

    void process (const float* const* in, float* const* out, int numChannels, int numSamples) noexcept
    {
        auto pos = writePos;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& b = buffers[(size_t) ch];
            pos = writePos;

            for (int i = 0; i < numSamples; ++i)
            {
                b[(size_t) pos] = in[ch][i];
                out[ch][i] = b[(size_t) ((pos - delay) & mask)];
                pos = (pos + 1) & mask;
            }
        }

        writePos = pos;
    }

private:
    std::array<std::vector<float>, maxChannels> buffers;
    int mask = 0, delay = 0, writePos = 0;
};

//==============================================================================
// Noise: synthesised tape hiss, vinyl surface (crackle, pops, rumble) or mains hum.
class NoiseModule
{
public:
    enum Type { hiss = 0, vinyl, hum };

    struct Params
    {
        bool enabled = false;
        float amount = 0.0f; // 0..1
        int type = hiss;
        float tone = 0.5f;   // 0..1
        float follow = 0.0f; // 0..1, how much the noise follows the input level
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        level.reset (sampleRate, 0.05);
        level.setCurrentAndTargetValue (0.0f);
        attack = onePoleCoefficient (1.0f / (twoPi * 0.005f), fs);
        release = onePoleCoefficient (1.0f / (twoPi * 0.25f), fs);
        rumbleCoef = onePoleCoefficient (40.0f, fs);
        popCoef = onePoleCoefficient (700.0f, fs);
        crackleDecay = std::exp (-1.0f / (0.00025f * fs));
        popDecay = std::exp (-1.0f / (0.004f * fs));
        rng[0].setSeed (0x1234567u);
        rng[1].setSeed (0x7654321u);
        for (auto& h : highpass)
            h.setup (Svf::Type::highpass, fs, 40.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& c : chans)
            c = {};
        for (auto& f : toneFilter) f.reset();
        for (auto& h : highpass) h.reset();
        envelope = 0.0f;
        humPhase = 0.0f;
    }

    void process (float* const* data, int numChannels, int numSamples, const Params& p) noexcept
    {
        const auto target = p.enabled && p.amount > 0.0f
                                ? juce::Decibels::decibelsToGain (-60.0f + 48.0f * p.amount)
                                : 0.0f;
        level.setTargetValue (target);

        if (! level.isSmoothing() && level.getCurrentValue() <= 0.0f)
            return;

        const auto toneHz = 1500.0f * std::pow (12.0f, p.tone);
        if (std::abs (toneHz - lastToneHz) > 1.0f)
        {
            for (auto& f : toneFilter)
                f.setup (Svf::Type::lowpass, fs, toneHz, 0.6f);
            lastToneHz = toneHz;
        }

        const auto humInc = 60.0f / fs;
        const auto crackleRate = 6.0f + 40.0f * p.tone; // clicks per second
        const auto popRate = 0.6f;

        for (int i = 0; i < numSamples; ++i)
        {
            auto peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, std::abs (data[ch][i]));
            envelope += (peak > envelope ? attack : release) * (peak - envelope);

            const auto gate = juce::jmin (1.0f, envelope * 8.0f);
            const auto g = level.getNextValue() * ((1.0f - p.follow) + p.follow * gate);

            auto humSample = 0.0f;
            if (p.type == hum)
            {
                const auto ph = twoPi * humPhase;
                const auto buzz = p.tone;
                humSample = 0.7f * std::sin (ph)
                          + buzz * (0.35f * std::sin (2.0f * ph) + 0.25f * std::sin (3.0f * ph)
                                    + 0.12f * std::sin (5.0f * ph) + 0.06f * std::sin (7.0f * ph));
                humPhase += humInc;
                if (humPhase >= 1.0f)
                    humPhase -= 1.0f;
            }

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& c = chans[(size_t) ch];
                auto& r = rng[(size_t) ch];
                auto n = 0.0f;

                if (p.type == hiss)
                {
                    n = toneFilter[(size_t) ch].process (pink (c, r.bipolar()) * 1.6f);
                }
                else if (p.type == vinyl)
                {
                    if (r.uniform() < crackleRate / fs)
                        c.crackle = r.bipolar() * (0.2f + 0.8f * cube (r.uniform()));
                    if (r.uniform() < popRate / fs)
                        c.pop = (r.uniform() < 0.5f ? -1.0f : 1.0f) * (0.6f + 0.4f * r.uniform());

                    const auto click = c.crackle * r.bipolar();
                    c.crackle *= crackleDecay;

                    c.popLp += popCoef * (c.pop - c.popLp);
                    c.pop *= popDecay;

                    c.rumble += rumbleCoef * (r.bipolar() - c.rumble);

                    const auto surface = pink (c, r.bipolar()) * 0.12f;
                    n = toneFilter[(size_t) ch].process (click * 5.0f + surface * 2.0f) + c.popLp * 3.0f + c.rumble * 1.2f;
                }
                else
                {
                    n = humSample * 0.5f;
                }

                data[ch][i] += highpass[(size_t) ch].process (n) * g;
            }
        }
    }

private:
    struct ChannelState
    {
        float b0 = 0, b1 = 0, b2 = 0;
        float crackle = 0, pop = 0, popLp = 0, rumble = 0;
    };

    static float cube (float x) noexcept { return x * x * x; }

    // Paul Kellet's economy pink noise filter.
    static float pink (ChannelState& c, float white) noexcept
    {
        c.b0 = 0.99765f * c.b0 + white * 0.0990460f;
        c.b1 = 0.96300f * c.b1 + white * 0.2965164f;
        c.b2 = 0.57000f * c.b2 + white * 1.0526913f;
        return (c.b0 + c.b1 + c.b2 + white * 0.1848f) * 0.18f;
    }

    float fs = 44100.0f;
    Smoothed level;
    std::array<ChannelState, maxChannels> chans;
    std::array<Random, maxChannels> rng;
    std::array<Svf, maxChannels> toneFilter, highpass;
    float lastToneHz = -1.0f;
    float envelope = 0, attack = 0, release = 0;
    float humPhase = 0;
    float rumbleCoef = 0, popCoef = 0, crackleDecay = 0, popDecay = 0;
};

//==============================================================================
// Wobble: wow (slow pitch drift) and flutter (fast pitch jitter) from a modulated delay.
// The delay always runs, so the plugin's latency stays the same when the module is off.
class WobbleModule
{
public:
    struct Params
    {
        bool enabled = false;
        float wow = 0.0f;     // 0..1
        float rate = 0.8f;    // Hz
        float flutter = 0.0f; // 0..1
        float stereo = 0.0f;  // 0..1
    };

    static constexpr float centreMs = 4.0f; // just over the deepest wow plus flutter swing (3.85 ms)
    static constexpr float maxWowMs = 3.5f;
    static constexpr float maxFlutterMs = 0.35f;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        centre = (int) std::round (centreMs * 0.001f * fs);
        const auto size = juce::nextPowerOfTwo (2 * centre + 16);
        for (auto& b : buffers)
            b.assign ((size_t) size, 0.0f);
        mask = size - 1;
        wowDepth.reset (sampleRate, 0.15);
        flutterDepth.reset (sampleRate, 0.15);
        rng.setSeed (0x2468aceu);
        reset();
    }

    int getLatencySamples() const noexcept { return centre; }

    void reset() noexcept
    {
        for (auto& b : buffers)
            std::fill (b.begin(), b.end(), 0.0f);
        writePos = 0;
        wowPhase = flutterPhase1 = flutterPhase2 = 0.0f;
        drift = {};
        driftTarget = {};
        flutterNoise = {};
        driftCounter = 0;
        wowDepth.setCurrentAndTargetValue (0.0f);
        flutterDepth.setCurrentAndTargetValue (0.0f);
    }

    void process (float* const* data, int numChannels, int numSamples, const Params& p) noexcept
    {
        const auto msToSamples = 0.001f * fs;
        wowDepth.setTargetValue (p.enabled ? p.wow * maxWowMs * msToSamples : 0.0f);
        flutterDepth.setTargetValue (p.enabled ? p.flutter * maxFlutterMs * msToSamples : 0.0f);

        const auto wowInc = p.rate / fs;
        const auto flutterInc1 = 7.3f / fs;
        const auto flutterInc2 = 11.9f / fs;
        const auto driftSlew = onePoleCoefficient (p.rate * 0.7f, fs);
        const auto flutterSlew = onePoleCoefficient (18.0f, fs);
        const auto driftPeriod = juce::jmax (1, (int) (fs / (p.rate * 1.5f)));
        const auto offset = 0.25f * p.stereo;

        for (int i = 0; i < numSamples; ++i)
        {
            if (--driftCounter <= 0)
            {
                driftCounter = driftPeriod;
                driftTarget[0] = rng.bipolar();
                driftTarget[1] = rng.bipolar();
            }

            const auto wd = wowDepth.getNextValue();
            const auto fd = flutterDepth.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto o = (float) ch * offset;
                drift[(size_t) ch] += driftSlew * (driftTarget[(size_t) ch] - drift[(size_t) ch]);
                flutterNoise[(size_t) ch] += flutterSlew * (rng.bipolar() - flutterNoise[(size_t) ch]);

                const auto d = ch == 0 ? drift[0] : drift[0] + p.stereo * (drift[1] - drift[0]);
                const auto wow = 0.7f * std::sin (twoPi * (wowPhase + o)) + 0.3f * d;
                const auto flutter = 0.5f * std::sin (twoPi * (flutterPhase1 + o))
                                   + 0.25f * std::sin (twoPi * (flutterPhase2 + 1.7f * o))
                                   + 0.8f * flutterNoise[(size_t) ch];

                auto& b = buffers[(size_t) ch];
                b[(size_t) writePos] = data[ch][i];

                const auto delay = (float) centre + wd * wow + fd * flutter;
                data[ch][i] = read (b, (float) writePos - delay);
            }

            writePos = (writePos + 1) & mask;
            wowPhase = wrap (wowPhase + wowInc);
            flutterPhase1 = wrap (flutterPhase1 + flutterInc1);
            flutterPhase2 = wrap (flutterPhase2 + flutterInc2);
        }
    }

private:
    static float wrap (float x) noexcept { return x >= 1.0f ? x - 1.0f : x; }

    // 4-point Hermite interpolation.
    float read (const std::vector<float>& b, float pos) const noexcept
    {
        const auto ip = (int) std::floor (pos);
        const auto t = pos - (float) ip;
        const auto xm1 = b[(size_t) ((ip - 1) & mask)];
        const auto x0 = b[(size_t) (ip & mask)];
        const auto x1 = b[(size_t) ((ip + 1) & mask)];
        const auto x2 = b[(size_t) ((ip + 2) & mask)];
        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    float fs = 44100.0f;
    int centre = 0, mask = 0, writePos = 0, driftCounter = 0;
    std::array<std::vector<float>, maxChannels> buffers;
    Smoothed wowDepth, flutterDepth;
    float wowPhase = 0, flutterPhase1 = 0, flutterPhase2 = 0;
    std::array<float, maxChannels> drift {}, driftTarget {}, flutterNoise {};
    Random rng;
};

//==============================================================================
// Distort: four saturation characters, run at 2x oversampling to keep aliasing down.
// The oversampling filters are linear phase, so the dry/wet Mix never comb filters.
class DistortModule
{
public:
    enum Type { tube = 0, tape, fuzz, hard };

    struct Params
    {
        bool enabled = false;
        float drive = 0.0f; // 0..1
        int type = tube;
        float tone = 1.0f;  // 0..1
        float mix = 1.0f;   // 0..1
    };

    void prepare (double sampleRate, int maxBlock)
    {
        osRate = (float) sampleRate * 2.0f;
        oversampling.initProcessing ((size_t) maxBlock);
        wet.reset (sampleRate * 2.0, 0.05);
        driveGain.reset (sampleRate * 2.0, 0.05);
        for (auto& f : dcBlock)
            f.setup (Svf::Type::highpass, osRate, 12.0f);
        lastToneHz = -1.0f;
        reset();
    }

    int getLatencySamples() const noexcept { return (int) std::round (oversampling.getLatencyInSamples()); }

    void reset() noexcept
    {
        oversampling.reset();
        for (auto& f : toneFilter) f.reset();
        for (auto& f : dcBlock) f.reset();
        wet.setCurrentAndTargetValue (0.0f);
        driveGain.setCurrentAndTargetValue (1.0f);
    }

    void process (juce::dsp::AudioBlock<float> block, const Params& p) noexcept
    {
        wet.setTargetValue (p.enabled ? p.mix : 0.0f);
        driveGain.setTargetValue (juce::Decibels::decibelsToGain (30.0f * p.drive));

        const auto toneHz = 2000.0f * std::pow (10.0f, p.tone);
        if (std::abs (toneHz - lastToneHz) > 1.0f)
        {
            for (auto& f : toneFilter)
                f.setup (Svf::Type::lowpass, osRate, toneHz, 0.6f);
            lastToneHz = toneHz;
        }

        auto up = oversampling.processSamplesUp (block);
        const auto numChannels = (int) up.getNumChannels();
        const auto numSamples = (int) up.getNumSamples();

        // Smoothers are shared across channels, so step them once per sample.
        for (int i = 0; i < numSamples; ++i)
        {
            const auto w = wet.getNextValue();
            const auto drive = driveGain.getNextValue();
            const auto makeup = 1.0f / std::sqrt (drive);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* d = up.getChannelPointer ((size_t) ch);
                const auto x = d[i];
                auto y = shape (x * drive, p.type) * makeup;
                y = dcBlock[(size_t) ch].process (toneFilter[(size_t) ch].process (y));
                d[i] = x + w * (y - x);
            }
        }

        oversampling.processSamplesDown (block);
    }

private:
    static float shape (float x, int type) noexcept
    {
        switch (type)
        {
            case tube: return std::tanh (x + 0.3f) - 0.29131261f; // tanh(0.3), even harmonics
            case tape: return (2.0f / pi) * std::atan (x * pi * 0.5f);
            case fuzz: return x >= 0.0f ? 1.0f - std::exp (-1.5f * x)
                                        : -0.6f * (1.0f - std::exp (2.5f * x));
            default:   return juce::jlimit (-1.0f, 1.0f, x);
        }
    }

    float osRate = 88200.0f, lastToneHz = -1.0f;
    juce::dsp::Oversampling<float> oversampling { (size_t) maxChannels, 1,
                                                  juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
                                                  true, true };
    Smoothed wet, driveGain;
    std::array<Svf, maxChannels> toneFilter, dcBlock;
};

//==============================================================================
// Digital: sample-rate reduction (sample and hold, no anti-aliasing on purpose) and bit crushing.
class DigitalModule
{
public:
    struct Params
    {
        bool enabled = false;
        float mix = 0.0f;       // 0..1
        float rate = 48000.0f;  // Hz
        float bits = 16.0f;     // 2..16
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        wet.reset (sampleRate, 0.05);
        reset();
    }

    void reset() noexcept
    {
        phase = 1.0f;
        held = {};
        wet.setCurrentAndTargetValue (0.0f);
    }

    void process (float* const* data, int numChannels, int numSamples, const Params& p) noexcept
    {
        wet.setTargetValue (p.enabled ? p.mix : 0.0f);

        if (! wet.isSmoothing() && wet.getCurrentValue() <= 0.0f)
            return;

        const auto inc = juce::jmin (1.0f, p.rate / fs);
        const auto steps = std::pow (2.0f, p.bits - 1.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            phase += inc;
            const auto sampleNow = phase >= 1.0f;
            if (sampleNow)
                phase -= 1.0f;

            const auto w = wet.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto x = data[ch][i];
                if (sampleNow)
                    held[(size_t) ch] = std::round (x * steps) / steps;
                data[ch][i] = x + w * (held[(size_t) ch] - x);
            }
        }
    }

private:
    float fs = 44100.0f, phase = 1.0f;
    std::array<float, maxChannels> held {};
    Smoothed wet;
};

//==============================================================================
// Space: an 8-line feedback delay network for small rooms through to long, dark halls.
class SpaceModule
{
public:
    struct Params
    {
        bool enabled = false;
        float mix = 0.0f;   // 0..1
        float size = 0.5f;  // 0..1
        float decay = 0.5f; // 0..1
    };

    static constexpr int numLines = 8;

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        const auto maxSamples = (int) std::ceil (baseMs.back() * maxScale * 0.001f * fs) + 4;
        const auto size = juce::nextPowerOfTwo (maxSamples);
        for (auto& l : lines)
            l.assign ((size_t) size, 0.0f);
        mask = size - 1;
        wet.reset (sampleRate, 0.05);
        scale.reset (sampleRate, 0.3);
        dampCoef = onePoleCoefficient (5500.0f, fs);
        for (auto& f : inputHighpass)
            f.setup (Svf::Type::highpass, fs, 180.0f);
        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lines)
            std::fill (l.begin(), l.end(), 0.0f);
        damp = {};
        writePos = 0;
        for (auto& f : inputHighpass) f.reset();
        wet.setCurrentAndTargetValue (0.0f);
        scale.setCurrentAndTargetValue (scaleFor (0.5f));
        silent = true;
    }

    void process (float* const* data, int numChannels, int numSamples, const Params& p) noexcept
    {
        wet.setTargetValue (p.enabled ? p.mix : 0.0f);
        scale.setTargetValue (scaleFor (p.size));

        if (! wet.isSmoothing() && wet.getCurrentValue() <= 0.0f)
        {
            if (! silent)
                reset();
            return;
        }
        silent = false;

        const auto rt60 = 0.25f * std::pow (32.0f, p.decay); // 0.25 s .. 8 s
        std::array<float, numLines> gains {};
        for (int n = 0; n < numLines; ++n)
        {
            const auto len = baseMs[(size_t) n] * scale.getTargetValue() * 0.001f * fs;
            gains[(size_t) n] = std::pow (10.0f, -3.0f * len / (rt60 * fs));
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const auto s = scale.getNextValue() * 0.001f * fs;
            const auto w = wet.getNextValue();

            const auto inL = inputHighpass[0].process (data[0][i]);
            const auto inR = numChannels > 1 ? inputHighpass[1].process (data[1][i]) : inL;

            std::array<float, numLines> o {};
            auto sum = 0.0f;
            for (int n = 0; n < numLines; ++n)
            {
                const auto readPos = (float) writePos - baseMs[(size_t) n] * s;
                const auto ip = (int) std::floor (readPos);
                const auto t = readPos - (float) ip;
                const auto& l = lines[(size_t) n];
                const auto a = l[(size_t) (ip & mask)];
                const auto b = l[(size_t) ((ip + 1) & mask)];
                auto v = a + t * (b - a);
                damp[(size_t) n] += dampCoef * (v - damp[(size_t) n]);
                v = damp[(size_t) n] * gains[(size_t) n];
                o[(size_t) n] = v;
                sum += v;
            }

            const auto outL = (o[0] - o[2] + o[4] - o[6]) * 0.6f;
            const auto outR = (o[1] - o[3] + o[5] - o[7]) * 0.6f;

            // Householder feedback matrix: lossless, dense mixing between every line.
            const auto h = sum * (2.0f / (float) numLines);
            for (int n = 0; n < numLines; ++n)
            {
                const auto in = (n & 1) == 0 ? inL : inR;
                const auto sign = (n & 2) == 0 ? 0.5f : -0.5f;
                lines[(size_t) n][(size_t) writePos] = o[(size_t) n] - h + in * sign;
            }
            writePos = (writePos + 1) & mask;

            const auto dryGain = 1.0f - 0.4f * w;
            if (numChannels > 1)
            {
                data[0][i] = data[0][i] * dryGain + outL * w;
                data[1][i] = data[1][i] * dryGain + outR * w;
            }
            else
            {
                data[0][i] = data[0][i] * dryGain + 0.5f * (outL + outR) * w;
            }
        }
    }

private:
    static constexpr float maxScale = 1.6f;
    static float scaleFor (float size) noexcept { return 0.3f + (maxScale - 0.3f) * size; }

    static constexpr std::array<float, numLines> baseMs { 23.3f, 29.7f, 37.1f, 41.1f, 47.3f, 53.9f, 61.7f, 71.3f };

    float fs = 44100.0f, dampCoef = 0;
    int mask = 0, writePos = 0;
    bool silent = true;
    std::array<std::vector<float>, numLines> lines;
    std::array<float, numLines> damp {};
    std::array<Svf, maxChannels> inputHighpass;
    Smoothed wet, scale;
};

//==============================================================================
// Magnetic: tape saturation and head bump, worn-tape high loss and level wander, and dropouts.
class MagneticModule
{
public:
    struct Params
    {
        bool enabled = false;
        float saturation = 0.0f; // 0..1
        float wear = 0.0f;       // 0..1
        float dropouts = 0.0f;   // 0..1
    };

    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        wet.reset (sampleRate, 0.05);
        dropAttack = onePoleCoefficient (60.0f, fs);
        dropRelease = onePoleCoefficient (15.0f, fs);
        wanderCoef = onePoleCoefficient (0.6f, fs);
        rng.setSeed (0x51ed270bu);
        lastSaturation = -1.0f;
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : bump) f.reset();
        for (auto& f : lowCut) f.reset();
        lp1 = lp2 = {};
        dropGain = 1.0f;
        dropTarget = 1.0f;
        dropRemaining = 0;
        wander = wanderTarget = 0.0f;
        wanderCounter = 0;
        wet.setCurrentAndTargetValue (0.0f);
    }

    void process (float* const* data, int numChannels, int numSamples, const Params& p) noexcept
    {
        wet.setTargetValue (p.enabled ? 1.0f : 0.0f);

        if (! wet.isSmoothing() && wet.getCurrentValue() <= 0.0f)
            return;

        if (std::abs (p.saturation - lastSaturation) > 1.0e-4f)
        {
            for (auto& f : bump)
                f.setup (Svf::Type::bell, fs, 85.0f, 0.9f, 3.5f * p.saturation);
            for (auto& f : lowCut)
                f.setup (Svf::Type::highpass, fs, 22.0f + 10.0f * p.saturation);
            lastSaturation = p.saturation;
        }

        const auto drive = 1.0f + 3.0f * p.saturation;
        const auto wearHz = 20000.0f * std::pow (0.15f, p.wear);
        const auto dropRate = p.dropouts * p.dropouts * 3.0f; // events per second
        const auto wanderDepth = 0.06f * p.wear;
        float coef = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            if (dropRemaining > 0)
            {
                if (--dropRemaining == 0)
                    dropTarget = 1.0f;
            }
            else if (dropRate > 0.0f && rng.uniform() < dropRate / fs)
            {
                dropRemaining = (int) ((0.03f + 0.22f * rng.uniform()) * fs);
                dropTarget = 1.0f - p.dropouts * (0.3f + 0.6f * rng.uniform());
            }
            dropGain += (dropTarget < dropGain ? dropAttack : dropRelease) * (dropTarget - dropGain);

            if (--wanderCounter <= 0)
            {
                wanderCounter = (int) (fs * 0.4f);
                wanderTarget = rng.bipolar();
            }
            wander += wanderCoef * (wanderTarget - wander);

            // Dropouts also dull the top end, like tape lifting off the head.
            if ((i & 15) == 0)
                coef = onePoleCoefficient (juce::jmax (800.0f, wearHz * (0.25f + 0.75f * dropGain)), fs);

            const auto gain = dropGain * (1.0f + wanderDepth * wander);
            const auto w = wet.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto x = data[ch][i];
                auto y = lowCut[(size_t) ch].process (bump[(size_t) ch].process (x));
                y = std::tanh (y * drive) / drive;
                lp1[(size_t) ch] += coef * (y - lp1[(size_t) ch]);
                lp2[(size_t) ch] += coef * (lp1[(size_t) ch] - lp2[(size_t) ch]);
                y = lp2[(size_t) ch] * gain;
                data[ch][i] = x + w * (y - x);
            }
        }
    }

private:
    float fs = 44100.0f, lastSaturation = -1.0f;
    std::array<Svf, maxChannels> bump, lowCut;
    std::array<float, maxChannels> lp1 {}, lp2 {};
    float dropGain = 1, dropTarget = 1, dropAttack = 0, dropRelease = 0;
    int dropRemaining = 0, wanderCounter = 0;
    float wander = 0, wanderTarget = 0, wanderCoef = 0;
    Random rng;
    Smoothed wet;
};

} // namespace patina
