#pragma once

// FET peak compressor model.
//
// The classic FET limiter has a fixed threshold: you turn Input up to push
// more signal into it and Output to set the level back. A field-effect
// transistor acts as a voltage-controlled resistor, so the gain changes
// very fast (attack from about 20 microseconds to 0.8 ms) and the FET adds
// a touch of second harmonic as it works harder. A class-A line amp with a
// transformer adds gentle odd harmonics at high levels.
//
// Ratios 4, 8, 12 and 20 get progressively higher thresholds and harder
// knees. "All buttons" mode overcompresses: past the knee the output
// actually drops as the input rises, release slows a little and distortion
// goes up, which is where the aggressive pumping sound comes from.
//
// This class runs at whatever rate it is prepared with; the plugin runs it
// at twice the host rate to keep the fast envelope and saturation clean.

#include "CruzDSP.h"
#include <array>

namespace cruz
{
class FetModel
{
public:
    static constexpr int maxChannels = 2;

    enum Ratio { ratio4, ratio8, ratio12, ratio20, ratioAll };

    struct Settings
    {
        float attack = 4.0f;  // 1 (slowest, 800 us) .. 7 (fastest, 20 us)
        float release = 4.0f; // 1 (slowest, 1.1 s) .. 7 (fastest, 50 ms)
        int ratio = ratio4;
    };

    static double attackSeconds (float knob) noexcept
    {
        return 800.0e-6 * std::pow (20.0 / 800.0, (std::clamp (knob, 1.0f, 7.0f) - 1.0) / 6.0);
    }

    static double releaseSeconds (float knob) noexcept
    {
        return 1.1 * std::pow (0.05 / 1.1, (std::clamp (knob, 1.0f, 7.0f) - 1.0) / 6.0);
    }

    // Static curve: gain change in dB (<= 0) for a detector level in dBFS.
    static float computeGainDb (float levelDb, int ratio) noexcept
    {
        struct Curve { float threshold, ratio, knee; };
        static constexpr Curve curves[] = {
            { -20.0f, 4.0f, 10.0f },
            { -18.5f, 8.0f, 7.0f },
            { -17.5f, 12.0f, 5.0f },
            { -16.5f, 20.0f, 3.0f },
            { -19.0f, 30.0f, 4.0f },
        };
        const auto& c = curves[std::clamp (ratio, 0, 4)];

        const float over = levelDb - c.threshold;
        float out;
        if (2.0f * over < -c.knee)
            out = levelDb;
        else if (2.0f * std::abs (over) <= c.knee)
        {
            const float t = over + c.knee * 0.5f;
            out = levelDb + (1.0f / c.ratio - 1.0f) * t * t / (2.0f * c.knee);
        }
        else
            out = c.threshold + over / c.ratio;

        float gr = out - levelDb;

        // All buttons: past the knee, push the output back down.
        if (ratio == ratioAll)
            gr -= 0.35f * std::max (0.0f, over - 3.0f);

        return std::min (gr, 0.0f);
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        for (auto& d : dcBlock)
            d.prepare (sampleRate);
        tube.setShape (0.5f, 0.05f);
        settingsDirty = true;
        reset();
    }

    void reset() noexcept
    {
        envDb = 0.0f;
        slowEnvDb = 0.0f;
        for (auto& d : dcBlock)
            d.reset();
    }

    void setSettings (const Settings& s) noexcept
    {
        if (std::abs (s.attack - settings.attack) > 1.0e-4f || std::abs (s.release - settings.release) > 1.0e-4f
            || s.ratio != settings.ratio)
        {
            settings = s;
            settingsDirty = true;
        }
    }

    // Processes up to two channels in place, linked detection.
    // Returns the deepest gain reduction in this block, in dB (<= 0).
    float process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);

        if (settingsDirty)
            updateCoefficients();

        const bool all = settings.ratio == ratioAll;
        const float fetAmount = all ? 0.22f : 0.08f;
        float deepest = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = std::max (peak, std::abs (channels[ch][i]));

            const float targetDb = computeGainDb (gainToDb (peak), settings.ratio);

            if (targetDb < envDb)
                envDb += attackCoef * (targetDb - envDb);
            else
            {
                // Program-dependent release: deep reduction lets go more slowly.
                const float depth = std::clamp (-envDb / 18.0f, 0.0f, 1.0f);
                envDb += releaseCoef / (1.0f + 0.8f * depth) * (targetDb - envDb);
            }

            // Second, slower stage that holds a little of long, heavy reduction.
            slowEnvDb += (envDb < slowEnvDb ? slowAttackCoef : slowReleaseCoef) * (envDb - slowEnvDb);
            const float grDb = std::min (envDb, 0.75f * envDb + 0.25f * slowEnvDb);

            const float gain = dbToGain (grDb);
            const float fet = fetAmount * std::clamp (-grDb / 20.0f, 0.0f, 1.0f);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float y = channels[ch][i] * gain;
                const float t = std::tanh (y);
                y += fet * t * t;              // FET: even harmonics, more as it works
                y = tube.process (y);          // line amp and transformer
                channels[ch][i] = dcBlock[(size_t) ch].process (y);
            }

            deepest = std::min (deepest, grDb);
        }

        return deepest;
    }

private:
    void updateCoefficients() noexcept
    {
        attackCoef = onePoleCoef (attackSeconds (settings.attack), sampleRate);
        const double rel = releaseSeconds (settings.release) * (settings.ratio == ratioAll ? 1.3 : 1.0);
        releaseCoef = onePoleCoef (rel, sampleRate);
        slowAttackCoef = onePoleCoef (0.3, sampleRate);
        slowReleaseCoef = onePoleCoef (rel * 4.0, sampleRate);
        settingsDirty = false;
    }

    double sampleRate = 96000.0;
    Settings settings;
    bool settingsDirty = true;

    float attackCoef = 0, releaseCoef = 0, slowAttackCoef = 0, slowReleaseCoef = 0;
    float envDb = 0, slowEnvDb = 0;
    std::array<DcBlocker, maxChannels> dcBlock;
    TubeStage tube;
};
} // namespace cruz
