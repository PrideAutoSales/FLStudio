// Accuracy, stability and speed checks for the EQ engine. Runs without a host:
// build the CruzEQTests target and run it, or use `ctest`.

#include "dsp/EqEngine.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

using namespace cruz::dsp;

namespace
{
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    failures += ok ? 0 : 1;
}

const char* shapeName (Shape s)
{
    static const char* names[] { "bell", "low shelf", "low cut", "high shelf", "high cut", "notch", "band pass", "tilt shelf" };
    return names[(int) s];
}

bool isStable (const BiquadCoeffs& c)
{
    return std::isfinite (c.b0) && std::isfinite (c.b1) && std::isfinite (c.b2)
        && std::abs (c.a2) < 1.0 && std::abs (c.a1) < 1.0 + c.a2;
}

// Largest gap in dB between the digital filter and its analog prototype.
// Anything below -24 dB counts as -24 dB: a fraction of a dB that far down a
// cut's stopband is inaudible and would otherwise dominate the result.
struct Accuracy
{
    double worstDb = 0.0;
    BandParams at;
    double atHz = 0.0;
};

Accuracy measure (double sampleRate, double maxBandHz, std::initializer_list<double> qs,
                  std::initializer_list<double> gains, double maxTestHz, Shape shape)
{
    Accuracy result;

    for (double f = 20.0; f <= maxBandHz; f *= 1.07)
        for (double q : qs)
            for (double g : gains)
                for (int slope : slopesDbPerOct)
                {
                    const BandParams p { shape, f, g, q, slope };
                    const auto digital = designBand (p, sampleRate);
                    const auto analog = analogPrototype (p);

                    for (double hz = 20.0; hz <= maxTestHz; hz *= 1.03)
                    {
                        const double d = std::max (digital.magnitudeDb (2.0 * pi * hz / sampleRate), -24.0);
                        const double a = std::max (10.0 * std::log10 (std::max (analog.powerAt (hz / f), 1.0e-24)), -24.0);

                        if (std::abs (d - a) > result.worstDb)
                            result = { std::abs (d - a), p, hz };
                    }

                    if (! shapeUsesSlope (shape))
                        break;
                }

    return result;
}

void testStability()
{
    int unstable = 0, total = 0;

    for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
        for (int s = 0; s < numShapes; ++s)
            for (double f = minFreqHz; f <= maxFreqHz * 1.0001; f *= 1.05)
                for (double q = minQ; q <= maxQ * 1.0001; q *= 1.6)
                    for (double g : { -30.0, -18.0, -6.0, -0.5, 0.0, 0.5, 6.0, 18.0, 30.0 })
                        for (int slope : slopesDbPerOct)
                        {
                            const auto d = designBand ({ (Shape) s, f, g, q, slope }, fs);
                            for (int i = 0; i < d.numSections; ++i)
                            {
                                ++total;
                                unstable += isStable (d.sections[(size_t) i]) ? 0 : 1;
                            }
                        }

    std::printf ("      %d of %d filter sections checked were unstable\n", unstable, total);
    check (unstable == 0, "every shape, frequency, Q, gain and slope gives a stable filter");
}

void testAccuracy()
{
    for (double fs : { 44100.0, 48000.0, 96000.0 })
    {
        double worstTypical = 0.0, worstExtreme = 0.0;

        for (int s = 0; s < numShapes; ++s)
        {
            // Everyday settings: bands up to 5 kHz, moderate Q and gain.
            const auto typical = measure (fs, 5000.0, { 0.5, 0.7071, 1.0, 2.0 }, { -12.0, -6.0, 6.0, 12.0 }, 0.3 * fs, (Shape) s);
            // Extreme settings: bands up to a quarter of the sample rate, Q 0.1 to 8, gain to +/-30 dB.
            const auto extreme = measure (fs, 0.25 * fs, { 0.1, 0.3, 0.7071, 2.0, 8.0 }, { -30.0, -12.0, 12.0, 30.0 }, 0.3 * fs, (Shape) s);

            std::printf ("      %6.0f Hz  %-10s  typical %.3f dB   extreme %.3f dB (%.0f Hz, Q %.2f, %+.0f dB, at %.0f Hz)\n",
                         fs, shapeName ((Shape) s), typical.worstDb, extreme.worstDb,
                         extreme.at.freqHz, extreme.at.q, extreme.at.gainDb, extreme.atHz);

            worstTypical = std::max (worstTypical, typical.worstDb);
            worstExtreme = std::max (worstExtreme, extreme.worstDb);
        }

        char label[160];
        std::snprintf (label, sizeof (label), "at %.0f Hz, everyday settings track the analog curve within 1 dB (worst %.3f dB)", fs, worstTypical);
        check (worstTypical <= 1.0, label);
        std::snprintf (label, sizeof (label), "at %.0f Hz, extreme settings track the analog curve within 2 dB (worst %.3f dB)", fs, worstExtreme);
        check (worstExtreme <= 2.0, label);
    }
}

void testUnityAtZeroGain()
{
    double worst = 0.0;

    for (auto s : { Shape::bell, Shape::lowShelf, Shape::highShelf, Shape::tiltShelf })
        for (double f = 20.0; f < 20000.0; f *= 1.5)
            for (double hz = 20.0; hz < 20000.0; hz *= 1.2)
                worst = std::max (worst, std::abs (designBand ({ s, f, 0.0, 1.0, 12 }, 48000.0).magnitudeDb (2.0 * pi * hz / 48000.0)));

    check (worst < 1.0e-6, "bands at 0 dB gain leave the signal untouched");
}

void testZeroLatencyAndMatch()
{
    EqEngine eq;
    eq.prepare (48000.0);
    const BandParams bell { Shape::bell, 1000.0, 9.0, 1.0, 12 };
    eq.setBand (0, { true, bell });

    // Impulse response: the first output sample must already carry the
    // filter's response, so there is no added delay.
    std::vector<float> left (4096, 0.0f), right (4096, 0.0f);
    left[0] = right[0] = 1.0f;
    float* channels[] { left.data(), right.data() };
    eq.process (channels, 2, (int) left.size());
    check (std::abs (left[0] - eq.currentDesign (0).sections[0].b0) < 1.0e-6, "zero latency: output starts on the first sample");

    // Steady-state gain of a sine at the band frequency equals the design.
    EqEngine eq2;
    eq2.prepare (48000.0);
    eq2.setBand (0, { true, bell });
    const int n = 48000;
    std::vector<float> sine ((size_t) n);
    for (int i = 0; i < n; ++i)
        sine[(size_t) i] = (float) (0.1 * std::sin (2.0 * pi * 1000.0 * i / 48000.0));
    float* mono[] { sine.data() };
    eq2.process (mono, 1, n);

    double peak = 0.0;
    for (int i = n / 2; i < n; ++i)
        peak = std::max (peak, (double) std::abs (sine[(size_t) i]));

    const double measuredDb = 20.0 * std::log10 (peak / 0.1);
    std::printf ("      +9 dB bell at 1 kHz measures %+.3f dB on a 1 kHz sine\n", measuredDb);
    check (std::abs (measuredDb - 9.0) < 0.05, "processed audio matches the designed curve");
}

void testParameterSweepsStayBounded()
{
    EqEngine eq;
    eq.prepare (44100.0);

    std::mt19937 rng (1234);
    std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
    std::vector<float> left (512), right (512);
    float* channels[] { left.data(), right.data() };
    double worst = 0.0;
    bool finite = true;

    for (int block = 0; block < 4000; ++block)
    {
        for (int b = 0; b < EqEngine::numBands; ++b)
        {
            const double t = block * 0.01 + b;
            const BandParams p { (Shape) (b % numShapes),
                                 minFreqHz * std::pow (maxFreqHz / minFreqHz, 0.5 + 0.5 * std::sin (t * 1.3)),
                                 18.0 * std::sin (t * 0.7),
                                 std::exp (2.0 * std::sin (t * 0.9)),
                                 slopesDbPerOct[(size_t) (b % (int) slopesDbPerOct.size())] };
            eq.setBand (b, { (block / 500 + b) % 3 != 0, p });
        }

        for (size_t i = 0; i < left.size(); ++i)
            left[i] = right[i] = 0.1f * noise (rng);

        eq.process (channels, 2, (int) left.size());

        for (float x : left)
        {
            finite &= std::isfinite (x);
            worst = std::max (worst, (double) std::abs (x));
        }
    }

    std::printf ("      loudest sample while sweeping all 24 bands: %.2f\n", worst);
    check (finite && worst < 1000.0, "sweeping every band continuously never blows up");
}

void benchmark()
{
    const double fs = 48000.0;
    const int seconds = 10, blockSize = 256;

    for (int scenario = 0; scenario < 2; ++scenario)
    {
        EqEngine eq;
        eq.prepare (fs);

        for (int b = 0; b < EqEngine::numBands; ++b)
        {
            const BandParams p = scenario == 0 ? BandParams { Shape::bell, 30.0 * std::pow (1.3, b), 6.0, 1.0, 12 }
                                               : BandParams { b % 2 ? Shape::highCut : Shape::lowCut, 30.0 * std::pow (1.3, b), 0.0, 0.7071, 96 };
            eq.setBand (b, { true, p });
        }

        std::vector<float> left ((size_t) blockSize), right ((size_t) blockSize);
        float* channels[] { left.data(), right.data() };
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> noise (-0.5f, 0.5f);

        const int blocks = (int) (seconds * fs / blockSize);
        double elapsed = 0.0;

        for (int i = 0; i < blocks; ++i)
        {
            for (int j = 0; j < blockSize; ++j)
                left[(size_t) j] = right[(size_t) j] = noise (rng);

            const auto start = std::chrono::steady_clock::now();
            eq.process (channels, 2, blockSize);
            elapsed += std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
        }

        std::printf ("BENCH  %s: %.2f%% of one CPU core (stereo, 48 kHz)\n",
                     scenario == 0 ? "24 bell bands" : "24 cut bands at 96 dB/oct (worst case)",
                     100.0 * elapsed / seconds);
    }
}
} // namespace

int main()
{
    testStability();
    testAccuracy();
    testUnityAtZeroGain();
    testZeroLatencyAndMatch();
    testParameterSweepsStayBounded();
    benchmark();

    std::printf ("\n%s (%d failed)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}
