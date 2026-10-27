#pragma once

#include <array>
#include <vector>

#include "Biquad.h"

namespace cruzverb
{
    // Everything the engine needs to know, in plain units.
    struct ReverbSettings
    {
        float space = 45.0f;          // %, room size
        float decaySeconds = 2.2f;    // RT60 in the mid band
        float predelayMs = 12.0f;
        float distance = 35.0f;       // %, 0 = close (more early reflections), 100 = far (mostly tail)
        float brightness = 60.0f;     // %, tone of the sound fed into the room
        float character = 30.0f;      // %, 0 = clean and still, 100 = lush and moving
        float width = 100.0f;         // %, 0 = mono, 200 = extra wide

        float decayLowFreq = 250.0f;  // Decay EQ: low band corner and decay-time multiplier
        float decayLowMult = 1.2f;
        float decayHighFreq = 4500.0f; // Decay EQ: high band corner and decay-time multiplier
        float decayHighMult = 0.5f;

        float lowCut = 10.0f;         // Post EQ, applied to the reverb only
        float highCut = 22000.0f;
        float lowShelfFreq = 200.0f;
        float lowShelfGain = 0.0f;
        float highShelfFreq = 6000.0f;
        float highShelfGain = 0.0f;

        float ducking = 0.0f;         // %, how much the dry signal pushes the reverb down
        bool freeze = false;
    };

    // Feedback delay network reverb: pre-delay, early reflections, input diffusion and a
    // 16-line modulated FDN with per-line decay filters, followed by width and post EQ.
    // Produces the wet signal only; the caller mixes it with the dry signal.
    class ReverbEngine
    {
    public:
        static constexpr int numLines = 16;

        void prepare (double sampleRate);
        void reset() noexcept;
        void setSettings (const ReverbSettings& s) noexcept { target = s; }

        // Inputs and outputs may not alias. Any block size is fine.
        void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept;

        // Shared with the editor so the display draws exactly what the engine does.
        struct LoopFilter
        {
            Biquad::Coeffs low, high;
            double gain = 1.0;

            double magnitude (double freq, double fs) const noexcept
            {
                return gain * low.magnitude (freq, fs) * high.magnitude (freq, fs);
            }
        };

        static double sizeScaleFor (double space) noexcept;
        static double referenceDelaySeconds (double space) noexcept;
        static LoopFilter designLoopFilter (double delaySamples, double fs, double decaySeconds,
                                            double lowFreq, double lowMult,
                                            double highFreq, double highMult, bool freeze) noexcept;

        struct PostEq { Biquad::Coeffs lowCut, lowShelf, highShelf, highCut; };
        static PostEq designPostEq (double fs, double lowCut, double highCut,
                                    double lowShelfFreq, double lowShelfGain,
                                    double highShelfFreq, double highShelfGain) noexcept;

    private:
        class DelayLine
        {
        public:
            void setMaxDelay (int samples);
            void clear() noexcept;
            void push (float x) noexcept { buffer[(size_t) writePos] = x; writePos = (writePos + 1) & mask; }
            float at (int delay) const noexcept { return buffer[(size_t) ((writePos - delay) & mask)]; }
            float readLinear (float delay) const noexcept;
            float readHermite (float delay) const noexcept;

        private:
            std::vector<float> buffer;
            int mask = 0, writePos = 0;
        };

        struct Smoothed
        {
            float current = 0.0f;
            void snap (float v) noexcept { current = v; }
            bool approach (float targetValue, float coeff) noexcept;
        };

        void updateSmoothedValues (bool snap) noexcept;
        void updateLoopFilters() noexcept;
        void updatePostEq() noexcept;

        double fs = 44100.0;
        ReverbSettings target;

        // Smoothed control values.
        Smoothed sizeScale, decay, lowFreq, lowMult, highFreq, highMult, predelay, erGain, lateGain,
                 width, brightCoeff, depth, rate, lowCut, highCut, lsFreq, lsGain, hsFreq, hsGain,
                 duckAmount, freezeAmount;
        float fastCoeff = 0.1f, slowCoeff = 0.05f;
        bool loopDirty = true, postDirty = true;

        // Input stage.
        std::array<float, 2> toneState {};
        std::array<DelayLine, 2> inputDelay;     // pre-delay and early reflection taps
        std::array<std::array<DelayLine, 4>, 2> diffusers;
        std::array<std::array<int, 4>, 2> diffuserLengths {};

        // Tank.
        std::array<DelayLine, numLines> lines;
        std::array<Biquad, numLines> lowShelves, highShelves;
        std::array<float, numLines> lineGain {}, lineLength {}, modOffset {}, modTarget {}, modStep {}, modPhase {};

        // Output stage.
        std::array<std::array<Biquad, 4>, 2> postEq;
        float duckEnv = 0.0f, duckAttack = 0.0f, duckRelease = 0.0f;
        float freezeInput = 1.0f;
        int modCounter = 0;
    };
}
