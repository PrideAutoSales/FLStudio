// Offline checks for the Cruz Drive signal path. Build with -DCRUZDRIVE_TESTS=ON
// and run CruzDriveTests; it exits non-zero if any check fails.

#include "DriveEngine.h"

#include <cstdio>

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    int failures = 0;

    void check (bool ok, const char* what, double value)
    {
        std::printf ("%s  %-58s %10.2f\n", ok ? "PASS" : "FAIL", what, value);
        if (! ok)
            ++failures;
    }

    // Runs a signal through a fresh engine and returns the output.
    juce::AudioBuffer<float> run (const cruzdrive::Settings& s, const juce::AudioBuffer<float>& input,
                                  int* latency = nullptr, float* reduction = nullptr)
    {
        cruzdrive::Engine engine;
        engine.setSettings (s);
        engine.prepare (sampleRate, blockSize, input.getNumChannels());
        if (latency != nullptr)
            *latency = engine.getLatencySamples();

        juce::AudioBuffer<float> out (input);
        float maxReduction = 0.0f;
        for (int start = 0; start < out.getNumSamples(); start += blockSize)
        {
            const auto n = juce::jmin (blockSize, out.getNumSamples() - start);
            juce::dsp::AudioBlock<float> block (out);
            engine.process (block.getSubBlock ((size_t) start, (size_t) n));
            maxReduction = juce::jmax (maxReduction, engine.meterReductionDb.load());
        }
        if (reduction != nullptr)
            *reduction = maxReduction;
        return out;
    }

    juce::AudioBuffer<float> sine (double hz, float gainDb, double seconds, int channels = 2)
    {
        const auto n = (int) (seconds * sampleRate);
        juce::AudioBuffer<float> b (channels, n);
        const auto amp = juce::Decibels::decibelsToGain (gainDb);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < channels; ++c)
                b.setSample (c, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / sampleRate));
        return b;
    }

    double rmsDb (const juce::AudioBuffer<float>& b, int from)
    {
        double sum = 0.0;
        const auto n = b.getNumSamples() - from;
        for (int i = from; i < b.getNumSamples(); ++i)
            sum += (double) b.getSample (0, i) * b.getSample (0, i);
        return 10.0 * std::log10 (sum / n + 1.0e-30);
    }

    // Magnitude spectrum (dB, relative to the largest bin) of the last 2^order samples.
    std::vector<float> spectrum (const juce::AudioBuffer<float>& b, int order)
    {
        const auto size = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) size, juce::dsp::WindowingFunction<float>::blackmanHarris);
        std::vector<float> data ((size_t) size * 2, 0.0f);
        const auto offset = b.getNumSamples() - size;
        for (int i = 0; i < size; ++i)
            data[(size_t) i] = b.getSample (0, offset + i);
        window.multiplyWithWindowingTable (data.data(), (size_t) size);
        fft.performFrequencyOnlyForwardTransform (data.data());

        std::vector<float> mags ((size_t) size / 2);
        float peak = 1.0e-20f;
        for (size_t i = 0; i < mags.size(); ++i)
            peak = juce::jmax (peak, data[i]);
        for (size_t i = 0; i < mags.size(); ++i)
            mags[i] = juce::Decibels::gainToDecibels (data[i] / peak, -200.0f);
        return mags;
    }

    float levelAt (const std::vector<float>& mags, double hz, int order)
    {
        const auto bin = (int) std::round (hz * (1 << order) / sampleRate);
        float best = -200.0f;
        for (int i = bin - 3; i <= bin + 3; ++i)
            if (juce::isPositiveAndBelow (i, (int) mags.size()))
                best = juce::jmax (best, mags[(size_t) i]);
        return best;
    }
}

int main()
{
    using cruzdrive::Settings;

    // 1. Silence stays silent.
    {
        juce::AudioBuffer<float> silence (2, 48000);
        silence.clear();
        Settings s;
        s.driveDb = 36.0f;
        s.punish = true;
        s.crush = 1.0f;
        for (int style = 0; style < cruzdrive::numStyles; ++style)
        {
            s.style = style;
            const auto out = run (s, silence);
            const auto peak = out.getMagnitude (0, out.getNumSamples());
            check (peak < 1.0e-4f, ("silence in, silence out: " + cruzdrive::styleNames[style]).toRawUTF8(),
                   juce::Decibels::gainToDecibels (peak, -200.0f));
        }
    }

    // 2. Auto gain keeps loudness close to the input for every style.
    {
        const auto in = sine (220.0, -18.0f, 3.0);
        const auto inDb = rmsDb (in, 96000);
        for (int style = 0; style < cruzdrive::numStyles; ++style)
        {
            Settings s;
            s.style = style;
            s.driveDb = 25.0f;
            const auto diff = rmsDb (run (s, in), 96000) - inDb;
            check (std::abs (diff) < 1.5, ("auto gain level match (dB): " + cruzdrive::styleNames[style]).toRawUTF8(), diff);
        }
    }

    // 3. Mix at 0 is the dry signal, delayed by exactly the reported latency.
    {
        juce::Random rng (1);
        juce::AudioBuffer<float> noise (2, 48000);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < noise.getNumSamples(); ++i)
                noise.setSample (c, i, rng.nextFloat() * 0.5f - 0.25f);

        Settings s;
        s.mix = 0.0f;
        s.driveDb = 30.0f;
        int latency = 0;
        const auto out = run (s, noise, &latency);
        double err = 0.0, ref = 0.0;
        for (int i = latency; i < noise.getNumSamples(); ++i)
        {
            const auto d = out.getSample (0, i) - noise.getSample (0, i - latency);
            err += d * d;
            ref += noise.getSample (0, i - latency) * noise.getSample (0, i - latency);
        }
        const auto nullDb = 10.0 * std::log10 (err / ref + 1.0e-30);
        check (nullDb < -80.0, "mix 0 nulls against delayed dry (dB)", nullDb);
        check (latency > 0 && latency < 128, "reported latency (samples)", latency);
    }

    // 4. Aliasing: loud tones through heavy drive. Everything that is not a true
    //    harmonic counts as alias. Fuzz at full drive is the worst case.
    {
        constexpr int order = 15;
        struct Case { int style; double bin; float driveDb; float limitDb; const char* name; };
        const Case cases[] {
            { 4, 2111.0, 30.0f, -50.0f, "worst alias, 3 kHz fuzz +30 dB drive (dB)" },
            { 0, 2111.0, 30.0f, -70.0f, "worst alias, 3 kHz warm +30 dB drive (dB)" },
            { 4, 4779.0, 30.0f, -35.0f, "worst alias, 7 kHz fuzz +30 dB drive (dB)" },
        };

        for (const auto& tc : cases)
        {
            const auto fundamental = sampleRate * tc.bin / (1 << order);
            Settings s;
            s.style = tc.style;
            s.driveDb = tc.driveDb;
            s.autoGain = false;
            const auto mags = spectrum (run (s, sine (fundamental, -3.0f, 2.0)), order);

            float worst = -200.0f;
            for (size_t i = 1; i < mags.size(); ++i)
            {
                const auto hz = (double) i * sampleRate / (1 << order);
                const auto harmonic = hz / fundamental;
                if (hz < 50.0 || std::abs (harmonic - std::round (harmonic)) * fundamental < 40.0)
                    continue;
                worst = juce::jmax (worst, mags[i]);
            }
            check (worst < tc.limitDb, tc.name, worst);
        }
    }

    // 5. Styles have different harmonic fingerprints.
    {
        constexpr int order = 15;
        const auto f = 48000.0 * 683.0 / 32768.0; // ~1 kHz, bin-centred
        std::printf ("\n      style   H2 (dB)  H3 (dB)  H4 (dB)  H5 (dB)\n");
        for (int style = 0; style < cruzdrive::numStyles; ++style)
        {
            Settings s;
            s.style = style;
            s.driveDb = 18.0f;
            const auto mags = spectrum (run (s, sine (f, -12.0f, 2.0)), order);
            std::printf ("      %-6s  %7.1f  %7.1f  %7.1f  %7.1f\n", cruzdrive::styleNames[style].toRawUTF8(),
                         levelAt (mags, 2 * f, order), levelAt (mags, 3 * f, order),
                         levelAt (mags, 4 * f, order), levelAt (mags, 5 * f, order));
        }
        std::printf ("\n");
    }

    // 6. Crush reports gain reduction on a loud signal.
    {
        Settings s;
        s.crush = 1.0f;
        float reduction = 0.0f;
        run (s, sine (100.0, -6.0f, 1.0), nullptr, &reduction);
        check (reduction > 15.0f, "crush 100% gain reduction on -6 dBFS (dB)", reduction);
    }

    // 7. Random settings and block sizes never produce NaN or runaway levels.
    {
        juce::Random rng (7);
        // Prepared for 128 samples but fed up to 512, like a misbehaving host.
        cruzdrive::Engine engine;
        engine.prepare (sampleRate, 128, 2);
        juce::AudioBuffer<float> buf (2, blockSize);
        float peak = 0.0f;
        bool finite = true;
        for (int b = 0; b < 2000; ++b)
        {
            Settings s;
            s.style = rng.nextInt (cruzdrive::numStyles);
            s.driveDb = rng.nextFloat() * 36.0f;
            s.punish = rng.nextBool();
            s.lowCutHz = 10.0f + rng.nextFloat() * 990.0f;
            s.thump = rng.nextBool();
            s.tone = rng.nextFloat() * 2.0f - 1.0f;
            s.highCutHz = 1000.0f + rng.nextFloat() * 21000.0f;
            s.steep = rng.nextBool();
            s.crush = rng.nextFloat();
            s.darkness = rng.nextFloat();
            s.autoGain = rng.nextBool();
            s.mix = rng.nextFloat();
            s.outputDb = rng.nextFloat() * 36.0f - 24.0f;
            engine.setSettings (s);

            const auto n = 1 + rng.nextInt (blockSize);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    buf.setSample (c, i, rng.nextFloat() * 2.0f - 1.0f);
            juce::dsp::AudioBlock<float> block (buf);
            engine.process (block.getSubBlock (0, (size_t) n));
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                {
                    finite = finite && std::isfinite (buf.getSample (c, i));
                    peak = juce::jmax (peak, std::abs (buf.getSample (c, i)));
                }
        }
        check (finite, "random automation: all samples finite", finite ? 1.0 : 0.0);
        check (peak < 16.0f, "random automation: peak stays bounded (gain)", peak);
    }

    // 8. Speed: 60 seconds of stereo audio with everything switched on.
    {
        Settings s;
        s.driveDb = 20.0f;
        s.crush = 0.5f;
        s.darkness = 0.3f;
        s.lowCutHz = 80.0f;
        s.highCutHz = 12000.0f;
        s.steep = true;
        s.mix = 0.7f;
        const auto in = sine (110.0, -10.0f, 60.0);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        run (s, in);
        const auto seconds = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        check (seconds < 6.0, "x realtime for 60 s stereo at 48 kHz (higher is faster)", 60.0 / seconds);
    }

    std::printf ("\n%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "SOME CHECKS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
