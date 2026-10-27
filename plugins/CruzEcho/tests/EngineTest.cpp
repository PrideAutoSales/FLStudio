// Quick checks on the echo engine that run without a DAW:
//   c++ -std=c++20 -O2 -I../Source EngineTest.cpp -o engine-test && ./engine-test
#include "EchoEngine.h"

#include <cstdio>
#include <vector>

using namespace cruzecho;

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what);
    if (! ok)
        ++failures;
}

struct Run
{
    std::vector<float> l, r;
};

static Run render (Params p, int seconds, float sr = 48000.0f, bool impulse = true, int block = 512)
{
    EchoEngine e;
    e.setParams (p);
    e.prepare (sr, block);
    e.setParams (p);

    const int n = (int) sr * seconds;
    Run out { std::vector<float> ((size_t) n), std::vector<float> ((size_t) n) };
    uint32_t rng = 3;

    for (int i = 0; i < n; ++i)
    {
        if (impulse)
            out.l[(size_t) i] = out.r[(size_t) i] = (i == 0 ? 0.5f : 0.0f);
        else
        {
            rng = rng * 1664525u + 1013904223u;
            out.l[(size_t) i] = out.r[(size_t) i] = 0.5f * ((float) (rng >> 8) / 8388608.0f - 1.0f);
        }
    }

    for (int start = 0; start < n; start += block)
    {
        float* ch[] = { out.l.data() + start, out.r.data() + start };
        e.process (ch, 2, std::min (block, n - start));
    }
    return out;
}

static int peakIndex (const std::vector<float>& x, int from, int to)
{
    int best = from;
    for (int i = from; i < to; ++i)
        if (std::abs (x[(size_t) i]) > std::abs (x[(size_t) best]))
            best = i;
    return best;
}

static bool allFinite (const Run& r, float limit)
{
    for (size_t i = 0; i < r.l.size(); ++i)
        if (! std::isfinite (r.l[i]) || ! std::isfinite (r.r[i]) || std::abs (r.l[i]) > limit || std::abs (r.r[i]) > limit)
            return false;
    return true;
}

int main()
{
    const float sr = 48000.0f;

    {
        Params p;
        p.style = Style::digital; p.wobble = 0; p.mix = 1; p.feedback = 0.5f; p.delayMs = 250; p.saturation = 0;
        auto r = render (p, 2);
        const int t = (int) (0.25f * sr);
        const int first = peakIndex (r.l, 100, t + t / 2);
        const int second = peakIndex (r.l, t + t / 2, 2 * t + t / 2);
        std::printf ("      first echo at %d, second at %d (expected %d, %d)\n", first, second, t, 2 * t);
        check (std::abs (first - t) <= 3, "digital: first echo lands on time");
        check (std::abs (second - 2 * t) <= 3, "digital: second echo lands on time");
        const float ratio = std::abs (r.l[(size_t) second]) / std::abs (r.l[(size_t) first]);
        std::printf ("      repeat ratio %.3f\n", ratio);
        check (ratio > 0.4f && ratio < 0.6f, "digital: feedback 50% halves each repeat");
    }

    {
        Params p;
        p.style = Style::digital; p.wobble = 0; p.mix = 1; p.feedback = 0.5f; p.delayMs = 300; p.groove = 1;
        auto r = render (p, 2);
        const int t = (int) (0.3f * sr);
        const int first = peakIndex (r.l, 100, (int) (1.6f * t));
        const int second = peakIndex (r.l, (int) (1.6f * t), (int) (2.6f * t));
        std::printf ("      groove: first %d (expect %d), second %d (expect %d)\n", first, (int) (t * 4 / 3), second, 2 * t);
        check (std::abs (first - t * 4 / 3) <= 3 && std::abs (second - 2 * t) <= 3, "groove swings alternate repeats");
    }

    {
        Params p;
        p.style = Style::digital; p.wobble = 0; p.mix = 1; p.feedback = 0.5f; p.delayMs = 200; p.mode = Mode::pingPong;
        auto r = render (p, 1);
        const int t = (int) (0.2f * sr);
        check (std::abs (r.l[(size_t) t]) > 0.1f && std::abs (r.r[(size_t) t]) < 0.01f, "ping-pong: first echo on the left");
        check (std::abs (r.r[(size_t) (2 * t)]) > 0.05f && std::abs (r.l[(size_t) (2 * t)]) < 0.01f, "ping-pong: second echo on the right");
    }

    for (int s = 0; s < 5; ++s)
    {
        for (int m = 0; m < 3; ++m)
        {
            Params p;
            p.style = (Style) s; p.mode = (Mode) m; p.feedback = 1.1f; p.saturation = 1; p.wobble = 1;
            p.delayMs = 90; p.mix = 1; p.groove = 0.6f; p.duck = 0.5f;
            auto r = render (p, 6, sr, false);
            char name[96];
            std::snprintf (name, sizeof name, "style %d mode %d: stays finite and bounded at 110%% feedback", s, m);
            check (allFinite (r, 8.0f), name);
        }
    }

    for (int s = 0; s < 5; ++s)
    {
        Params p;
        p.style = (Style) s; p.feedback = 0.6f; p.mix = 1;
        auto r = render (p, 12, sr, false);
        float peak = 0;
        for (auto v : r.l) peak = std::max (peak, std::abs (v));
        char name[96];
        std::snprintf (name, sizeof name, "style %d: noise at 60%% feedback peaks %.2f (under 3)", s, peak);
        check (peak < 3.0f, name);
    }

    {
        Params p;
        p.style = Style::tape; p.feedback = 0.7f; p.mix = 1;
        auto r = render (p, 20, sr, true);
        float tail = 0;
        for (size_t i = r.l.size() - 48000; i < r.l.size(); ++i) tail = std::max (tail, std::abs (r.l[i]));
        std::printf ("      tail level after 20 s: %g\n", tail);
        check (tail < 1e-3f, "tape: echoes die away at 70% feedback");
    }

    for (float rate : { 44100.0f, 96000.0f, 192000.0f })
    {
        Params p;
        p.delayMs = 4000; p.groove = 1; p.wobble = 1; p.style = Style::space;
        auto r = render (p, 2, rate);
        check (allFinite (r, 8.0f), "longest time with full groove at other sample rates");
    }

    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
