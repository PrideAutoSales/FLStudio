#pragma once

// The sound Cruz Chords plays when no synth is loaded: soft electric-style
// keys or a warm pad. It only needs to be good enough to hear the chords.

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cmath>

namespace cruzchords
{
struct ChordSound final : juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class ChordVoice final : public juce::SynthesiserVoice
{
public:
    explicit ChordVoice (const std::atomic<int>& soundChoice) : choice (soundChoice) {}

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<ChordSound*> (s) != nullptr; }

    void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
    {
        pad = choice.load() == 1;
        const double hz = juce::MidiMessage::getMidiNoteInHertz (note);
        const double sr = getSampleRate();
        inc = hz / sr;
        detune = std::pow (2.0, 7.0 / 1200.0);
        gain = 0.16f * (0.35f + 0.65f * velocity);
        brightness = 0.5f + 0.5f * velocity;

        juce::ADSR::Parameters p;
        if (pad)
            p = { 0.35f, 1.2f, 0.8f, 0.9f };
        else
            p = { 0.003f, 1.4f, 0.3f, 0.45f };
        env.setSampleRate (sr);
        env.setParameters (p);
        env.noteOn();
        filterEnvTime = 0.0;
        // Start the oscillators at different points so chords do not phase together.
        ph1 = random.nextDouble();
        ph2 = random.nextDouble();
        ph3 = 0.0;
        ic1L = ic2L = ic1R = ic2R = 0.0f;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
            env.noteOff();
        else
        {
            env.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive())
            return;

        const double sr = getSampleRate();
        auto* left = out.getWritePointer (0);
        auto* right = out.getNumChannels() > 1 ? out.getWritePointer (1) : nullptr;

        for (int i = 0; i < num; ++i)
        {
            if ((i & 31) == 0)
                updateFilter (sr);

            float l, r;
            if (pad)
            {
                const float a = saw (ph1, inc * detune), b = saw (ph2, inc / detune);
                const float sub = (float) std::sin (juce::MathConstants<double>::twoPi * ph3);
                advance (ph3, inc * 0.5);
                l = 0.55f * a + 0.25f * b + 0.3f * sub;
                r = 0.25f * a + 0.55f * b + 0.3f * sub;
            }
            else
            {
                // Sine plus a little saw: a soft electric piano-ish tone.
                const float s1 = (float) std::sin (juce::MathConstants<double>::twoPi * ph3);
                const float s2 = (float) std::sin (juce::MathConstants<double>::twoPi * 2.0 * ph3);
                advance (ph3, inc);
                const float a = saw (ph1, inc * detune), b = saw (ph2, inc / detune);
                l = 0.8f * s1 + 0.22f * s2 + 0.18f * a;
                r = 0.8f * s1 + 0.22f * s2 + 0.18f * b;
            }

            const float e = env.getNextSample() * gain;
            l = lowpass (l, ic1L, ic2L) * e;
            r = lowpass (r, ic1R, ic2R) * e;
            left[start + i] += l;
            if (right != nullptr)
                right[start + i] += r;

            if (! env.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

    using juce::SynthesiserVoice::renderNextBlock;

private:
    static void advance (double& phase, double step)
    {
        phase += step;
        if (phase >= 1.0)
            phase -= 1.0;
    }

    static float saw (double& phase, double step)
    {
        double t = phase;
        double v = 2.0 * t - 1.0;
        if (t < step)
        {
            t /= step;
            v -= t + t - t * t - 1.0;
        }
        else if (t > 1.0 - step)
        {
            t = (t - 1.0) / step;
            v -= t * t + t + t + 1.0;
        }
        advance (phase, step);
        return (float) v;
    }

    void updateFilter (double sr)
    {
        filterEnvTime += 32.0 / sr;
        double cutoff;
        if (pad)
            cutoff = 1400.0 + 1800.0 * brightness;
        else
            cutoff = 1500.0 + 5000.0 * brightness * std::exp (-filterEnvTime * 3.0);
        cutoff = std::min (cutoff, sr * 0.45);
        const double g = std::tan (juce::MathConstants<double>::pi * cutoff / sr);
        const double k = 1.2;
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
    }

    float lowpass (float x, float& ic1, float& ic2) const
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

    const std::atomic<int>& choice;
    juce::ADSR env;
    juce::Random random;
    bool pad = false;
    double inc = 0, detune = 1, ph1 = 0, ph2 = 0, ph3 = 0, filterEnvTime = 0;
    float gain = 0, brightness = 1, a1 = 0, a2 = 0, a3 = 0;
    float ic1L = 0, ic2L = 0, ic1R = 0, ic2R = 0;
};
} // namespace cruzchords
