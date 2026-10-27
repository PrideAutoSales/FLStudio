#pragma once

// The tape model itself. Nothing in here depends on JUCE, so it can be
// tested on its own. The plugin wraps the hysteresis stage in an oversampler.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace cruztape
{
constexpr double pi = 3.14159265358979323846;

inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }

//==============================================================================
// Transposed direct form II biquad with the usual cookbook designs.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    double process (double x)
    {
        const auto y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void setNormalised (double nb0, double nb1, double nb2, double na0, double na1, double na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0;
        a1 = na1 / na0; a2 = na2 / na0;
    }

    static double clampFreq (double f, double fs) { return std::clamp (f, 1.0, fs * 0.47); }

    void setPeak (double fs, double f, double q, double gainDb)
    {
        const auto A = std::pow (10.0, gainDb / 40.0);
        const auto w = 2 * pi * clampFreq (f, fs) / fs;
        const auto alpha = std::sin (w) / (2 * q);
        const auto c = std::cos (w);
        setNormalised (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void setLowPass (double fs, double f, double q)
    {
        const auto w = 2 * pi * clampFreq (f, fs) / fs;
        const auto alpha = std::sin (w) / (2 * q);
        const auto c = std::cos (w);
        setNormalised ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    void setHighPass (double fs, double f, double q)
    {
        const auto w = 2 * pi * clampFreq (f, fs) / fs;
        const auto alpha = std::sin (w) / (2 * q);
        const auto c = std::cos (w);
        setNormalised ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + alpha, -2 * c, 1 - alpha);
    }

    // First-order shelf g (1 + s/wz) / (1 + s/wp), bilinear with prewarping.
    // Swapping wz and wp and inverting g gives the exact inverse, which is how
    // record and playback emphasis cancel out for small signals.
    void setFirstOrderShelf (double fs, double fz, double fp, double g = 1.0)
    {
        const auto warp = [fs] (double f) { return 2 * fs * std::tan (pi * clampFreq (f, fs) / fs); };
        const auto wz = warp (fz), wp = warp (fp);
        const auto k = 2 * fs;
        setNormalised (g * (1 + k / wz), g * (1 - k / wz), 0, 1 + k / wp, 1 - k / wp, 0);
    }
};

//==============================================================================
// Jiles-Atherton magnetic hysteresis, solved per sample with second order
// Runge-Kutta. This is what gives tape its soft, level dependent compression,
// odd harmonics and the slight "smear" that a static waveshaper cannot.
class Hysteresis
{
public:
    void prepare (double sampleRate)
    {
        T = 1.0 / sampleRate;
        reset();
    }

    void reset() { M = H = Hd = 0; }

    // drive, saturation and width are 0..1.
    void setParameters (double drive, double saturation, double width)
    {
        Ms = 0.5 + 1.5 * (1 - saturation);
        a = Ms / (0.01 + 6 * drive);
        c = std::sqrt (std::max (0.0, 1 - width)) - 0.01;
        nc = 1 - c;
        MsOverA = Ms / a;
        // Small signal gain of the model is roughly the reversible part of the
        // anhysteretic slope, so this keeps quiet material at unity.
        makeup = 3 * a / (Ms * c);
    }

    double process (double x)
    {
        const auto Hn = x;
        const auto HdN = derivAlpha1 * (Hn - H) - derivAlpha * Hd;

        const auto k1 = T * dMdt (M, H, Hd);
        const auto k2 = T * dMdt (M + 0.5 * k1, 0.5 * (H + Hn), 0.5 * (Hd + HdN));
        auto Mn = M + k2;

        if (! std::isfinite (Mn) || std::abs (Mn) > 4 * Ms)
            Mn = 0;

        M = Mn;
        H = Hn;
        Hd = HdN;
        return M * makeup;
    }

    void updateDerivativeCoefficients()
    {
        derivAlpha1 = (1 + derivAlpha) / T;
    }

private:
    static constexpr double alpha = 1.6e-3;
    static constexpr double k = 0.47875;
    static constexpr double derivAlpha = 0.75;

    // Langevin function L(q) = coth(q) - 1/q and its derivative.
    static void langevin (double q, double& L, double& dL)
    {
        const auto aq = std::abs (q);

        if (aq < 1.0e-3)
        {
            L = q / 3;
            dL = 1.0 / 3;
            return;
        }

        if (aq > 20)
        {
            L = (q > 0 ? 1.0 : -1.0) - 1 / q;
            dL = 1 / (q * q);
            return;
        }

        const auto e = std::exp (2 * q);
        const auto em1 = e - 1;
        const auto coth = (e + 1) / em1;
        const auto invSinh2 = 4 * e / (em1 * em1);
        L = coth - 1 / q;
        dL = 1 / (q * q) - invSinh2;
    }

    double dMdt (double m, double h, double hd) const
    {
        const auto Q = (h + alpha * m) / a;
        double L, dL;
        langevin (Q, L, dL);

        const auto Mdiff = Ms * L - m;
        const auto delta = hd >= 0 ? 1.0 : -1.0;
        const auto deltaM = (delta > 0) == (Mdiff > 0) ? 1.0 : 0.0;

        const auto dLs = MsOverA * dL;
        auto denomIrr = nc * delta * k - alpha * Mdiff;

        if (std::abs (denomIrr) < 1.0e-6)
            denomIrr = denomIrr < 0 ? -1.0e-6 : 1.0e-6;

        const auto irreversible = nc * deltaM * Mdiff / denomIrr;
        const auto reversible = c * dLs;
        const auto denom = 1 - c * alpha * dLs;

        return hd * (irreversible + reversible) / denom;
    }

    double T = 1.0 / 192000.0;
    double derivAlpha1 = (1 + derivAlpha) * 192000.0;
    double Ms = 1, a = 1, c = 0.5, nc = 0.5, MsOverA = 1, makeup = 1;
    double M = 0, H = 0, Hd = 0;
};

//==============================================================================
// Fractional delay with cubic Hermite interpolation, used for wow and flutter.
class ModDelay
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 1;
        while (size < maxDelaySamples + 8)
            size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); }

    float process (float x, double delaySamples)
    {
        buffer[(size_t) writePos] = x;

        const auto d = std::max (1.0, delaySamples);
        const auto whole = (int) d;
        const auto frac = (float) (d - whole);

        const auto read = [this] (int offset) { return buffer[(size_t) ((writePos - offset) & mask)]; };
        const auto xm1 = read (whole - 1);
        const auto x0 = read (whole);
        const auto x1 = read (whole + 1);
        const auto x2 = read (whole + 2);

        writePos = (writePos + 1) & mask;

        const auto c1 = 0.5f * (x1 - xm1);
        const auto c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const auto c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * frac + c2) * frac + c1) * frac + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

//==============================================================================
class Random
{
public:
    explicit Random (uint32_t seed) : state (seed ? seed : 0x9e3779b9u) {}

    // Uniform in -1..1.
    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) ((double) state / 2147483648.0 - 1.0);
    }

private:
    uint32_t state;
};

//==============================================================================
// Everything the controls set. Choices are stored as indices.
struct Settings
{
    double inputDb = 0;
    double outputDb = 0;
    bool autoGain = true;
    int speed = 1;        // 0 = 7.5 ips, 1 = 15 ips, 2 = 30 ips
    int formula = 1;      // see formulas below
    int calibration = 1;  // 0 = +3 dB, 1 = +6 dB, 2 = +9 dB
    int emphasis = 0;     // 0 = NAB, 1 = CCIR
    double bias = 0;      // -1 under biased .. +1 over biased
    double wow = 0;       // 0..1
    double flutter = 0;   // 0..1
    double hiss = 0;      // 0..1, 0.5 is a realistic machine
    double mix = 1;       // 0..1
};

struct Formula
{
    const char* name;
    double drive, saturation, width;
    double hfScale, bumpScale, noiseDb;
};

// Four tape stocks, from older and easier to saturate to modern high output.
inline constexpr Formula formulas[] = {
    { "Vintage", 0.72, 0.55, 0.55, 0.85, 1.25, 2.0 },
    { "Classic", 0.60, 0.45, 0.50, 1.00, 1.00, 0.0 },
    { "Hot",     0.48, 0.32, 0.45, 1.10, 0.90, -2.0 },
    { "Modern",  0.38, 0.22, 0.40, 1.20, 0.80, -4.0 },
};

struct Speed
{
    double bumpHz, bumpDb, bumpQ;
    double hfHz;
    double modScale;
    double noiseDb;
    double nabTauUs, ccirTauUs;
};

inline constexpr Speed speeds[] = {
    { 34.0,  3.0, 1.2, 13000.0, 1.4, -66.0, 90.0, 70.0 },   // 7.5 ips
    { 58.0,  2.0, 1.0, 22000.0, 1.0, -70.0, 50.0, 35.0 },   // 15 ips
    { 105.0, 1.5, 0.9, 30000.0, 0.7, -73.0, 17.5, 17.5 },   // 30 ips
};

//==============================================================================
// The full tape path for one channel, split into the part before the
// oversampled hysteresis and the part after it.
class TapeChannel
{
public:
    explicit TapeChannel (uint32_t seed) : noise (seed) {}

    void prepare (double sampleRate, double oversampledRate)
    {
        fs = sampleRate;
        hysteresis.prepare (oversampledRate);
        hysteresis.updateDerivativeCoefficients();
        dcR = 1 - 2 * pi * 5.0 / fs;
        reset();
    }

    void reset()
    {
        for (auto* f : { &recordHf, &recordLf, &playHf, &playLf, &bump, &bumpDip, &lowCut, &hfLoss1, &hfLoss2, &noiseTilt })
            f->reset();
        hysteresis.reset();
        dcX = dcY = 0;
        pink = 0;
    }

    void configure (const Settings& s, double hfScale, double bumpScale)
    {
        const auto& sp = speeds[std::clamp (s.speed, 0, 2)];

        // Record and playback emphasis. High frequencies are boosted before
        // the tape and cut after, so they saturate earlier, like real tape.
        const auto tau = (s.emphasis == 0 ? sp.nabTauUs : sp.ccirTauUs) * 1.0e-6;
        const auto fz = 1.0 / (2 * pi * tau);
        recordHf.setFirstOrderShelf (fs, fz, fz * 4);
        playHf.setFirstOrderShelf (fs, fz * 4, fz);

        // NAB also records the deep lows quieter and lifts them on playback,
        // so bass below about 50 Hz saturates a little less.
        if (s.emphasis == 0)
        {
            recordLf.setFirstOrderShelf (fs, 50.0, 150.0, 1.0 / 3.0);
            playLf.setFirstOrderShelf (fs, 150.0, 50.0, 3.0);
        }
        else
        {
            recordLf.setIdentity();
            playLf.setIdentity();
        }

        // Head bump, the dip above it and the roll off below it.
        bump.setPeak (fs, sp.bumpHz, sp.bumpQ, sp.bumpDb * bumpScale);
        bumpDip.setPeak (fs, sp.bumpHz * 2.3, 1.4, -0.35 * sp.bumpDb * bumpScale);
        lowCut.setHighPass (fs, sp.bumpHz * 0.27, 0.6);

        // High frequency loss. Over biasing loses more top end, under biasing less.
        const auto biasHf = s.bias > 0 ? std::pow (2.0, -0.9 * s.bias) : 1.0 - 0.25 * s.bias;
        const auto hf = sp.hfHz * hfScale * biasHf;
        hfLoss1.setLowPass (fs, hf, 0.55);
        hfLoss2.setLowPass (fs, std::min (hf * 1.6, fs * 0.47), 0.8);

        noiseTilt.setLowPass (fs, std::min (hf * 0.8, fs * 0.45), 0.5);
    }

    void setHysteresis (double drive, double saturation, double width)
    {
        hysteresis.setParameters (drive, saturation, width);
    }

    float preProcess (float x)
    {
        return (float) recordHf.process (recordLf.process (x));
    }

    float hysteresisProcess (float x)
    {
        return (float) hysteresis.process (x);
    }

    float postProcess (float x)
    {
        auto y = playLf.process (playHf.process (x));
        y = lowCut.process (bumpDip.process (bump.process (y)));
        y = hfLoss2.process (hfLoss1.process (y));

        // The playback electronics add a little second harmonic on top of the
        // tape's mostly odd harmonics. The DC this creates is removed below.
        y += 0.04 * y * y;

        // DC blocker at about 5 Hz.
        const auto dc = y - dcX + dcR * dcY;
        dcX = y;
        dcY = dc;
        return (float) dc;
    }

    // Tape hiss: slightly pink white noise rolled off like the playback head.
    float hissSample()
    {
        const auto w = noise.next();
        pink = 0.97f * pink + 0.03f * w * 6.0f;
        return (float) noiseTilt.process (0.6f * w + 0.4f * pink);
    }

private:
    double fs = 48000;
    Biquad recordHf, recordLf, playHf, playLf, bump, bumpDip, lowCut, hfLoss1, hfLoss2, noiseTilt;
    Hysteresis hysteresis;
    double dcX = 0, dcY = 0, dcR = 0.999;
    Random noise;
    float pink = 0;
};

//==============================================================================
// Wow and flutter, shared by both channels like a real transport.
class Transport
{
public:
    static constexpr double centreDelaySeconds = 0.0025;
    static constexpr double maxDepthSeconds = 0.0023;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        phaseWow1 = phaseWow2 = phaseFl1 = phaseFl2 = 0;
        drift = driftTarget = flutterNoise = 0;
        driftCounter = 0;
    }

    int centreDelaySamples() const { return (int) std::lround (centreDelaySeconds * fs); }
    int maxDelaySamples() const { return (int) std::ceil ((centreDelaySeconds + maxDepthSeconds) * fs) + 4; }

    // Returns the delay in samples for this sample.
    double next (double wow, double flutter, double modScale)
    {
        const auto inc = 2 * pi / fs;
        phaseWow1 = std::fmod (phaseWow1 + inc * 0.52, 2 * pi);
        phaseWow2 = std::fmod (phaseWow2 + inc * 0.83, 2 * pi);
        phaseFl1 = std::fmod (phaseFl1 + inc * 7.3, 2 * pi);
        phaseFl2 = std::fmod (phaseFl2 + inc * 11.9, 2 * pi);

        // Slow random drift so the wow never sounds like a plain LFO.
        if (--driftCounter <= 0)
        {
            driftTarget = rng.next();
            driftCounter = (int) (fs * 0.4);
        }
        drift += (driftTarget - drift) * (1.0 / (fs * 0.6));
        flutterNoise += (rng.next() - flutterNoise) * (2 * pi * 25.0 / fs);

        const auto wowShape = 0.6 * std::sin (phaseWow1) + 0.25 * std::sin (phaseWow2) + 0.35 * drift;
        const auto flutterShape = std::sin (phaseFl1) + 0.45 * std::sin (phaseFl2) + 0.8 * flutterNoise;

        auto depth = wow * modScale * 1.1e-3 * wowShape + flutter * modScale * 2.4e-5 * flutterShape;
        depth = std::clamp (depth, -maxDepthSeconds, maxDepthSeconds);
        return (centreDelaySeconds + depth) * fs;
    }

private:
    double fs = 48000;
    double phaseWow1 = 0, phaseWow2 = 0, phaseFl1 = 0, phaseFl2 = 0;
    double drift = 0, driftTarget = 0, flutterNoise = 0;
    int driftCounter = 0;
    Random rng { 0x51ed27u };
};

//==============================================================================
// Level that puts a -18 dBFS signal (a typical mix level) at the tape's
// nominal operating point with +6 dB calibration.
inline constexpr double tapeReferenceGain = 1.4;

inline double calibrationDb (int index) { return (std::clamp (index, 0, 2) - 1) * 3.0; }

struct DerivedParameters
{
    double drive, saturation, width, hfScale, bumpScale, noiseGain;
};

inline DerivedParameters derive (const Settings& s)
{
    const auto& f = formulas[std::clamp (s.formula, 0, 3)];
    const auto& sp = speeds[std::clamp (s.speed, 0, 2)];

    DerivedParameters d;
    // Under biasing widens the loop and drives harder, over biasing cleans up.
    d.drive = std::clamp (f.drive * (1 - 0.25 * s.bias), 0.05, 0.95);
    d.saturation = f.saturation;
    d.width = std::clamp (f.width - 0.25 * s.bias, 0.05, 0.95);
    d.hfScale = f.hfScale;
    d.bumpScale = f.bumpScale;

    // Hotter calibration puts more level on tape, so hiss sits lower.
    const auto hissKnob = std::clamp (s.hiss, 0.0, 1.0);
    const auto hissDb = sp.noiseDb + f.noiseDb - calibrationDb (s.calibration);
    d.noiseGain = (hissKnob / 0.5) * (hissKnob / 0.5) * dbToGain (hissDb);
    return d;
}

} // namespace cruztape
