#include "ReverbEngine.h"

#include <algorithm>
#include <cmath>

namespace cruzverb
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr int controlInterval = 32;

        // Tank delay lengths at a size scale of 1, chosen so no two share a simple ratio.
        constexpr std::array<double, ReverbEngine::numLines> lineDelaysMs {
            31.7, 37.1, 41.9, 46.3, 51.1, 55.7, 60.9, 65.3,
            70.1, 75.7, 80.3, 85.9, 90.1, 95.3, 101.9, 107.3
        };

        // Slightly different modulation speeds per line so the movement never lines up.
        constexpr std::array<float, ReverbEngine::numLines> lineRateSpread {
            0.61f, 1.37f, 0.83f, 1.12f, 0.71f, 1.29f, 0.94f, 1.05f,
            0.66f, 1.21f, 0.88f, 1.43f, 0.77f, 1.16f, 0.99f, 1.33f
        };

        // Output tap signs. The two rows are orthogonal so left and right are decorrelated.
        constexpr std::array<float, ReverbEngine::numLines> outSignL {
            1, -1, 1, 1, -1, 1, -1, -1, 1, 1, -1, 1, -1, 1, 1, -1
        };
        constexpr std::array<float, ReverbEngine::numLines> outSignR {
            1, 1, -1, 1, 1, -1, -1, 1, -1, 1, 1, -1, -1, -1, 1, -1
        };

        // Early reflection pattern at a size scale of 1 (milliseconds and gains).
        constexpr int numTaps = 14;
        constexpr std::array<std::array<float, numTaps>, 2> tapMs {{
            { 4.3f, 7.9f, 11.2f, 15.6f, 19.1f, 23.7f, 28.4f, 33.0f, 38.6f, 44.1f, 51.3f, 58.9f, 66.2f, 74.8f },
            { 5.1f, 8.6f, 12.4f, 14.9f, 20.3f, 24.8f, 27.1f, 34.6f, 40.2f, 46.3f, 49.7f, 57.4f, 68.1f, 72.3f }
        }};
        constexpr std::array<std::array<float, numTaps>, 2> tapGain {{
            { 0.42f, -0.38f, 0.35f, 0.31f, -0.29f, 0.26f, -0.23f, 0.21f, 0.18f, -0.16f, 0.14f, -0.12f, 0.10f, 0.08f },
            { 0.40f, 0.37f, -0.34f, 0.32f, 0.28f, -0.25f, 0.24f, -0.20f, 0.19f, 0.15f, -0.13f, 0.12f, -0.09f, 0.08f }
        }};

        constexpr std::array<double, 4> diffuserMs { 4.77, 3.59, 12.73, 9.31 };
        constexpr float diffusion = 0.62f;

        constexpr double maxPredelayMs = 500.0;
        constexpr double maxSizeScale = 1.6;

        // In-place fast Walsh-Hadamard transform, scaled to be energy preserving.
        void hadamard16 (std::array<float, ReverbEngine::numLines>& x) noexcept
        {
            for (int span = 1; span < ReverbEngine::numLines; span <<= 1)
                for (int i = 0; i < ReverbEngine::numLines; i += span << 1)
                    for (int j = i; j < i + span; ++j)
                    {
                        const auto a = x[(size_t) j], b = x[(size_t) (j + span)];
                        x[(size_t) j] = a + b;
                        x[(size_t) (j + span)] = a - b;
                    }

            for (auto& v : x)
                v *= 0.25f;
        }

        float onePoleCoeff (double cutoff, double fs) noexcept
        {
            return (float) (1.0 - std::exp (-2.0 * pi * cutoff / fs));
        }
    }

    //==============================================================================
    void ReverbEngine::DelayLine::setMaxDelay (int samples)
    {
        int size = 1;
        while (size < samples + 4)
            size <<= 1;

        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void ReverbEngine::DelayLine::clear() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    }

    float ReverbEngine::DelayLine::readLinear (float delay) const noexcept
    {
        const auto i = (int) delay;
        const auto t = delay - (float) i;
        const auto a = at (i), b = at (i + 1);
        return a + t * (b - a);
    }

    float ReverbEngine::DelayLine::readHermite (float delay) const noexcept
    {
        const auto i = (int) delay;
        const auto t = delay - (float) i;
        const auto p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);

        const auto c1 = 0.5f * (p2 - p0);
        const auto c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const auto c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
        return ((c3 * t + c2) * t + c1) * t + p1;
    }

    bool ReverbEngine::Smoothed::approach (float targetValue, float coeff) noexcept
    {
        const auto diff = targetValue - current;

        if (std::abs (diff) <= 1.0e-6f * std::max (1.0f, std::abs (targetValue)))
        {
            const auto changed = std::abs (diff) > 0.0f;
            current = targetValue;
            return changed;
        }

        current += coeff * diff;
        return true;
    }

    //==============================================================================
    double ReverbEngine::sizeScaleFor (double space) noexcept
    {
        // 0 % is a small booth, 100 % a very large hall.
        constexpr double minScale = 0.15;
        return minScale * std::pow (maxSizeScale / minScale, std::clamp (space, 0.0, 100.0) / 100.0);
    }

    double ReverbEngine::referenceDelaySeconds (double space) noexcept
    {
        return lineDelaysMs[numLines / 2] * 0.001 * sizeScaleFor (space);
    }

    ReverbEngine::LoopFilter ReverbEngine::designLoopFilter (double delaySamples, double fs, double decaySeconds,
                                                             double lowFreqHz, double lowMultiplier,
                                                             double highFreqHz, double highMultiplier,
                                                             bool freeze) noexcept
    {
        LoopFilter f;

        if (freeze)
            return f;

        // Attenuation per trip round the loop that gives a 60 dB decay in the wanted time.
        const auto dbFor = [&] (double seconds) { return -60.0 * delaySamples / (seconds * fs); };
        const auto midDb = dbFor (decaySeconds);
        const auto lowDb = dbFor (decaySeconds * lowMultiplier);
        const auto highDb = dbFor (decaySeconds * highMultiplier);

        f.gain = std::pow (10.0, midDb / 20.0);
        f.low = Biquad::Coeffs::lowShelf (fs, lowFreqHz, lowDb - midDb);
        f.high = Biquad::Coeffs::highShelf (fs, highFreqHz, highDb - midDb);

        // Overlapping shelves can bulge above their end values. Keep every frequency
        // strictly below unity loop gain so the reverb can never run away.
        const auto ceiling = std::min (0.9999, std::pow (10.0, std::max ({ midDb, lowDb, highDb }) / 20.0) * 1.0001);
        double peak = 0.0;

        for (int k = 0; k <= 48; ++k)
        {
            const auto freq = 20.0 * std::pow (1000.0, k / 48.0);
            if (freq < fs * 0.5)
                peak = std::max (peak, f.magnitude (freq, fs));
        }

        if (peak > ceiling)
            f.gain *= ceiling / peak;

        return f;
    }

    ReverbEngine::PostEq ReverbEngine::designPostEq (double fs, double lowCutHz, double highCutHz,
                                                     double lowShelfFreq, double lowShelfGain,
                                                     double highShelfFreq, double highShelfGain) noexcept
    {
        PostEq p;
        p.lowCut = lowCutHz > 10.5 ? Biquad::Coeffs::highPass (fs, lowCutHz) : Biquad::Coeffs::identity();
        p.highCut = highCutHz < 21500.0 && highCutHz < fs * 0.49 ? Biquad::Coeffs::lowPass (fs, highCutHz)
                                                                  : Biquad::Coeffs::identity();
        p.lowShelf = Biquad::Coeffs::lowShelf (fs, lowShelfFreq, lowShelfGain);
        p.highShelf = Biquad::Coeffs::highShelf (fs, highShelfFreq, highShelfGain);
        return p;
    }

    //==============================================================================
    void ReverbEngine::prepare (double sampleRate)
    {
        fs = sampleRate;

        const auto maxTapMs = (double) std::max (tapMs[0].back(), tapMs[1].back());
        for (auto& d : inputDelay)
            d.setMaxDelay ((int) std::ceil ((maxPredelayMs + maxTapMs * maxSizeScale + 5.0) * 0.001 * fs));

        for (size_t ch = 0; ch < 2; ++ch)
            for (size_t s = 0; s < diffuserMs.size(); ++s)
            {
                // Offset the right channel slightly for a wider image.
                const auto ms = diffuserMs[s] * (ch == 0 ? 1.0 : 1.093);
                diffuserLengths[ch][s] = std::max (1, (int) std::round (ms * 0.001 * fs));
                diffusers[ch][s].setMaxDelay (diffuserLengths[ch][s] + 1);
            }

        for (size_t i = 0; i < lines.size(); ++i)
            lines[i].setMaxDelay ((int) std::ceil ((lineDelaysMs[i] * maxSizeScale + 4.0) * 0.001 * fs));

        fastCoeff = (float) (1.0 - std::exp (-controlInterval / (0.03 * fs)));
        slowCoeff = (float) (1.0 - std::exp (-controlInterval / (0.15 * fs)));
        duckAttack = (float) (1.0 - std::exp (-1.0 / (0.004 * fs)));
        duckRelease = (float) (1.0 - std::exp (-1.0 / (0.25 * fs)));

        reset();
    }

    void ReverbEngine::reset() noexcept
    {
        toneState = {};
        for (auto& d : inputDelay) d.clear();
        for (auto& ch : diffusers) for (auto& d : ch) d.clear();
        for (auto& d : lines) d.clear();
        for (auto& f : lowShelves) f.reset();
        for (auto& f : highShelves) f.reset();
        for (auto& ch : postEq) for (auto& f : ch) f.reset();

        modOffset = {};
        modTarget = {};
        for (size_t i = 0; i < modPhase.size(); ++i)
            modPhase[i] = (float) (2.0 * pi * (double) i / numLines);

        duckEnv = 0.0f;
        modCounter = 0;

        updateSmoothedValues (true);
        updateLoopFilters();
        updatePostEq();
    }

    void ReverbEngine::updateSmoothedValues (bool snap) noexcept
    {
        const auto& t = target;
        const auto dist = std::clamp (t.distance, 0.0f, 100.0f) / 100.0f;
        const auto chr = std::clamp (t.character, 0.0f, 100.0f) / 100.0f;
        const auto brightHz = 1200.0 * std::pow (20000.0 / 1200.0, std::clamp (t.brightness, 0.0f, 100.0f) / 100.0);

        // Longer tails build up more energy, so trim the late level as the decay grows.
        const auto decayTrim = 1.0f / std::sqrt (1.0f + 0.35f * std::max (0.1f, t.decaySeconds));

        const float targets[] = {
            (float) sizeScaleFor (t.space),
            std::max (0.05f, t.decaySeconds),
            t.decayLowFreq, std::max (0.05f, t.decayLowMult),
            t.decayHighFreq, std::max (0.05f, t.decayHighMult),
            (float) (std::clamp ((double) t.predelayMs, 0.0, maxPredelayMs) * 0.001 * fs),
            0.75f * std::pow (1.0f - dist, 1.3f) + 0.04f,
            1.4f * (0.45f + 0.75f * dist) * decayTrim,
            std::clamp (t.width, 0.0f, 200.0f) / 100.0f,
            onePoleCoeff (brightHz, fs),
            (float) ((0.04 + 1.1 * std::pow (chr, 1.5)) * 0.001 * fs),
            0.15f + 0.85f * chr,
            t.lowCut, t.highCut, t.lowShelfFreq, t.lowShelfGain, t.highShelfFreq, t.highShelfGain,
            std::clamp (t.ducking, 0.0f, 100.0f) / 100.0f,
            t.freeze ? 1.0f : 0.0f
        };

        Smoothed* values[] = { &sizeScale, &decay, &lowFreq, &lowMult, &highFreq, &highMult, &predelay,
                               &erGain, &lateGain, &width, &brightCoeff, &depth, &rate,
                               &lowCut, &highCut, &lsFreq, &lsGain, &hsFreq, &hsGain, &duckAmount, &freezeAmount };

        static_assert (std::size (targets) == std::size (values));

        if (snap)
        {
            for (size_t i = 0; i < std::size (values); ++i)
                values[i]->snap (targets[i]);

            loopDirty = postDirty = true;
        }
        else
        {
            // Size moves slowly because changing delay lengths bends pitch.
            if (sizeScale.approach (targets[0], slowCoeff))
                loopDirty = true;

            for (size_t i = 1; i <= 5; ++i)
                if (values[i]->approach (targets[i], fastCoeff))
                    loopDirty = true;

            for (size_t i = 6; i <= 12; ++i)
                values[i]->approach (targets[i], i == 6 ? slowCoeff : fastCoeff);

            for (size_t i = 13; i <= 18; ++i)
                if (values[i]->approach (targets[i], fastCoeff))
                    postDirty = true;

            duckAmount.approach (targets[19], fastCoeff);

            if (freezeAmount.approach (targets[20], fastCoeff))
                loopDirty = true;
        }

        for (size_t i = 0; i < lines.size(); ++i)
            lineLength[i] = (float) (lineDelaysMs[i] * 0.001 * fs * sizeScale.current);
    }

    void ReverbEngine::updateLoopFilters() noexcept
    {
        const auto frozen = freezeAmount.current > 0.5f;

        for (size_t i = 0; i < lines.size(); ++i)
        {
            const auto f = designLoopFilter (lineLength[i], fs, decay.current, lowFreq.current, lowMult.current,
                                             highFreq.current, highMult.current, frozen);
            lowShelves[i].setCoeffs (f.low);
            highShelves[i].setCoeffs (f.high);
            lineGain[i] = (float) f.gain;
        }

        loopDirty = false;
    }

    void ReverbEngine::updatePostEq() noexcept
    {
        const auto p = designPostEq (fs, lowCut.current, highCut.current, lsFreq.current, lsGain.current,
                                     hsFreq.current, hsGain.current);

        for (auto& ch : postEq)
        {
            ch[0].setCoeffs (p.lowCut);
            ch[1].setCoeffs (p.lowShelf);
            ch[2].setCoeffs (p.highShelf);
            ch[3].setCoeffs (p.highCut);
        }

        postDirty = false;
    }

    //==============================================================================
    void ReverbEngine::process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
    {
        std::array<float, numLines> y {};
        std::array<float, 2> x {}, diffused {}, early {}, wet {};

        for (int n = 0; n < numSamples; ++n)
        {
            if (modCounter == 0)
            {
                updateSmoothedValues (false);

                if (loopDirty)
                    updateLoopFilters();

                if (postDirty)
                    updatePostEq();

                const auto phaseStep = (float) (2.0 * pi * controlInterval / fs) * rate.current;

                for (size_t i = 0; i < lines.size(); ++i)
                {
                    modPhase[i] += phaseStep * lineRateSpread[i];
                    if (modPhase[i] > (float) (2.0 * pi))
                        modPhase[i] -= (float) (2.0 * pi);

                    modTarget[i] = depth.current * std::sin (modPhase[i]);
                    modStep[i] = (modTarget[i] - modOffset[i]) / (float) controlInterval;
                }

                modCounter = controlInterval;
            }

            --modCounter;

            x[0] = inL[n];
            x[1] = inR[n];

            const auto peak = std::max (std::abs (x[0]), std::abs (x[1]));
            duckEnv += (peak > duckEnv ? duckAttack : duckRelease) * (peak - duckEnv);

            const auto inputGain = 1.0f - freezeAmount.current;
            const auto erScale = sizeScale.current * (float) (0.001 * fs);

            for (size_t ch = 0; ch < 2; ++ch)
            {
                toneState[ch] += brightCoeff.current * (x[ch] - toneState[ch]);
                inputDelay[ch].push (toneState[ch]);

                const auto pre = predelay.current + 1.0f;
                float e = 0.0f;

                for (size_t k = 0; k < (size_t) numTaps; ++k)
                    e += tapGain[ch][k] * inputDelay[ch].readLinear (pre + tapMs[ch][k] * erScale);

                early[ch] = e;

                // Smear the input into a dense cloud before it enters the tank.
                auto d = inputDelay[ch].readLinear (pre) * inputGain;

                for (size_t s = 0; s < diffuserMs.size(); ++s)
                {
                    auto& ap = diffusers[ch][s];
                    const auto delayed = ap.at (diffuserLengths[ch][s]);
                    const auto v = d - diffusion * delayed;
                    ap.push (v);
                    d = delayed + diffusion * v;
                }

                diffused[ch] = d;
            }

            // Tank: read every line, filter it for its decay, mix through the Hadamard matrix.
            float lateL = 0.0f, lateR = 0.0f;

            for (size_t i = 0; i < lines.size(); ++i)
            {
                modOffset[i] += modStep[i];
                auto v = lines[i].readHermite (std::max (2.0f, lineLength[i] + modOffset[i]));
                v = highShelves[i].process (lowShelves[i].process (v)) * lineGain[i];
                y[i] = v;
                lateL += outSignL[i] * v;
                lateR += outSignR[i] * v;
            }

            hadamard16 (y);

            for (size_t i = 0; i < lines.size(); ++i)
            {
                const auto in = (i & 1) == 0 ? diffused[0] : diffused[1];
                lines[i].push (y[i] + ((i & 2) == 0 ? in : -in) * 0.5f);
            }

            wet[0] = erGain.current * early[0] + lateGain.current * 0.25f * lateL;
            wet[1] = erGain.current * early[1] + lateGain.current * 0.25f * lateR;

            // Width on the wet signal only.
            const auto mid = 0.5f * (wet[0] + wet[1]);
            const auto side = 0.5f * (wet[0] - wet[1]) * width.current;
            wet[0] = mid + side;
            wet[1] = mid - side;

            const auto duckGain = 1.0f / (1.0f + duckAmount.current * 12.0f * duckEnv);

            for (size_t ch = 0; ch < 2; ++ch)
            {
                auto v = wet[ch];
                for (auto& f : postEq[ch])
                    v = f.process (v);
                wet[ch] = v * duckGain;
            }

            outL[n] = wet[0];
            outR[n] = wet[1];
        }
    }
}
