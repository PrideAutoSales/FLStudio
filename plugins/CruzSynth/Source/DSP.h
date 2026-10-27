#pragma once

#include <juce_core/juce_core.h>
#include <cmath>
#include <vector>

// Small per-voice building blocks: envelope, filters, noise, sub oscillator.
namespace cs::dsp
{
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float twoPi = juce::MathConstants<float>::twoPi;

inline float fastTanh (float x) noexcept
{
    x = juce::jlimit (-4.5f, 4.5f, x);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// 0..1 -> 0..1 with an adjustable bow. k = 0 is a straight line.
inline float bend (float p, float k) noexcept
{
    if (std::abs (k) < 1.0e-3f)
        return p;
    return (1.0f - std::exp (-k * p)) / (1.0f - std::exp (-k));
}

//==============================================================================
class Envelope
{
public:
    enum Stage { Idle, Attack, Hold, Decay, Sustain, Release };

    struct Settings { float attack = 0.01f, hold = 0.0f, decay = 1.0f, sustain = 1.0f, release = 0.2f, curve = 0.5f; };

    void noteOn (bool fromCurrentLevel)
    {
        startLevel = fromCurrentLevel ? value : 0.0f;
        stage = Attack;
        pos = 0.0f;
    }

    void noteOff()
    {
        if (stage != Idle && stage != Release)
        {
            releaseFrom = value;
            stage = Release;
            pos = 0.0f;
        }
    }

    void reset()    { stage = Idle; value = 0.0f; pos = 0.0f; }
    bool isActive() const noexcept { return stage != Idle; }
    Stage getStage() const noexcept { return stage; }
    float getValue() const noexcept { return value; }

    // Advances by dt seconds and returns the new level.
    float tick (const Settings& s, float dt) noexcept
    {
        const float k = (s.curve - 0.5f) * 2.0f * 7.0f + 3.5f; // ~0..7, default 3.5
        switch (stage)
        {
            case Idle: value = 0.0f; break;
            case Attack:
                pos += s.attack > 1.0e-4f ? dt / s.attack : 1.0f;
                if (pos >= 1.0f) { value = 1.0f; stage = s.hold > 0.0f ? Hold : Decay; pos = 0.0f; }
                else value = startLevel + (1.0f - startLevel) * bend (pos, k * 0.4f);
                break;
            case Hold:
                pos += dt / s.hold;
                value = 1.0f;
                if (pos >= 1.0f) { stage = Decay; pos = 0.0f; }
                break;
            case Decay:
                pos += s.decay > 1.0e-4f ? dt / s.decay : 1.0f;
                if (pos >= 1.0f) { value = s.sustain; stage = Sustain; }
                else value = 1.0f - (1.0f - s.sustain) * bend (pos, k);
                break;
            case Sustain:
                value = s.sustain;
                break;
            case Release:
                pos += s.release > 1.0e-4f ? dt / s.release : 1.0f;
                if (pos >= 1.0f) { value = 0.0f; stage = Idle; }
                else value = releaseFrom * (1.0f - bend (pos, k));
                break;
        }
        return value;
    }

private:
    Stage stage = Idle;
    float value = 0.0f, pos = 0.0f, startLevel = 0.0f, releaseFrom = 0.0f;
};

//==============================================================================
// Zero-delay-feedback state variable filter (one channel).
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, k = 1.4f;

    void setCoefs (float g, float damping) noexcept
    {
        k = damping;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    struct Out { float lp, bp, hp; };

    Out process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

// Four cascaded one-pole stages with resonant feedback and a soft-clipped input.
struct Ladder
{
    float s[4] {};
    float G = 0.0f, beta = 1.0f, k = 0.0f;

    void setCoefs (float g, float res) noexcept
    {
        G = g / (1.0f + g);
        beta = 1.0f / (1.0f + g);
        k = res * 3.9f;
    }

    float process (float x) noexcept
    {
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float S = G3 * beta * s[0] + G2 * beta * s[1] + G * beta * s[2] + beta * s[3];
        const float y4 = (G4 * x * (1.0f + k * 0.3f) + S) / (1.0f + k * G4);
        float u = fastTanh (x * (1.0f + k * 0.3f) - k * y4);
        for (int i = 0; i < 4; ++i)
        {
            const float v = (u - s[i]) * G;
            const float y = v + s[i];
            s[i] = y + v;
            u = y;
        }
        return u;
    }

    void reset() noexcept { for (auto& v : s) v = 0.0f; }
};

// Feedback comb tuned to the cutoff frequency.
struct Comb
{
    std::vector<float> buf;
    int write = 0;

    void prepare (int maxDelay) { buf.assign ((size_t) maxDelay + 4, 0.0f); write = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }

    float process (float x, float delaySamples, float feedback) noexcept
    {
        const int size = (int) buf.size();
        if (size == 0)
            return x;
        delaySamples = juce::jlimit (1.0f, (float) size - 3.0f, delaySamples);
        float rp = (float) write - delaySamples;
        if (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float t = rp - (float) i0;
        const float d = buf[(size_t) i0] + t * (buf[(size_t) i1] - buf[(size_t) i0]);
        const float y = x + feedback * d;
        buf[(size_t) write] = fastTanh (y);
        write = (write + 1) % size;
        return y * 0.5f;
    }
};

//==============================================================================
class Noise
{
public:
    void seed (juce::uint32 s) noexcept { state = s * 2654435761u + 12345u; }

    float white() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) (state >> 8) * (2.0f / 16777216.0f) - 1.0f;
    }

    float next (int type, float color) noexcept
    {
        float x = 0.0f;
        switch (type)
        {
            case 0: x = white(); break;
            case 1: // pink (Paul Kellet's economy filter)
            {
                const float w = white();
                b0 = 0.99765f * b0 + w * 0.0990460f;
                b1 = 0.96300f * b1 + w * 0.2965164f;
                b2 = 0.57000f * b2 + w * 1.0526913f;
                x = (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
                break;
            }
            case 2: // brown
                brown = juce::jlimit (-1.0f, 1.0f, brown + white() * 0.04f);
                brown *= 0.998f;
                x = brown * 2.5f;
                break;
            case 3: // crackle: sparse random clicks
                x = (white() > 0.995f - color * 0.05f) ? white() : 0.0f;
                x *= 2.0f;
                break;
            default: // digital: stepped noise
                if (--hold <= 0) { held = white(); hold = 1 + (int) ((1.0f - color) * 60.0f); }
                return held;
        }

        if (type == 3)
            return x;

        // Color: 0 = dark, 0.5 = flat, 1 = bright
        const float c = color * 2.0f - 1.0f;
        lp += (x - lp) * 0.08f;
        return c < 0.0f ? x + (lp - x) * (-c) : x + (x - lp) * c * 1.5f;
    }

private:
    juce::uint32 state = 22222;
    float b0 = 0, b1 = 0, b2 = 0, brown = 0, lp = 0, held = 0;
    int hold = 0;
};

//==============================================================================
inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float subOsc (int shape, float phase, float dt) noexcept
{
    switch (shape)
    {
        case 0: return std::sin (twoPi * phase);
        case 1: return 1.0f - 4.0f * std::abs (phase - 0.5f) + 0.0f;
        case 2: return (2.0f * phase - 1.0f) - polyBlep (phase, dt);
        case 3:
        case 4:
        {
            const float w = shape == 3 ? 0.5f : 0.25f;
            float y = phase < w ? 1.0f : -1.0f;
            y += polyBlep (phase, dt);
            float t2 = phase - w;
            if (t2 < 0.0f) t2 += 1.0f;
            y -= polyBlep (t2, dt);
            return y * 0.8f;
        }
        default: return 0.0f;
    }
}
} // namespace cs::dsp
