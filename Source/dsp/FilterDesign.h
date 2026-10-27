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
    tiltShelf
};

inline constexpr int numShapes = 8;
inline constexpr std::array<int, 9> slopesDbPerOct { 6, 12, 18, 24, 30, 36, 48, 72, 96 };
inline constexpr int maxSections = 8;

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
    return s == Shape::bell || s == Shape::lowShelf || s == Shape::highShelf || s == Shape::tiltShelf;
}

inline bool shapeUsesSlope (Shape s) noexcept
{
    return s == Shape::lowCut || s == Shape::highCut;
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

// Analog sections for a band, all normalised to the band frequency.
struct AnalogDesign
{
    std::array<AnalogSection, maxSections> sections {};
    int numSections = 0;

    void add (const AnalogSection& s) noexcept { sections[(size_t) numSections++] = s; }

    double powerAt (double x) const noexcept
    {
        double p = 1.0;
        for (int i = 0; i < numSections; ++i)
            p *= sections[(size_t) i].power (x);
        return p;
    }
};

inline AnalogDesign analogPrototype (const BandParams& p) noexcept
{
    AnalogDesign d;
    const double q = std::clamp (p.q, minQ, maxQ);
    const double A = std::pow (10.0, p.gainDb / 40.0);
    const double sqrtA = std::sqrt (A);

    switch (p.shape)
    {
        case Shape::bell:
            d.add ({ 1.0, A / q, 1.0, 1.0, 1.0 / (A * q), 1.0 });
            break;

        case Shape::lowShelf:
            d.add ({ A, A * sqrtA / q, A * A, A, sqrtA / q, 1.0 });
            break;

        case Shape::highShelf:
            d.add ({ A * A, A * sqrtA / q, A, 1.0, sqrtA / q, A });
            break;

        case Shape::tiltShelf:
            // High shelf with its gain split evenly around the band frequency.
            d.add ({ A, sqrtA / q, 1.0, 1.0, sqrtA / q, A });
            break;

        case Shape::notch:
            d.add ({ 1.0, 0.0, 1.0, 1.0, 1.0 / q, 1.0 });
            break;

        case Shape::bandPass:
            d.add ({ 0.0, 1.0 / q, 0.0, 1.0, 1.0 / q, 1.0 });
            break;

        case Shape::lowCut:
        case Shape::highCut:
        {
            // Butterworth cascade. Q adds resonance to the sharpest section, so
            // the default Q gives a flat (maximally flat) response.
            const bool highPass = p.shape == Shape::lowCut;
            const int order = std::clamp (p.slopeDbPerOct / 6, 1, 2 * maxSections);
            const int pairs = order / 2;

            if (order % 2 == 1)
                d.add (highPass ? AnalogSection { 0.0, 1.0, 0.0, 0.0, 1.0, 1.0 }
                                : AnalogSection { 0.0, 0.0, 1.0, 0.0, 1.0, 1.0 });

            for (int k = 1; k <= pairs; ++k)
            {
                double sectionQ = 1.0 / (2.0 * std::sin ((2 * k - 1) * pi / (2.0 * order)));
                if (k == 1)
                    sectionQ *= q / defaultQ;

                d.add (highPass ? AnalogSection { 1.0, 0.0, 0.0, 1.0, 1.0 / sectionQ, 1.0 }
                                : AnalogSection { 0.0, 0.0, 1.0, 1.0, 1.0 / sectionQ, 1.0 });
            }
            break;
        }
    }

    return d;
}

inline BandDesign designBand (const BandParams& p, double sampleRate) noexcept
{
    const double w0 = 2.0 * pi * std::clamp (p.freqHz, minFreqHz, maxFreqHz) / sampleRate;
    const double absGainDb = std::abs (p.gainDb);
    const bool cut = p.gainDb < 0.0;

    BandDesign d;

    const auto single = [&d] (const BiquadCoeffs& c)
    {
        d.sections[0] = c;
        d.numSections = 1;
    };

    // Every gain shape is built from a boost, which matched coefficients can
    // always realise. Cuts are exact inverses of boosts in the analog domain,
    // and a high shelf is a low shelf turned upside down.
    switch (p.shape)
    {
        case Shape::bell:
        {
            auto boost = p;
            boost.gainDb = absGainDb;
            const auto c = matchSecondOrder (analogPrototype (boost).sections[0], w0);
            single (cut ? inverse (c) : c);
            break;
        }

        case Shape::lowShelf:
        case Shape::highShelf:
        case Shape::tiltShelf:
        {
            BandParams lowBoost { Shape::lowShelf, p.freqHz, absGainDb, p.q, p.slopeDbPerOct };
            const auto low = matchSecondOrder (analogPrototype (lowBoost).sections[0], w0);
            const double g = std::pow (10.0, absGainDb / 20.0);

            if (p.shape == Shape::lowShelf)
                single (cut ? inverse (low) : low);
            else if (p.shape == Shape::highShelf)
                single (cut ? scaled (low, 1.0 / g) : scaled (inverse (low), g));
            else
                single (cut ? scaled (low, 1.0 / std::sqrt (g)) : scaled (inverse (low), std::sqrt (g)));
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
