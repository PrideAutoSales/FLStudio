#pragma once

// Zero-latency filter design.
//
// Every band is described by an analog prototype and turned into biquads with
// "matched" coefficients: the poles come from the exact (impulse-invariant)
// mapping of the analog poles, and the zeros are solved so the digital magnitude
// equals the analog magnitude at DC, at Nyquist and at a third frequency near the
// band. This avoids the cramping of bilinear-transform EQs near the top of the
// spectrum while staying minimum phase with no added latency.
// Reference: M. Vicanek, "Matched Second Order Digital Filters" (2016).

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>

namespace cruz::dsp
{
inline constexpr double pi = 3.14159265358979323846;

enum class Shape
{
    bell,
    lowShelf,
    lowCut,
    highShelf,
    highCut,
    notch,
    bandPass,
    tiltShelf,
    flatTilt
};

inline constexpr int numShapes = 9;
inline constexpr std::array<int, 9> slopesDbPerOct { 6, 12, 18, 24, 30, 36, 48, 72, 96 };
inline constexpr int maxSections = 8;
inline constexpr int maxAnalogSections = 16;

inline constexpr double minFreqHz = 10.0;
inline constexpr double maxFreqHz = 30000.0;
inline constexpr double minGainDb = -30.0;
inline constexpr double maxGainDb = 30.0;
inline constexpr double minQ = 0.025;
inline constexpr double maxQ = 40.0;
inline constexpr double defaultQ = 0.70710678118654752;

// Exact comparison, written so compilers do not flag it: the zeros it checks
// for are structural, not the result of arithmetic.
inline bool exactlyEqual (double a, double b) noexcept
{
    return ! (a < b) && ! (a > b);
}

inline bool shapeUsesGain (Shape s) noexcept
{
    return s == Shape::bell || s == Shape::lowShelf || s == Shape::highShelf || s == Shape::tiltShelf || s == Shape::flatTilt;
}

inline bool shapeUsesSlope (Shape s) noexcept
{
    return s != Shape::bell && s != Shape::flatTilt;
}

inline bool shapeUsesQ (Shape s) noexcept
{
    return s != Shape::flatTilt;
}

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

    std::complex<double> response (double w) const noexcept
    {
        const auto z1 = std::polar (1.0, -w);
        const auto z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }
};

// H(s) = (n2 s^2 + n1 s + n0) / (d2 s^2 + d1 s + d0), with s normalised to the
// section's own frequency. A first-order section has n2 = d2 = 0.
struct AnalogSection
{
    double n2, n1, n0, d2, d1, d0;

    bool isFirstOrder() const noexcept { return exactlyEqual (d2, 0.0); }

    // |H(jx)|^2 at normalised frequency x.
    double power (double x) const noexcept
    {
        const double nr = n0 - n2 * x * x, ni = n1 * x;
        const double dr = d0 - d2 * x * x, di = d1 * x;
        return (nr * nr + ni * ni) / (dr * dr + di * di);
    }

    // H(jx), with phase.
    std::complex<double> response (double x) const noexcept
    {
        return std::complex<double> (n0 - n2 * x * x, n1 * x) / std::complex<double> (d0 - d2 * x * x, d1 * x);
    }
};

// Frequency weights for the power-spectrum form |H|^2 = (B.phi) / (A.phi).
struct Phi
{
    double p1, p0, p2;

    explicit Phi (double w) noexcept
        : p1 (std::pow (std::sin (0.5 * w), 2.0)), p0 (1.0 - p1), p2 (4.0 * p0 * p1)
    {
    }
};

// One-pole sections have three free coefficients, so the pole is solved too
// and the magnitude matches the analog one exactly at DC, at the band (or
// just below Nyquist when the band is above it) and at Nyquist.
inline BiquadCoeffs matchFirstOrder (const AnalogSection& h, double w0) noexcept
{
    BiquadCoeffs c;
    const double wm = std::min (w0, 0.9 * pi);
    const Phi fm { wm };
    const double t0 = h.power (0.0), tm = h.power (wm / w0), tn = h.power (pi / w0);
    const double ratio = fm.p1 * (tm - tn) / (fm.p0 * (t0 - tm));

    if (std::isfinite (ratio) && ratio > 0.0)
    {
        const double r = std::sqrt (ratio);
        c.a1 = (r - 1.0) / (r + 1.0);
    }
    else
    {
        c.a1 = -std::exp (-w0 * h.d0 / h.d1);
    }

    const double s0 = std::sqrt (t0) * (1.0 + c.a1);
    const double s1 = std::sqrt (tn) * (1.0 - c.a1);
    c.b0 = 0.5 * (s0 + s1);
    c.b1 = 0.5 * (s0 - s1);
    return c;
}

inline BiquadCoeffs matchPoles (const AnalogSection& h, double w0) noexcept
{
    // Exact mapping of the analog poles, z = exp(sT).
    BiquadCoeffs c;
    const double wp = w0 * std::sqrt (h.d0 / h.d2);
    const double zeta = h.d1 / (2.0 * std::sqrt (h.d0 * h.d2));
    const double e = std::exp (-zeta * wp);
    c.a2 = e * e;

    if (zeta <= 1.0)
        c.a1 = -2.0 * e * std::cos (std::min (wp * std::sqrt (1.0 - zeta * zeta), pi));
    else
        c.a1 = -2.0 * e * std::cosh (wp * std::sqrt (zeta * zeta - 1.0));

    return c;
}

inline BiquadCoeffs matchSecondOrder (const AnalogSection& h, double w0) noexcept
{
    auto c = matchPoles (h, w0);

    const double A0 = (1.0 + c.a1 + c.a2) * (1.0 + c.a1 + c.a2);
    const double A1 = (1.0 - c.a1 + c.a2) * (1.0 - c.a1 + c.a2);
    const double A2 = -4.0 * c.a2;
    const auto denPower = [&] (const Phi& f) { return A0 * f.p0 + A1 * f.p1 + A2 * f.p2; };

    // Third match point: the band frequency, kept clear of Nyquist so the
    // solve stays well conditioned when the band sits at or above it.
    const double wm = std::min (w0, 0.9 * pi);
    const Phi fm { wm };
    const double target = h.power (wm / w0) * denPower (fm);

    if (! exactlyEqual (h.n2, 0.0) && exactlyEqual (h.n1, 0.0) && exactlyEqual (h.n0, 0.0))
    {
        // High-pass: keep the double zero at DC and match the gain at the band.
        c.b0 = std::sqrt (target) / (4.0 * fm.p1);
        c.b1 = -2.0 * c.b0;
        c.b2 = c.b0;
        return c;
    }

    const double B0 = A0 * h.power (0.0);

    if (exactlyEqual (h.n2, 0.0) && exactlyEqual (h.n1, 0.0))
    {
        // Low-pass: match DC and the band, with b2 = 0.
        const double B1 = std::max ((target - B0 * fm.p0) / fm.p1, 0.0);
        c.b0 = 0.5 * (std::sqrt (B0) + std::sqrt (B1));
        c.b1 = std::sqrt (B0) - c.b0;
        c.b2 = 0.0;
        return c;
    }

    // Resonant zeros (notches, high-Q shelves) are placed with the same exact
    // mapping as the poles, so the dip lands where the analog one does.
    const bool quadraticZeros = h.n0 > 0.0 && h.n2 > 0.0;
    const double zetaZeros = quadraticZeros ? h.n1 / (2.0 * std::sqrt (h.n0 * h.n2)) : 1.0;
    const double wz = quadraticZeros ? w0 * std::sqrt (h.n0 / h.n2) : 0.0;

    const auto mapZeros = [&]
    {
        const auto zeros = matchPoles ({ 0.0, 0.0, 0.0, h.n2, h.n1, h.n0 }, w0);
        const double k = std::sqrt (B0) / (1.0 + zeros.a1 + zeros.a2);
        c.b0 = k;
        c.b1 = k * zeros.a1;
        c.b2 = k * zeros.a2;
        return c;
    };

    if (quadraticZeros && zetaZeros < 0.5 && wz * std::sqrt (1.0 - zetaZeros * zetaZeros) < 0.9 * pi)
        return mapZeros();

    // General case: match DC, Nyquist and the band, then recover the
    // minimum-phase numerator from its power spectrum.
    const double B1 = A1 * h.power (pi / w0);
    const double B2 = (target - B0 * fm.p0 - B1 * fm.p1) / fm.p2;

    const double s0 = std::sqrt (std::max (B0, 0.0));
    const double s1 = std::sqrt (std::max (B1, 0.0));
    const double w = 0.5 * (s0 + s1);
    const double disc = w * w + B2;

    c.b1 = 0.5 * (s0 - s1);

    // Not realisable with these poles (bands at or above Nyquist): fall back to
    // mapped zeros, which are always minimum phase and so safe to invert. A
    // band pass is never inverted, so its closest fit is fine.
    if (disc < 0.0)
    {
        if (quadraticZeros)
            return mapZeros();

        c.b0 = c.b2 = 0.5 * w;
        return c;
    }

    c.b0 = 0.5 * (w + std::sqrt (disc));
    c.b2 = 0.5 * (w - std::sqrt (disc));
    return c;
}

inline BiquadCoeffs matchSection (const AnalogSection& h, double w0) noexcept
{
    return h.isFirstOrder() ? matchFirstOrder (h, w0) : matchSecondOrder (h, w0);
}

inline BiquadCoeffs inverse (const BiquadCoeffs& c) noexcept
{
    return { 1.0 / c.b0, c.a1 / c.b0, c.a2 / c.b0, c.b1 / c.b0, c.b2 / c.b0 };
}

inline BiquadCoeffs scaled (BiquadCoeffs c, double gain) noexcept
{
    c.b0 *= gain;
    c.b1 *= gain;
    c.b2 *= gain;
    return c;
}

struct BandParams
{
    Shape shape = Shape::bell;
    double freqHz = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slopeDbPerOct = 12;
};

struct BandDesign
{
    std::array<BiquadCoeffs, maxSections> sections {};
    int numSections = 0;

    std::complex<double> response (double w) const noexcept
    {
        std::complex<double> h { 1.0, 0.0 };
        for (int i = 0; i < numSections; ++i)
            h *= sections[(size_t) i].response (w);
        return h;
    }

    double magnitudeDb (double w) const noexcept
    {
        return 20.0 * std::log10 (std::max (std::abs (response (w)), 1.0e-12));
    }
};

// Analog sections for a band, all normalised to the band frequency, plus an
// overall amplitude factor.
struct AnalogDesign
{
    std::array<AnalogSection, maxAnalogSections> sections {};
    int numSections = 0;
    double gain = 1.0;

    void add (const AnalogSection& s) noexcept { sections[(size_t) numSections++] = s; }

    double powerAt (double x) const noexcept
    {
        double p = gain * gain;
        for (int i = 0; i < numSections; ++i)
            p *= sections[(size_t) i].power (x);
        return p;
    }

    std::complex<double> responseAt (double x) const noexcept
    {
        std::complex<double> h (gain, 0.0);
        for (int i = 0; i < numSections; ++i)
            h *= sections[(size_t) i].response (x);
        return h;
    }
};

inline AnalogSection swapped (const AnalogSection& s) noexcept
{
    return { s.d2, s.d1, s.d0, s.n2, s.n1, s.n0 };
}

inline int orderForSlope (int slopeDbPerOct) noexcept
{
    return std::clamp (slopeDbPerOct / 6, 1, 2 * maxSections);
}

// Q of section k (from 1) of an order-n Butterworth cascade. The band's Q adds
// resonance to the sharpest section, so the default Q is maximally flat.
inline double butterworthQ (int k, int order, double q) noexcept
{
    const double sectionQ = 1.0 / (2.0 * std::sin ((2 * k - 1) * pi / (2.0 * order)));
    return k == 1 ? sectionQ * std::clamp (q, minQ, maxQ) / defaultQ : sectionQ;
}

// Low shelf boost of any order: Butterworth poles below the band frequency and
// matching zeros above it, so the curve passes through half its gain (in dB)
// at the band frequency and gets steeper with the order. Order 2 is the
// classic 12 dB/oct shelf.
inline AnalogDesign lowShelfBoost (double gainDb, double q, int order) noexcept
{
    AnalogDesign d;
    const double g = std::pow (10.0, std::abs (gainDb) / 20.0);
    const double z = std::pow (g, 1.0 / (2.0 * order));
    const double p = 1.0 / z;

    if (order % 2 == 1)
        d.add ({ 0.0, 1.0, z, 0.0, 1.0, p });

    for (int k = 1; k <= order / 2; ++k)
    {
        const double sq = butterworthQ (k, order, q);
        d.add ({ 1.0, z / sq, z * z, 1.0, p / sq, p * p });
    }

    return d;
}

// Flat tilt: a constant dB/oct slope through the band frequency, built from
// first-order shelves one octave apart from 2.5 Hz to 160 kHz, so the slope
// is even across the audible range. The gain is the total tilt between 20 Hz
// and 20 kHz.
inline constexpr int flatTiltShelves = 16;

inline double flatTiltShelfHz (int i) noexcept
{
    return 2.5 * std::pow (2.0, i + 0.5);
}

inline double flatTiltStepGain (double gainDb) noexcept
{
    return std::pow (10.0, gainDb / std::log2 (1000.0) / 20.0);
}

inline AnalogDesign analogPrototype (const BandParams& p) noexcept
{
    AnalogDesign d;
    const double q = std::clamp (p.q, minQ, maxQ);
    const double A = std::pow (10.0, p.gainDb / 40.0);
    const double g = std::pow (10.0, std::abs (p.gainDb) / 20.0);
    const bool cut = p.gainDb < 0.0;

    switch (p.shape)
    {
        case Shape::bell:
            d.add ({ 1.0, A / q, 1.0, 1.0, 1.0 / (A * q), 1.0 });
            break;

        case Shape::lowShelf:
        case Shape::highShelf:
        case Shape::tiltShelf:
        {
            // A cut is the boost turned upside down, and a high shelf is a
            // low shelf flipped and lifted by its gain.
            const auto boost = lowShelfBoost (p.gainDb, q, orderForSlope (p.slopeDbPerOct));
            const bool flip = (p.shape == Shape::lowShelf) == cut;

            for (int i = 0; i < boost.numSections; ++i)
                d.add (flip ? swapped (boost.sections[(size_t) i]) : boost.sections[(size_t) i]);

            if (p.shape == Shape::highShelf)
                d.gain = cut ? 1.0 / g : g;
            else if (p.shape == Shape::tiltShelf)
                d.gain = cut ? 1.0 / std::sqrt (g) : std::sqrt (g);
            break;
        }

        case Shape::flatTilt:
        {
            const double step = flatTiltStepGain (p.gainDb);
            const double rootStep = std::sqrt (step);

            for (int i = 0; i < flatTiltShelves; ++i)
            {
                const double c = flatTiltShelfHz (i) / p.freqHz;
                d.add ({ 0.0, step, c * rootStep, 0.0, 1.0, c * rootStep });
            }

            d.gain = 1.0 / std::sqrt (d.powerAt (1.0));
            break;
        }

        case Shape::notch:
        case Shape::bandPass:
        {
            // Steeper slopes cascade identical sections.
            const int copies = std::clamp (p.slopeDbPerOct / 12, 1, maxSections);
            const AnalogSection section = p.shape == Shape::notch ? AnalogSection { 1.0, 0.0, 1.0, 1.0, 1.0 / q, 1.0 }
                                                                  : AnalogSection { 0.0, 1.0 / q, 0.0, 1.0, 1.0 / q, 1.0 };
            for (int i = 0; i < copies; ++i)
                d.add (section);
            break;
        }

        case Shape::lowCut:
        case Shape::highCut:
        {
            const bool highPass = p.shape == Shape::lowCut;
            const int order = orderForSlope (p.slopeDbPerOct);

            if (order % 2 == 1)
                d.add (highPass ? AnalogSection { 0.0, 1.0, 0.0, 0.0, 1.0, 1.0 }
                                : AnalogSection { 0.0, 0.0, 1.0, 0.0, 1.0, 1.0 });

            for (int k = 1; k <= order / 2; ++k)
            {
                const double sq = butterworthQ (k, order, q);
                d.add (highPass ? AnalogSection { 1.0, 0.0, 0.0, 1.0, 1.0 / sq, 1.0 }
                                : AnalogSection { 0.0, 0.0, 1.0, 1.0, 1.0 / sq, 1.0 });
            }
            break;
        }
    }

    return d;
}

// Product of two first-order sections as one biquad.
inline BiquadCoeffs combined (const BiquadCoeffs& x, const BiquadCoeffs& y) noexcept
{
    return { x.b0 * y.b0, x.b0 * y.b1 + x.b1 * y.b0, x.b1 * y.b1, x.a1 + y.a1, x.a1 * y.a1 };
}

inline BandDesign designBand (const BandParams& p, double sampleRate) noexcept
{
    const double w0 = 2.0 * pi * std::clamp (p.freqHz, minFreqHz, maxFreqHz) / sampleRate;
    const double g = std::pow (10.0, std::abs (p.gainDb) / 20.0);
    const bool cut = p.gainDb < 0.0;

    BandDesign d;

    // Every gain shape is built from a boost, which matched coefficients can
    // always realise. Cuts are exact inverses of boosts in the analog domain,
    // and a high shelf is a low shelf turned upside down.
    switch (p.shape)
    {
        case Shape::bell:
        {
            auto boost = p;
            boost.gainDb = std::abs (p.gainDb);
            const auto c = matchSecondOrder (analogPrototype (boost).sections[0], w0);
            d.sections[0] = cut ? inverse (c) : c;
            d.numSections = 1;
            break;
        }

        case Shape::lowShelf:
        case Shape::highShelf:
        case Shape::tiltShelf:
        {
            const auto boost = lowShelfBoost (p.gainDb, p.q, orderForSlope (p.slopeDbPerOct));
            d.numSections = boost.numSections;

            for (int i = 0; i < boost.numSections; ++i)
            {
                const auto& section = boost.sections[(size_t) i];
                const auto low = matchSection (section, w0);
                const double sectionGain = std::sqrt (section.power (0.0));

                if (p.shape == Shape::lowShelf)
                    d.sections[(size_t) i] = cut ? inverse (low) : low;
                else
                    d.sections[(size_t) i] = cut ? scaled (low, 1.0 / sectionGain) : scaled (inverse (low), sectionGain);
            }

            if (p.shape == Shape::tiltShelf)
                d.sections[0] = scaled (d.sections[0], cut ? std::sqrt (g) : 1.0 / std::sqrt (g));
            break;
        }

        case Shape::flatTilt:
        {
            // Each shelf is matched at its own frequency, then pairs share a biquad.
            const double step = flatTiltStepGain (p.gainDb);
            const double rootStep = std::sqrt (step);
            const AnalogSection shelf { 0.0, step, rootStep, 0.0, 1.0, rootStep };

            for (int i = 0; i < flatTiltShelves; i += 2)
            {
                const auto a = matchFirstOrder (shelf, 2.0 * pi * flatTiltShelfHz (i) / sampleRate);
                const auto b = matchFirstOrder (shelf, 2.0 * pi * flatTiltShelfHz (i + 1) / sampleRate);
                d.sections[(size_t) d.numSections++] = combined (a, b);
            }

            d.sections[0] = scaled (d.sections[0], 1.0 / std::abs (d.response (std::min (w0, 0.9999 * pi))));
            break;
        }

        case Shape::notch:
        case Shape::bandPass:
        case Shape::lowCut:
        case Shape::highCut:
        {
            const auto analog = analogPrototype (p);
            d.numSections = analog.numSections;
            for (int i = 0; i < analog.numSections; ++i)
                d.sections[(size_t) i] = matchSection (analog.sections[(size_t) i], w0);
            break;
        }
    }

    return d;
}

// Transposed direct form II, run in double precision so low bands at high
// sample rates stay quiet and accurate.
struct BiquadState
{
    double s1 = 0.0, s2 = 0.0;

    double process (const BiquadCoeffs& c, double x) noexcept
    {
        const double y = c.b0 * x + s1;
        s1 = c.b1 * x - c.a1 * y + s2;
        s2 = c.b2 * x - c.a2 * y;
        return y;
    }
};
} // namespace cruz::dsp
