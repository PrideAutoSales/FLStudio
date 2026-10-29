#pragma once

// Optical leveling amplifier model.
//
// The classic opto leveler works like this: the program signal lights an
// electroluminescent panel, a photoresistor next to it drops in resistance
// as it brightens, and that resistor shunts the audio to ground. The
// detector listens to the output of the gain stage (feedback), which gives
// the very soft knee and the gentle, level-dependent ratio. The photocell is
// what makes it sound the way it does:
//   * attack of roughly 10 ms, so the first edge of a transient gets through;
//   * a two-stage release: about half of the gain reduction recovers in
//     ~60 ms, the rest drifts back over one to several seconds;
//   * memory: the longer and harder it has been working, the slower the
//     tail of the release.
// After the cell, a tube make-up stage adds a little second harmonic.

#include "CruzDSP.h"
#include <array>

namespace cruz
{
class OptoModel
{
public:
    static constexpr int maxChannels = 2;

    struct Settings
    {
        float peakReduction = 0.0f; // 0..100, how hard the panel is driven
        float gainDb = 0.0f;        // make-up gain
        bool limitMode = false;     // firmer ratio and a little more drive
        float emphasis = 0.0f;      // 0..100, sidechain high-frequency emphasis
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;

        panelCoef       = onePoleCoef (0.002, sampleRate);
        fastAttackCoef  = onePoleCoef (0.006, sampleRate);
        fastReleaseCoef = onePoleCoef (0.087, sampleRate); // ~60 ms to half
        slowAttackCoef  = onePoleCoef (0.040, sampleRate);
        memoryUpCoef    = onePoleCoef (2.0, sampleRate);
        memoryDownCoef  = onePoleCoef (8.0, sampleRate);
        gainSmoothCoef  = onePoleCoef (0.020, sampleRate);

        for (auto& f : emphasisFilter)
            f.prepare (sampleRate, 1500.0);
        for (auto& d : dcBlock)
            d.prepare (sampleRate);

        tube.setShape (0.2f, 0.12f);
        reset();
    }

    void reset() noexcept
    {
        panel = fast = slow = memory = 0.0f;
        prevReduced.fill (0.0f);
        lastGain = 1.0f;
        makeup = target.gainDb;
        for (auto& f : emphasisFilter)
            f.reset();
        for (auto& d : dcBlock)
            d.reset();
    }

    void setSettings (const Settings& s) noexcept { target = s; }

    // Processes up to two channels in place. Channels share one photocell.
    // Returns the deepest gain reduction in this block, in dB (<= 0).
    float process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, maxChannels);

        // Peak Reduction drives the panel. 0 leaves the cell dark for any
        // sane level, 100 starts working about 40 dB below full scale.
        const float drive = dbToGain (target.peakReduction * 0.5f - 10.0f + (target.limitMode ? 4.0f : 0.0f));
        const float exponent = target.limitMode ? 10.0f : 3.0f; // about 3:1 vs 10:1 in use, like the hardware
        const float emphasisAmount = target.emphasis * 0.03f;  // up to about +12 dB at the top
        const float makeupTarget = target.gainDb;

        float deepest = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            // Sidechain: rectified output of the gain cell (feedback).
            float sc = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float r = prevReduced[(size_t) ch];
                const float e = r + emphasisAmount * emphasisFilter[(size_t) ch].process (r);
                sc = std::max (sc, std::abs (e));
            }

            // Electroluminescent panel.
            panel += panelCoef * (sc * drive - panel);

            // Photocell: fast and slow parts, the slow one with memory.
            fast += (panel > fast ? fastAttackCoef : fastReleaseCoef) * (panel - fast);

            if (panel > slow)
                slow += slowAttackCoef * (panel - slow);
            else
                slow += slowReleaseCoef * (panel - slow);

            const float cell = 0.6f * fast + 0.4f * slow;
            const float gain = 1.0f / (1.0f + std::pow (cell, exponent));

            const float grDb = gainToDb (gain);
            const float depth = std::clamp (-grDb / 12.0f, 0.0f, 1.0f);
            memory += (depth > memory ? memoryUpCoef : memoryDownCoef) * (depth - memory);

            if ((i & 31) == 0)
                slowReleaseCoef = onePoleCoef (0.5 + 5.0 * memory, sampleRate);

            makeup += gainSmoothCoef * (makeupTarget - makeup);
            const float makeupGain = dbToGain (makeup);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float reduced = channels[ch][i] * gain;
                prevReduced[(size_t) ch] = reduced;
                channels[ch][i] = dcBlock[(size_t) ch].process (tube.process (reduced * makeupGain));
            }

            deepest = std::min (deepest, grDb);
            lastGain = gain;
        }

        return deepest;
    }

    float getCurrentGain() const noexcept { return lastGain; }

private:
    double sampleRate = 48000.0;
    Settings target;

    float panelCoef = 0, fastAttackCoef = 0, fastReleaseCoef = 0, slowAttackCoef = 0;
    float slowReleaseCoef = 0.0001f, memoryUpCoef = 0, memoryDownCoef = 0, gainSmoothCoef = 0;

    float panel = 0, fast = 0, slow = 0, memory = 0, lastGain = 1.0f, makeup = 0.0f;
    std::array<float, maxChannels> prevReduced {};
    std::array<OnePoleHighPass, maxChannels> emphasisFilter;
    std::array<DcBlocker, maxChannels> dcBlock;
    TubeStage tube;
};
} // namespace cruz
