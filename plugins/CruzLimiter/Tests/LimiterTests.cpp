// Offline checks for the Cruz Limiter engine. Exits with a non-zero code on failure.

#include <juce_dsp/juce_dsp.h>

#include "../Source/LimiterEngine.h"
#include "../Source/LoudnessMeter.h"

#include <chrono>
#include <cstdio>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

/**
    A hard-to-limit test signal: kick, bright noise, high tones and band-limited clicks.
    Content stops short of the Nyquist frequency, as it does in real recordings.
*/
juce::AudioBuffer<float> makeProgram (double sampleRate, double seconds, float levelDb)
{
    const auto n = (int) (sampleRate * seconds);
    juce::AudioBuffer<float> buffer (2, n);
    juce::Random random (1234);
    const auto level = juce::Decibels::decibelsToGain (levelDb);
    const auto pi = juce::MathConstants<double>::pi;
    const auto smoothing = (float) std::exp (-2.0 * pi * 12000.0 / sampleRate);
    float lp1 = 0.0f, lp2 = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const auto t = i / sampleRate;
        const auto envelope = 0.5f + 0.5f * (float) std::sin (2.0 * pi * 1.3 * t);
        const auto kick = (i % 11025) < 2000 ? (float) std::sin (2.0 * pi * 55.0 * t) : 0.0f;
        lp1 = (1.0f - smoothing) * (random.nextFloat() * 2.0f - 1.0f) + smoothing * lp1;
        lp2 = (1.0f - smoothing) * lp1 + smoothing * lp2;
        const auto hiss = 3.0f * lp2;
        const auto tone = (float) std::sin (2.0 * pi * (i % 20000 < 10000 ? 11025.0 : 15013.0) * t);

        // band-limited click centred every 7919 samples
        const auto k = (i % 7919) - 24;
        const auto arg = pi * 0.8 * k;
        const auto click = std::abs (k) < 24 ? (float) ((k == 0 ? 1.0 : std::sin (arg) / arg) * (0.5 + 0.5 * std::cos (pi * k / 24.0)))
                                             : 0.0f;

        for (int c = 0; c < 2; ++c)
        {
            const auto s = 0.6f * kick + 0.3f * hiss * envelope + 0.4f * tone * (c == 0 ? 1.0f : 0.5f)
                         + (c == 0 ? 1.2f : -1.2f) * click;
            buffer.setSample (c, i, s * level);
        }
    }
    return buffer;
}

void runEngine (cruz::LimiterEngine& engine, juce::AudioBuffer<float>& buffer, int blockSize)
{
    for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
    {
        const auto len = std::min (blockSize, buffer.getNumSamples() - start);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
        engine.process (view);
    }
}

float samplePeakDb (const juce::AudioBuffer<float>& b, int skip)
{
    float peak = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = skip; i < b.getNumSamples(); ++i)
            peak = std::max (peak, std::abs (b.getSample (c, i)));
    return juce::Decibels::gainToDecibels (peak, -200.0f);
}

/** Independent true-peak reference: 16x interpolation with a long Kaiser-windowed sinc. */
float truePeakDb (const juce::AudioBuffer<float>& b, int skip)
{
    constexpr int factor = 16, half = 64;
    auto bessel0 = [] (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    };

    std::vector<float> table ((size_t) (factor * 2 * half + 1));
    for (int k = -factor * half; k <= factor * half; ++k)
    {
        const double t = (double) k / factor;
        const double sinc = k == 0 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t);
        const double r = t / half;
        const double w = bessel0 (8.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / bessel0 (8.0);
        table[(size_t) (k + factor * half)] = (float) (sinc * w);
    }

    float peak = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
    {
        const auto* x = b.getReadPointer (c);
        for (int i = skip + half; i < b.getNumSamples() - half; ++i)
            for (int p = 0; p < factor; ++p)
            {
                float acc = 0.0f;
                for (int m = -half + 1; m <= half; ++m)
                    acc += x[i + m] * table[(size_t) (p - m * factor + factor * half)];
                peak = std::max (peak, std::abs (acc));
            }
    }
    return juce::Decibels::gainToDecibels (peak, -200.0f);
}

void testCeiling()
{
    const double rate = 48000.0;
    const auto source = makeProgram (rate, 3.0, 0.0f);

    for (int os = 0; os <= 4; ++os)
        for (float lookahead : { 0.0f, 1.0f, 5.0f })
            for (float attack : { 0.0f, 60.0f, 100.0f })
                for (bool truePeak : { false, true })
                {
                    if (os >= 3 && juce::approximatelyEqual (attack, 60.0f))
                        continue; // keep the run short; the other attack settings cover these

                    cruz::LimiterEngine engine;
                    cruz::LimiterSettings s;
                    s.inputGainDb = 18.0f;
                    s.ceilingDb = -1.0f;
                    s.lookaheadMs = lookahead;
                    s.attackPercent = attack;
                    s.releaseMs = 50.0f;
                    s.oversamplingIndex = os;
                    s.truePeak = truePeak;
                    s.style = (cruz::LimiterStyle) ((os * 3 + (int) lookahead) % 8);
                    engine.prepare (rate, 512, 2);
                    engine.setSettings (s);

                    auto buffer = source;
                    runEngine (engine, buffer, 512);

                    const auto peak = samplePeakDb (buffer, 0);
                    const auto label = juce::String::formatted ("os=%dx look=%.0fms attack=%.0f%% tp=%d style=%s",
                                                                1 << os, lookahead, attack, (int) truePeak,
                                                                cruz::getStyleNames()[(int) s.style].toRawUTF8());
                    check (peak <= -1.0f + 1.0e-4f, "sample peak " + juce::String (peak, 3) + " dB  " + label);

                    if (truePeak && lookahead > 0.0f)
                    {
                        const auto tp = truePeakDb (buffer, 2048);
                        check (tp <= -1.0f + 0.1f, "true peak   " + juce::String (tp, 3) + " dBTP " + label);
                    }
                }
}

void testStylesWithoutOversampling()
{
    const double rate = 44100.0;
    const auto source = makeProgram (rate, 3.0, 0.0f);

    for (int style = 0; style < cruz::getStyleNames().size(); ++style)
    {
        cruz::LimiterEngine engine;
        cruz::LimiterSettings s;
        s.inputGainDb = 15.0f;
        s.ceilingDb = -0.3f;
        s.oversamplingIndex = 0;
        s.truePeak = true;
        s.style = (cruz::LimiterStyle) style;
        engine.prepare (rate, 300, 2);
        engine.setSettings (s);

        auto buffer = source;
        runEngine (engine, buffer, 300);
        const auto label = " os=1x tp=1 style=" + cruz::getStyleNames()[style];
        check (samplePeakDb (buffer, 0) <= -0.3f + 1.0e-4f, "sample peak " + juce::String (samplePeakDb (buffer, 0), 3) + " dB " + label);
        const auto tp = truePeakDb (buffer, 2048);
        check (tp <= -0.3f + 0.1f, "true peak   " + juce::String (tp, 3) + " dBTP" + label);
    }
}

void testLatencyAndTransparency()
{
    const double rate = 44100.0;

    for (int os = 0; os <= 4; ++os)
        for (bool truePeak : { false, true })
            for (float lookahead : { 0.0f, 2.0f })
            {
                cruz::LimiterEngine engine;
                cruz::LimiterSettings s;
                s.ceilingDb = 0.0f;
                s.inputGainDb = 0.0f;
                s.lookaheadMs = lookahead;
                s.oversamplingIndex = os;
                s.truePeak = truePeak;
                engine.prepare (rate, 256, 2);
                engine.setSettings (s);

                // A quiet band-limited click; nothing should be limited.
                juce::AudioBuffer<float> buffer (2, 8192);
                buffer.clear();
                const int at = 1000;
                for (int k = -40; k <= 40; ++k)
                {
                    const double t = k * 0.45;
                    const auto v = 0.25f * (float) (k == 0 ? 1.0 : std::sin (juce::MathConstants<double>::pi * t) / (juce::MathConstants<double>::pi * t))
                                 * (float) (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * k / 41.0)) * 0.45f;
                    buffer.setSample (0, at + k, v);
                    buffer.setSample (1, at + k, v);
                }
                const auto input = buffer;
                runEngine (engine, buffer, 256);

                const auto latency = engine.getLatencySamples();
                double err = 0.0, ref = 0.0;
                for (int i = 0; i < 8192 - latency; ++i)
                {
                    const auto d = buffer.getSample (0, i + latency) - input.getSample (0, i);
                    err += d * d;
                    ref += input.getSample (0, i) * input.getSample (0, i);
                }
                const auto nullDb = 10.0 * std::log10 (err / ref + 1.0e-30);
                int measured = 0;
                for (int i = 0; i < 8192; ++i)
                    if (std::abs (buffer.getSample (0, i)) > std::abs (buffer.getSample (0, measured)))
                        measured = i;
                measured -= at;
                const auto label = juce::String::formatted ("os=%dx tp=%d look=%.0fms latency=%d (peak moved %d)",
                                                            1 << os, (int) truePeak, lookahead, latency, measured);
                check (nullDb < -60.0, "null vs delayed input " + juce::String (nullDb, 1) + " dB  " + label);
            }
}

void testAuditionNull()
{
    cruz::LimiterEngine engine;
    cruz::LimiterSettings s;
    s.ceilingDb = 0.0f;
    s.audition = true;
    s.oversamplingIndex = 0;
    s.truePeak = false; // true peak forces 2x, whose filter removes content above 20 kHz
    engine.prepare (48000.0, 512, 2);
    engine.setSettings (s);

    auto buffer = makeProgram (48000.0, 1.0, -20.0f);
    runEngine (engine, buffer, 512);
    check (samplePeakDb (buffer, 0) < -100.0f, "audition is silent when nothing is limited ("
                                                + juce::String (samplePeakDb (buffer, 0), 1) + " dB)");
}

void testLoudness()
{
    const double rate = 48000.0;
    cruz::LoudnessMeter meter;
    meter.prepare (rate, 2);

    // EBU Tech 3341 case: stereo 1 kHz sine at -23 dBFS reads -23 LUFS.
    const int n = (int) (rate * 20.0);
    juce::AudioBuffer<float> buffer (2, n);
    const auto amp = juce::Decibels::decibelsToGain (-23.0f);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < 2; ++c)
            buffer.setSample (c, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / rate));

    meter.process (buffer.getArrayOfReadPointers(), 2, n);
    check (std::abs (meter.getIntegrated() + 23.0f) < 0.1f, "integrated loudness " + juce::String (meter.getIntegrated(), 2) + " LUFS (want -23)");
    check (std::abs (meter.getShortTerm() + 23.0f) < 0.1f, "short-term loudness " + juce::String (meter.getShortTerm(), 2) + " LUFS (want -23)");
    check (meter.getRange() < 0.2f, "loudness range " + juce::String (meter.getRange(), 2) + " LU (want 0)");

    // EBU Tech 3342 case: 20 s at -20 dBFS then 20 s at -30 dBFS gives LRA 10 LU.
    meter.reset();
    for (float level : { -20.0f, -30.0f })
    {
        const auto a = juce::Decibels::decibelsToGain (level);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
                buffer.setSample (c, i, a * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / rate));
        meter.process (buffer.getArrayOfReadPointers(), 2, n);
    }
    check (std::abs (meter.getRange() - 10.0f) < 1.0f, "loudness range " + juce::String (meter.getRange(), 2) + " LU (want 10)");
}

void testSpeed()
{
    const double rate = 48000.0;
    const auto source = makeProgram (rate, 20.0, 0.0f);

    for (int os : { 1, 2, 4 })
    {
        cruz::LimiterEngine engine;
        cruz::LimiterSettings s;
        s.inputGainDb = 12.0f;
        s.oversamplingIndex = os;
        s.lookaheadMs = 2.0f;
        engine.prepare (rate, 512, 2);
        engine.setSettings (s);

        auto buffer = source;
        const auto start = std::chrono::steady_clock::now();
        runEngine (engine, buffer, 512);
        const auto seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        std::printf ("INFO  true peak + %2dx oversampling: %.1fx faster than real time (%.2f%% of one core)\n",
                     1 << os, 20.0 / seconds, 100.0 * seconds / 20.0);
    }
}
} // namespace

int main()
{
    testLoudness();
    testAuditionNull();
    testLatencyAndTransparency();
    testCeiling();
    testStylesWithoutOversampling();
    testSpeed();

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
