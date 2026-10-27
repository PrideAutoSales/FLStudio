#pragma once

#include <cmath>
#include <complex>

namespace cruzverb
{
    // A single second-order filter section (RBJ cookbook designs, transposed direct form II).
    struct Biquad
    {
        struct Coeffs
        {
            double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;

            static constexpr double pi = 3.14159265358979323846;

            static Coeffs identity() noexcept { return {}; }

            static double clampFreq (double freq, double fs) noexcept
            {
                return std::fmin (std::fmax (freq, 1.0), fs * 0.49);
            }

            static Coeffs lowShelf (double fs, double freq, double gainDb) noexcept
            {
                return shelf (fs, freq, gainDb, false);
            }

            static Coeffs highShelf (double fs, double freq, double gainDb) noexcept
            {
                return shelf (fs, freq, gainDb, true);
            }

            static Coeffs highPass (double fs, double freq, double q = 0.7071) noexcept
            {
                const auto w = 2.0 * pi * clampFreq (freq, fs) / fs;
                const auto cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
                return normalise ((1.0 + cw) * 0.5, -(1.0 + cw), (1.0 + cw) * 0.5,
                                  1.0 + alpha, -2.0 * cw, 1.0 - alpha);
            }

            static Coeffs lowPass (double fs, double freq, double q = 0.7071) noexcept
            {
                const auto w = 2.0 * pi * clampFreq (freq, fs) / fs;
                const auto cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
                return normalise ((1.0 - cw) * 0.5, 1.0 - cw, (1.0 - cw) * 0.5,
                                  1.0 + alpha, -2.0 * cw, 1.0 - alpha);
            }

            double magnitude (double freq, double fs) const noexcept
            {
                const auto w = 2.0 * pi * freq / fs;
                const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
                return std::abs ((b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2));
            }

        private:
            static Coeffs shelf (double fs, double freq, double gainDb, bool high) noexcept
            {
                if (std::abs (gainDb) < 1.0e-6)
                    return identity();

                const auto A = std::pow (10.0, gainDb / 40.0);
                const auto w = 2.0 * pi * clampFreq (freq, fs) / fs;
                const auto cw = std::cos (w);
                const auto alpha = std::sin (w) / 2.0 * std::sqrt (2.0); // shelf slope S = 1
                const auto k = 2.0 * std::sqrt (A) * alpha;

                if (high)
                    return normalise (A * ((A + 1.0) + (A - 1.0) * cw + k),
                                      -2.0 * A * ((A - 1.0) + (A + 1.0) * cw),
                                      A * ((A + 1.0) + (A - 1.0) * cw - k),
                                      (A + 1.0) - (A - 1.0) * cw + k,
                                      2.0 * ((A - 1.0) - (A + 1.0) * cw),
                                      (A + 1.0) - (A - 1.0) * cw - k);

                return normalise (A * ((A + 1.0) - (A - 1.0) * cw + k),
                                  2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                                  A * ((A + 1.0) - (A - 1.0) * cw - k),
                                  (A + 1.0) + (A - 1.0) * cw + k,
                                  -2.0 * ((A - 1.0) + (A + 1.0) * cw),
                                  (A + 1.0) + (A - 1.0) * cw - k);
            }

            static Coeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2) noexcept
            {
                return { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0 };
            }
        };

        void setCoeffs (const Coeffs& c) noexcept
        {
            b0 = (float) c.b0; b1 = (float) c.b1; b2 = (float) c.b2;
            a1 = (float) c.a1; a2 = (float) c.a2;
        }

        void reset() noexcept { s1 = s2 = 0.0f; }

        float process (float x) noexcept
        {
            const auto y = b0 * x + s1;
            s1 = b1 * x - a1 * y + s2;
            s2 = b2 * x - a2 * y;
            return y;
        }

        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float s1 = 0.0f, s2 = 0.0f;
    };
}
