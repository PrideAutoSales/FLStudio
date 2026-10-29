#pragma once

#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <cmath>

// The whole Cruz Drive signal path. It has no plugin or UI code, so the test
// app can run it directly.
//
//   input -> crush compressor -> low cut (+ thump) -> tone tilt -> style pre-voicing
//         -> drive -> 4x oversampled waveshaper -> style post-voicing
//         -> high cut -> darkness -> DC block -> auto gain -> dry/wet mix -> output
namespace cruzdrive
{
    inline constexpr int numStyles = 5;
    inline const juce::StringArray styleNames { "Warm", "Glass", "Iron", "Valve", "Fuzz" };

    // Signal level that counts as "nominal" for the shapers. A 0 dBFS peak enters
    // the curve at 0.5, so drive 0 only adds a light colour.
    inline constexpr float shaperReference = 0.5f;

    // Rational tanh approximation, within 0.0002 of std::tanh and much cheaper.
    inline float fastTanh (float x) noexcept
    {
        x = juce::jlimit (-4.97f, 4.97f, x);
        const auto x2 = x * x;
        const auto a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
        const auto b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
        return a / b;
    }

    //==============================================================================
    // Static transfer curves. Every curve has a slope of 1 at zero, so quiet
    // material passes at unity and only louder material bends.
    inline float shape (int style, float x) noexcept
    {
        switch (style)
        {
            case 0: // Warm: soft, slightly biased tanh, like a tape machine.
            {
                constexpr float bias = 0.18f;
                constexpr float offset = 0.1780810f; // tanh (bias)
                return fastTanh (x + bias) - offset;
            }

            case 1: // Glass: gentle rational curve, the negative half bends a bit sooner.
                return x >= 0.0f ? x / (1.0f + x) : x / (1.0f - 1.35f * x);

            case 2: // Iron: cubic soft clip with a bias, like a pushed transformer.
            {
                auto cubic = [] (float v)
                {
                    const auto u = juce::jlimit (-1.0f, 1.0f, v / 1.5f);
                    return 1.5f * (u - u * u * u / 3.0f);
                };
                constexpr float bias = 0.12f;
                constexpr float offset = 0.1197440f; // cubic (bias)
                return cubic (x + bias) - offset;
            }

            case 3: // Valve: a single-ended triode. The grid sits well off centre, so
                    // the curve bends far more on one side and second harmonic leads
                    // at every drive setting, and the two sides clip at different levels.
            {
                constexpr float bias = 0.6f;
                constexpr float offset = 0.5370496f;  // tanh (bias)
                constexpr float slope = 1.4053278f;   // 1 / (1 - tanh^2 (bias)), unity gain at zero
                return fastTanh (x * slope + bias) - offset;
            }

            case 4: // Fuzz: firm knee plus a squared term for gritty even harmonics.
            default:
            {
                const auto x2 = x * x;
                const auto y = x / std::sqrt (std::sqrt (1.0f + x2 * x2));
                return y + 0.25f * y * y;
            }
        }
    }

    // Tone shaping wrapped around each curve, so styles differ in which part of
    // the spectrum saturates first, not only in harmonic content.
    struct StyleVoicing
    {
        float preLowDb, preHighDb;   // shelves before the shaper
        float postLowDb, postHighDb; // shelves after the shaper
        float lowHz, highHz;         // shelf corner frequencies
        float postLowpassHz;         // final roll-off for this style
        float extraDriveDb;
    };

    inline constexpr std::array<StyleVoicing, numStyles> voicings {{
        { 0.0f, 4.0f,  1.5f, -5.0f,  90.0f, 3000.0f, 16000.0f, 0.0f }, // Warm
        { -2.0f, 6.0f, 2.0f, -4.0f, 150.0f, 2000.0f, 20000.0f, 0.0f }, // Glass
        { 7.0f, 0.0f, -7.0f, -1.0f, 150.0f, 5000.0f, 18000.0f, 0.0f }, // Iron
        { 1.0f, 2.0f, -1.0f, -2.0f, 200.0f, 4000.0f, 14000.0f, 0.0f }, // Valve
        { -3.0f, 3.0f, 2.0f, -4.0f, 250.0f, 2500.0f,  9000.0f, 6.0f }, // Fuzz
    }};

    //==============================================================================
    // Topology-preserving one-pole low-pass, used for shelves and the DC blocker.
    struct OnePole
    {
        float g = 0.0f, s = 0.0f;

        void setCutoff (float hz, float sampleRate) noexcept
        {
            const auto wc = std::tan (juce::MathConstants<float>::pi
                                      * juce::jmin (hz, 0.49f * sampleRate) / sampleRate);
            g = wc / (1.0f + wc);
        }

        float lowpass (float x) noexcept
        {
            const auto v = (x - s) * g;
            const auto y = v + s;
            s = y + v;
            return y;
        }

        void reset() noexcept { s = 0.0f; }
    };

    // Cytomic-style state variable filter (stable under fast modulation).
    struct Svf
    {
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f, k = 1.414f;
        float ic1 = 0.0f, ic2 = 0.0f;

        void set (float hz, float q, float sampleRate) noexcept
        {
            const auto g = std::tan (juce::MathConstants<float>::pi
                                     * juce::jmin (hz, 0.49f * sampleRate) / sampleRate);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        void tick (float v0, float& lp, float& hp) noexcept
        {
            const auto v3 = v0 - ic2;
            const auto v1 = a1 * ic1 + a2 * v3;
            const auto v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2;
            hp = v0 - k * v1 - v2;
        }

        float lowpass (float x) noexcept  { float lp, hp; tick (x, lp, hp); return lp; }
        float highpass (float x) noexcept { float lp, hp; tick (x, lp, hp); return hp; }
        void reset() noexcept { ic1 = ic2 = 0.0f; }
    };

    //==============================================================================
    struct Settings
    {
        float driveDb = 0.0f;     // 0..36
        int style = 0;
        float lowCutHz = 10.0f;   // <= lowCutOffHz means off
        bool thump = false;
        float tone = 0.0f;        // -1 dark .. +1 bright
        float highCutHz = 22000.0f; // >= highCutOffHz means off
        bool steep = false;
        bool punish = false;
        float crush = 0.0f;       // 0..1
        float darkness = 0.0f;    // 0..1
        bool autoGain = true;
        float mix = 1.0f;         // 0..1
        float outputDb = 0.0f;
    };

    inline constexpr float lowCutOffHz = 10.5f;
    inline constexpr float highCutOffHz = 21500.0f;

    //==============================================================================
    class Engine
    {
    public:
        static constexpr int oversamplingOrder = 3; // 2^3 = 8x
        static constexpr int chunkSize = 32;        // parameter update interval in samples
        static constexpr int maxChannels = 2;

        void prepare (double newSampleRate, int maxBlockSize, int numChannels)
        {
            sampleRate = (float) newSampleRate;
            maxBlock = juce::jmax (1, maxBlockSize);
            channels = juce::jlimit (1, maxChannels, numChannels);

            oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, (size_t) oversamplingOrder,
                juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false, true);
            oversampler->initProcessing ((size_t) chunkSize);

            mixer.prepare ({ newSampleRate, (juce::uint32) maxBlockSize, (juce::uint32) channels });
            mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
            mixer.setWetLatency ((float) getLatencySamples());

            const auto rampSeconds = 0.03;
            driveGain.reset (newSampleRate, rampSeconds);
            outputGain.reset (newSampleRate, rampSeconds);
            crushAmount.reset (newSampleRate, rampSeconds);
            toneAmount.reset (newSampleRate, rampSeconds);
            autoGainSmoothed.reset (newSampleRate, 0.05);

            // Detector and auto-gain time constants.
            attackCoeff = std::exp (-1.0f / (0.001f * sampleRate));
            releaseCoeff = std::exp (-1.0f / (0.080f * sampleRate));
            rmsCoeff = std::exp (-1.0f / (0.4f * sampleRate));

            for (auto& ch : state)
                ch.dcBlock.setCutoff (8.0f, sampleRate);

            reset();
            applySettings (current, true);
        }

        void reset()
        {
            if (oversampler != nullptr)
                oversampler->reset();
            mixer.reset();

            for (auto& ch : state)
                ch = ChannelState {};
            for (auto& ch : state)
                ch.dcBlock.setCutoff (8.0f, sampleRate);

            envelopeDb = -120.0f;
            inMeanSquare = outMeanSquare = 0.0f;
            autoGainSmoothed.setCurrentAndTargetValue (1.0f);
            lastVoicing = -1;
        }

        int getLatencySamples() const
        {
            return oversampler != nullptr ? (int) std::round (oversampler->getLatencyInSamples()) : 0;
        }

        void setSettings (const Settings& s) { applySettings (s, false); }

        void process (juce::dsp::AudioBlock<float> block)
        {
            // Hosts occasionally send more samples than promised; split those up.
            for (size_t start = 0; start < block.getNumSamples(); start += (size_t) maxBlock)
                processBlock (block.getSubBlock (start, juce::jmin ((size_t) maxBlock, block.getNumSamples() - start)));
        }

        // Readouts for the editor, refreshed every block.
        std::atomic<float> meterIn { 0.0f }, meterOut { 0.0f }, meterReductionDb { 0.0f };

    private:
        void processBlock (juce::dsp::AudioBlock<float> block)
        {
            const auto numSamples = (int) block.getNumSamples();
            const auto numCh = juce::jmin ((int) block.getNumChannels(), channels);
            if (numSamples == 0 || numCh == 0 || oversampler == nullptr)
                return;

            auto active = block.getSubsetChannelBlock (0, (size_t) numCh);
            mixer.pushDrySamples (active);

            float inPeak = 0.0f;
            for (int c = 0; c < numCh; ++c)
            {
                const auto range = active.getSingleChannelBlock ((size_t) c).findMinAndMax();
                inPeak = juce::jmax (inPeak, range.getEnd(), -range.getStart());
            }

            float maxReduction = 0.0f;

            for (int start = 0; start < numSamples; start += chunkSize)
            {
                const auto len = juce::jmin (chunkSize, numSamples - start);
                auto chunk = active.getSubBlock ((size_t) start, (size_t) len);
                updateFilters();
                maxReduction = juce::jmax (maxReduction, processPre (chunk, numCh));

                auto up = oversampler->processSamplesUp (chunk);
                for (size_t c = 0; c < up.getNumChannels(); ++c)
                    shapeBlock (up.getChannelPointer (c), (int) up.getNumSamples());
                oversampler->processSamplesDown (chunk);

                processPost (chunk, numCh);
            }

            mixer.mixWetSamples (active);

            float outPeak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                const auto g = outputGain.getNextValue();
                for (int c = 0; c < numCh; ++c)
                {
                    auto& v = active.getChannelPointer ((size_t) c)[i];
                    v *= g;
                    if (! std::isfinite (v))
                        v = 0.0f;
                    outPeak = juce::jmax (outPeak, std::abs (v));
                }
            }

            meterIn.store (inPeak);
            meterOut.store (outPeak);
            meterReductionDb.store (maxReduction);
        }

        // Picks the curve once per block so the inner loop has no branch on style.
        void shapeBlock (float* d, int n) const noexcept
        {
            constexpr auto makeup = 1.0f / shaperReference;
            auto run = [d, n] (auto curve)
            {
                for (int i = 0; i < n; ++i)
                    d[i] = curve (d[i]) * makeup;
            };

            switch (current.style)
            {
                case 0:  run ([] (float x) { return shape (0, x); }); break;
                case 1:  run ([] (float x) { return shape (1, x); }); break;
                case 2:  run ([] (float x) { return shape (2, x); }); break;
                case 3:  run ([] (float x) { return shape (3, x); }); break;
                default: run ([] (float x) { return shape (4, x); }); break;
            }
        }

        struct ChannelState
        {
            Svf lowCut;
            OnePole toneLow, toneHigh;
            OnePole preLow, preHigh, postLow, postHigh, postToneLow, postToneHigh;
            Svf styleLowpass;
            std::array<Svf, 2> highCut;
            Svf darkness;
            OnePole dcBlock;
        };

        static float shelfLow (OnePole& f, float x, float gain) noexcept
        {
            return x + (gain - 1.0f) * f.lowpass (x);
        }

        static float shelfHigh (OnePole& f, float x, float gain) noexcept
        {
            return x + (gain - 1.0f) * (x - f.lowpass (x));
        }

        void applySettings (const Settings& s, bool immediate)
        {
            current = s;
            current.style = juce::jlimit (0, numStyles - 1, s.style);
            const auto& v = voicings[(size_t) current.style];

            const auto totalDriveDb = s.driveDb + (s.punish ? 20.0f : 0.0f) + v.extraDriveDb;
            const auto driveTarget = juce::Decibels::decibelsToGain (totalDriveDb) * shaperReference;
            const auto outTarget = juce::Decibels::decibelsToGain (s.outputDb);

            if (immediate)
            {
                driveGain.setCurrentAndTargetValue (driveTarget);
                outputGain.setCurrentAndTargetValue (outTarget);
                crushAmount.setCurrentAndTargetValue (s.crush);
                toneAmount.setCurrentAndTargetValue (s.tone);
            }
            else
            {
                driveGain.setTargetValue (driveTarget);
                outputGain.setTargetValue (outTarget);
                crushAmount.setTargetValue (s.crush);
                toneAmount.setTargetValue (s.tone);
            }

            mixer.setWetMixProportion (juce::jlimit (0.0f, 1.0f, s.mix));
        }

        void updateFilters()
        {
            const std::array<float, 6> key { current.lowCutHz, current.thump ? 1.0f : 0.0f, current.highCutHz,
                                             current.steep ? 1.0f : 0.0f, current.darkness, (float) current.style };
            if (key == lastFilterKey && lastVoicing == current.style)
                return;
            lastFilterKey = key;

            const auto& v = voicings[(size_t) current.style];
            const auto lowCutOn = current.lowCutHz > lowCutOffHz;
            const auto highCutOn = current.highCutHz < highCutOffHz;
            const auto darkHz = 20000.0f * std::pow (600.0f / 20000.0f, std::pow (current.darkness, 0.8f));

            // Butterworth 4th order uses two sections with these Qs; 2nd order uses one.
            const auto q1 = current.steep ? 0.5412f : 0.7071f;
            const auto q2 = 1.3066f;

            for (int c = 0; c < channels; ++c)
            {
                auto& ch = state[(size_t) c];

                if (lowCutOn)
                    ch.lowCut.set (current.lowCutHz, current.thump ? 2.2f : 0.7071f, sampleRate);
                if (highCutOn)
                {
                    ch.highCut[0].set (current.highCutHz, q1, sampleRate);
                    ch.highCut[1].set (current.highCutHz, q2, sampleRate);
                }
                ch.darkness.set (darkHz, 0.6f, sampleRate);

                if (lastVoicing != current.style)
                {
                    ch.preLow.setCutoff (v.lowHz, sampleRate);
                    ch.postLow.setCutoff (v.lowHz, sampleRate);
                    ch.preHigh.setCutoff (v.highHz, sampleRate);
                    ch.postHigh.setCutoff (v.highHz, sampleRate);
                    ch.styleLowpass.set (v.postLowpassHz, 0.7071f, sampleRate);
                    ch.toneLow.setCutoff (800.0f, sampleRate);
                    ch.toneHigh.setCutoff (800.0f, sampleRate);
                    ch.postToneLow.setCutoff (800.0f, sampleRate);
                    ch.postToneHigh.setCutoff (800.0f, sampleRate);
                }
            }
            lastVoicing = current.style;
        }

        // Crush, filters and drive at the host rate. Returns the largest gain
        // reduction in dB.
        float processPre (juce::dsp::AudioBlock<float>& chunk, int numCh)
        {
            const auto& v = voicings[(size_t) current.style];
            const auto preLowG = juce::Decibels::decibelsToGain (v.preLowDb);
            const auto preHighG = juce::Decibels::decibelsToGain (v.preHighDb);
            const auto lowCutOn = current.lowCutHz > lowCutOffHz;
            const auto n = (int) chunk.getNumSamples();
            float maxReduction = 0.0f;
            float toneLowG = 1.0f, toneHighG = 1.0f;

            for (int i = 0; i < n; ++i)
            {
                const auto crush = crushAmount.getNextValue();
                const auto tone = toneAmount.getNextValue();
                const auto drive = driveGain.getNextValue();

                float inSquare = 0.0f, detector = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    const auto x = chunk.getChannelPointer ((size_t) c)[i];
                    inSquare += x * x;
                    detector = juce::jmax (detector, std::abs (x));
                }
                inMeanSquare = rmsCoeff * inMeanSquare + (1.0f - rmsCoeff) * inSquare / (float) numCh;

                // Crush: a fast, deep compressor in front of the shaper. Threshold
                // drops and ratio climbs together, and makeup pushes the squashed
                // signal back into the drive.
                float crushGain = 1.0f;
                if (crush > 0.0005f)
                {
                    const auto levelDb = juce::Decibels::gainToDecibels (detector, -120.0f);
                    const auto coeff = levelDb > envelopeDb ? attackCoeff : releaseCoeff;
                    envelopeDb = coeff * envelopeDb + (1.0f - coeff) * levelDb;

                    const auto thresholdDb = -36.0f * crush;
                    const auto ratio = 1.0f + 19.0f * crush;
                    const auto slope = 1.0f - 1.0f / ratio;
                    constexpr float kneeDb = 6.0f;
                    const auto over = envelopeDb - thresholdDb;

                    float reductionDb = 0.0f;
                    if (over > kneeDb * 0.5f)
                        reductionDb = over * slope;
                    else if (over > -kneeDb * 0.5f)
                        reductionDb = slope * juce::square (over + kneeDb * 0.5f) / (2.0f * kneeDb);

                    const auto makeupDb = -thresholdDb * slope * 0.7f;
                    crushGain = juce::Decibels::decibelsToGain (makeupDb - reductionDb);
                    maxReduction = juce::jmax (maxReduction, reductionDb);
                }
                else
                {
                    envelopeDb = -120.0f;
                }

                // Tone moves slowly, so its shelf gains are refreshed every 8 samples.
                if ((i & 7) == 0)
                {
                    toneLowG = juce::Decibels::decibelsToGain (-6.0f * tone);
                    toneHighG = juce::Decibels::decibelsToGain (6.0f * tone);
                }

                for (int c = 0; c < numCh; ++c)
                {
                    auto& ch = state[(size_t) c];
                    auto x = chunk.getChannelPointer ((size_t) c)[i] * crushGain;

                    if (lowCutOn)
                        x = ch.lowCut.highpass (x);

                    x = shelfLow (ch.toneLow, x, toneLowG);
                    x = shelfHigh (ch.toneHigh, x, toneHighG);
                    x = shelfLow (ch.preLow, x, preLowG);
                    x = shelfHigh (ch.preHigh, x, preHighG);

                    chunk.getChannelPointer ((size_t) c)[i] = x * drive;
                }
            }

            return maxReduction;
        }

        void processPost (juce::dsp::AudioBlock<float>& chunk, int numCh)
        {
            const auto& v = voicings[(size_t) current.style];
            const auto postLowG = juce::Decibels::decibelsToGain (v.postLowDb);
            const auto postHighG = juce::Decibels::decibelsToGain (v.postHighDb);
            const auto highCutOn = current.highCutHz < highCutOffHz;
            const auto darkOn = current.darkness > 0.001f;
            const auto tone = toneAmount.getCurrentValue();

            // Undo part of the tone tilt after the shaper, so Tone mostly changes
            // what distorts rather than only the final EQ.
            const auto postToneLowG = juce::Decibels::decibelsToGain (2.5f * tone);
            const auto postToneHighG = juce::Decibels::decibelsToGain (-2.5f * tone);
            const auto n = (int) chunk.getNumSamples();

            for (int i = 0; i < n; ++i)
            {
                float outSquare = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    auto& ch = state[(size_t) c];
                    auto x = chunk.getChannelPointer ((size_t) c)[i];

                    x = shelfLow (ch.postLow, x, postLowG);
                    x = shelfHigh (ch.postHigh, x, postHighG);
                    x = shelfLow (ch.postToneLow, x, postToneLowG);
                    x = shelfHigh (ch.postToneHigh, x, postToneHighG);
                    x = ch.styleLowpass.lowpass (x);

                    if (highCutOn)
                    {
                        x = ch.highCut[0].lowpass (x);
                        if (current.steep)
                            x = ch.highCut[1].lowpass (x);
                    }

                    if (darkOn)
                        x = ch.darkness.lowpass (x);

                    x -= ch.dcBlock.lowpass (x);

                    chunk.getChannelPointer ((size_t) c)[i] = x;
                    outSquare += x * x;
                }

                outMeanSquare = rmsCoeff * outMeanSquare + (1.0f - rmsCoeff) * outSquare / (float) numCh;

                // Auto gain: match the wet signal's loudness to the input over a
                // slow window. Hold the last value through silence.
                if (current.autoGain)
                {
                    if (inMeanSquare > 1.0e-7f && outMeanSquare > 1.0e-9f)
                        autoGainSmoothed.setTargetValue (juce::jlimit (0.0158f, 4.0f,
                                                                       std::sqrt (inMeanSquare / outMeanSquare)));
                }
                else
                {
                    autoGainSmoothed.setTargetValue (1.0f);
                }

                const auto g = autoGainSmoothed.getNextValue();
                for (int c = 0; c < numCh; ++c)
                    chunk.getChannelPointer ((size_t) c)[i] *= g;
            }
        }

        float sampleRate = 44100.0f;
        int maxBlock = 512;
        int channels = 2;
        Settings current;
        int lastVoicing = -1;
        std::array<float, 6> lastFilterKey {};

        std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
        juce::dsp::DryWetMixer<float> mixer { 256 };
        std::array<ChannelState, maxChannels> state;

        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveGain { 1.0f }, outputGain { 1.0f };
        juce::SmoothedValue<float> crushAmount, toneAmount;
        juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> autoGainSmoothed { 1.0f };

        float attackCoeff = 0.0f, releaseCoeff = 0.0f, rmsCoeff = 0.0f;
        float envelopeDb = -120.0f;
        float inMeanSquare = 0.0f, outMeanSquare = 0.0f;
    };
}
