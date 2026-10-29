// Quick checks on the sidechain engine that run without a DAW:
//   c++ -std=c++20 -O2 -I../Source EngineTest.cpp -o engine-test && ./engine-test
#include "SidechainEngine.h"

#include <chrono>
#include <cstdio>
#include <vector>

using namespace cruzsc;

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what);
    if (! ok)
        ++failures;
}

static constexpr float sr = 48000.0f;
static constexpr float pi = 3.14159265f;

struct Run
{
    std::vector<float> out, gr;
    std::vector<Frame> frames;
};

// Main: a steady sine at mainHz. Key: a ghost kick every beatSec (or none),
// or with burst, a 30 ms 60 Hz tone that stops dead.
static Run render (Params p, float seconds, float mainHz, float beatSec, float mainLevel = 0.5f, bool burst = false)
{
    SidechainEngine e;
    e.setParams (p);
    e.prepare (sr, 512);

    const int n = (int) (sr * seconds);
    Run run;
    run.out.resize ((size_t) n);
    run.gr.resize ((size_t) n);

    GhostKick kick;
    kick.prepare (sr);
    const int beat = beatSec > 0 ? (int) (beatSec * sr) : 0;

    std::vector<float> l (512), r (512), k (512);

    for (int start = 0; start < n; start += 512)
    {
        const int len = std::min (512, n - start);

        for (int i = 0; i < len; ++i)
        {
            const int s = start + i;
            l[(size_t) i] = r[(size_t) i] = mainLevel * std::sin (2 * pi * mainHz * (float) s / sr);
            if (beat > 0 && s % beat == 0)
                kick.trigger();
            k[(size_t) i] = kick.next();
            if (burst)
                k[(size_t) i] = beat > 0 && s % beat < (int) (0.03f * sr) ? 0.9f * std::sin (2 * pi * 60.0f * (float) s / sr) : 0.0f;
        }

        float* io[] = { l.data(), r.data() };
        const float* key[] = { k.data(), k.data() };

        // Feed one sample at a time so the gain reduction can be read per sample.
        for (int i = 0; i < len; ++i)
        {
            float* ioi[] = { io[0] + i, io[1] + i };
            const float* keyi[] = { key[0] + i, key[1] + i };
            e.process (ioi, 2, beat > 0 ? keyi : nullptr, 2, 1, [&] (const Frame& f) { run.frames.push_back (f); });
            run.out[(size_t) (start + i)] = l[(size_t) i];
            run.gr[(size_t) (start + i)] = e.getGainReductionDb();
        }
    }

    return run;
}

static float peakBetween (const std::vector<float>& x, float fromSec, float toSec)
{
    float m = 0.0f;
    for (int i = (int) (fromSec * sr); i < (int) (toSec * sr) && i < (int) x.size(); ++i)
        m = std::max (m, std::abs (x[(size_t) i]));
    return m;
}

static float maxBetween (const std::vector<float>& x, float fromSec, float toSec)
{
    float m = -1.0e9f;
    for (int i = (int) (fromSec * sr); i < (int) (toSec * sr) && i < (int) x.size(); ++i)
        m = std::max (m, x[(size_t) i]);
    return m;
}

int main()
{
    // Gain computer.
    {
        Params p;
        p.thresholdDb = -20.0f;
        p.ratio = infiniteRatio;
        p.kneeDb = 0.0f;
        p.rangeDb = 60.0f;
        check (gainReductionDb (-30.0f, p) == 0.0f, "no reduction below threshold");
        check (std::abs (gainReductionDb (-10.0f, p) - 10.0f) < 1.0e-4f, "infinite ratio holds output at threshold");
        p.ratio = 4.0f;
        check (std::abs (gainReductionDb (-8.0f, p) - 9.0f) < 1.0e-4f, "4:1 takes off three quarters of the overshoot");
        p.rangeDb = 6.0f;
        check (gainReductionDb (0.0f, p) == 6.0f, "range caps the reduction");
        p.rangeDb = 60.0f;
        p.kneeDb = 12.0f;
        const float atT = gainReductionDb (-20.0f, p);
        check (atT > 0.0f && atT < 3.0f, "soft knee starts reducing gently at the threshold");
        check (gainReductionDb (-26.5f, p) == 0.0f, "soft knee does nothing below its lower edge");
    }

    // Bends.
    {
        Bend b;
        b.set (0.0f);
        check (std::abs (b (0.5f) - 0.5f) < 1.0e-5f, "zero bend is a straight line");
        b.set (0.8f);
        check (b (0.5f) > 0.8f && std::abs (b (1.0f) - 1.0f) < 1.0e-4f, "positive bend moves fast first and still ends at 1");
        b.set (-0.8f);
        check (b (0.5f) < 0.2f, "negative bend starts slow");
    }

    // No key: the signal passes untouched.
    {
        Params p;
        auto run = render (p, 0.5f, 100.0f, 0.0f);
        check (std::abs (peakBetween (run.out, 0.1f, 0.5f) - 0.5f) < 0.01f, "without a key nothing is ducked");
    }

    // A kick every half second ducks the bass, then lets it back.
    {
        Params p;
        p.thresholdDb = -30.0f;
        p.ratio = infiniteRatio;
        p.rangeDb = 24.0f;
        p.attackMs = 1.0f;
        p.holdMs = 20.0f;
        p.releaseMs = 150.0f;
        auto run = render (p, 2.0f, 60.0f, 0.5f, 0.5f, true);

        check (maxBetween (run.gr, 0.5f, 0.52f) > 20.0f, "a kick ducks to the full range within the attack");
        check (maxBetween (run.gr, 0.9f, 0.99f) < 0.5f, "the bass is back before the next kick");
        check (peakBetween (run.out, 0.505f, 0.52f) < 0.1f, "the bass is quiet under the kick");
        check (peakBetween (run.out, 0.92f, 0.99f) > 0.45f, "the bass is full between kicks");
        check (run.frames.size() > 300, "display slices are made every few milliseconds");
    }

    // Release time and bend.
    {
        Params p;
        p.thresholdDb = -30.0f;
        p.ratio = infiniteRatio;
        p.rangeDb = 24.0f;
        p.attackMs = 0.0f;
        p.holdMs = 0.0f;
        p.releaseMs = 300.0f;
        p.releaseBend = 0.0f;
        auto linear = render (p, 1.0f, 60.0f, 0.5f, 0.5f, true);
        p.releaseBend = 0.9f;
        auto fast = render (p, 1.0f, 60.0f, 0.5f, 0.5f, true);
        p.releaseBend = -0.9f;
        auto slow = render (p, 1.0f, 60.0f, 0.5f, 0.5f, true);

        // Find when the key stops reducing in the linear run, then look half a release later.
        int peakAt = (int) (0.5f * sr);
        for (int i = peakAt; i < (int) (0.9f * sr); ++i)
            if (linear.gr[(size_t) i] >= linear.gr[(size_t) peakAt] - 1.0e-3f) peakAt = i;
        const auto mid = (size_t) (peakAt + (int) (0.15f * sr));
        std::printf ("      half-way through the release: linear %.1f dB, fast %.1f dB, slow %.1f dB\n",
                     linear.gr[mid], fast.gr[mid], slow.gr[mid]);
        check (fast.gr[mid] < linear.gr[mid] && linear.gr[mid] < slow.gr[mid], "release bend changes the recovery shape");
        check (linear.gr[(size_t) (peakAt + (int) (0.31f * sr))] < 0.5f, "release finishes in its set time");
    }

    // Split mode leaves the highs alone.
    {
        Params p;
        p.thresholdDb = -30.0f;
        p.ratio = infiniteRatio;
        p.rangeDb = 30.0f;
        p.splitLows = true;
        p.splitHz = 150.0f;
        auto highs = render (p, 1.0f, 3000.0f, 0.5f);
        auto lows = render (p, 1.0f, 50.0f, 0.5f);
        check (peakBetween (highs.out, 0.51f, 0.53f) > 0.45f, "lows-only ducking keeps a 3 kHz tone");
        check (peakBetween (lows.out, 0.51f, 0.53f) < 0.1f, "lows-only ducking still ducks a 50 Hz bass");
    }

    // Lookahead delays the main signal by its latency.
    {
        Params p;
        p.lookaheadMs = 5.0f;
        SidechainEngine e;
        e.setParams (p);
        e.prepare (sr, 512);
        std::vector<float> l (512, 0.0f), r (512, 0.0f);
        l[0] = r[0] = 1.0f;
        float* io[] = { l.data(), r.data() };
        e.process (io, 2, nullptr, 0, 512, [] (const Frame&) {});
        const int latency = e.getLatencySamples();
        check (latency == 240 && std::abs (l[(size_t) latency] - 1.0f) < 1.0e-6f, "lookahead delay matches the reported latency");
    }

    // Heat-map bands find the right frequency.
    {
        Params p;
        auto run = render (p, 1.0f, 57.0f, 0.0f, 1.0f);
        const auto& f = run.frames.back();
        int best = 0;
        for (int b = 1; b < numBands; ++b)
            if (f.inBands[(size_t) b] > f.inBands[(size_t) best]) best = b;
        std::printf ("      57 Hz tone: loudest band %.0f Hz at %.1f dB\n", bandHz (best), f.inBands[(size_t) best]);
        check (std::abs (bandHz (best) - 57.0f) < 6.0f, "a 57 Hz tone lights the band around 57 Hz");
        check (std::abs (f.inBands[(size_t) best]) < 3.0f, "a full-scale tone reads close to 0 dB");
        check (f.inBands[(size_t) (numBands - 1)] < f.inBands[(size_t) best] - 15.0f, "far bands stay dark");
    }

    // Speed: stereo processing with the analyser, in real-time multiples.
    {
        Params p;
        p.splitLows = true;
        SidechainEngine e;
        e.setParams (p);
        e.prepare (sr, 512);
        std::vector<float> l (512), r (512), k (512);
        for (int i = 0; i < 512; ++i)
            l[(size_t) i] = r[(size_t) i] = k[(size_t) i] = 0.3f * std::sin ((float) i * 0.05f);
        float* io[] = { l.data(), r.data() };
        const float* key[] = { k.data(), k.data() };
        int frames = 0;
        const int blocks = (int) (sr * 10 / 512);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b)
            e.process (io, 2, key, 2, 512, [&] (const Frame&) { ++frames; });
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("      10 s of stereo audio in %.1f ms (%.0fx real time)\n", secs * 1000.0, 10.0 / secs);
        check (std::isfinite (l[100]) && 10.0 / secs > 50.0, "fast enough with the heat map running");
    }

    std::printf ("\n%s\n", failures == 0 ? "All engine checks passed." : "Some engine checks FAILED.");
    return failures == 0 ? 0 : 1;
}
