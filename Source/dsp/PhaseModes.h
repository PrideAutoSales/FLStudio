#pragma once

// Natural phase and linear phase modes. Instead of running the bands as IIR
// filters, the whole EQ is turned into FIR kernels and applied by convolution:
//
// - Linear phase: the exact analog magnitude of every band with no phase
//   shift at all. The kernel is symmetric, so it adds half its length as
//   latency. Longer kernels (higher resolution) keep narrow low bands exact.
// - Natural phase: the exact analog magnitude and phase, so the EQ behaves
//   like an analog circuit all the way to the top of the spectrum, with only
//   a short latency to hold the ringing that the band limit adds.
//
// Mid and side bands are folded into a 2x2 matrix of kernels, so every mode
// has a single, fixed latency. This file has no JUCE dependency so the tests
// can check the kernels directly.

#include "EqEngine.h"

#include <vector>

namespace cruz::dsp
{
enum class PhaseMode
{
    zeroLatency,
    natural,
    linear
};

inline constexpr int numPhaseModes = 3;
inline constexpr int numLinearResolutions = 5; // low, medium, high, very high, maximum

// Kernels grow with the sample rate so their resolution in Hz stays the same.
inline int rateFactor (double sampleRate) noexcept
{
    return sampleRate > 100000.0 ? 4 : sampleRate > 50000.0 ? 2 : 1;
}

inline int kernelLength (PhaseMode mode, int resolution, double sampleRate) noexcept
{
    if (mode == PhaseMode::zeroLatency)
        return 0;

    const int base = mode == PhaseMode::natural ? 8192 : 2048 << std::clamp (resolution, 0, numLinearResolutions - 1);
    return base * rateFactor (sampleRate);
}

// Delay of the kernel's main tap, which is the latency the host is told about.
inline int kernelLatency (PhaseMode mode, int length, double sampleRate) noexcept
{
    switch (mode)
    {
        case PhaseMode::natural: return 64 * rateFactor (sampleRate);
        case PhaseMode::linear: return length / 2;
        case PhaseMode::zeroLatency: break;
    }
    return 0;
}

// Plain radix-2 complex FFT, used off the audio thread to build kernels.
class Fft
{
public:
    void transform (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        prepare (n);

        for (size_t i = 1, j = 0; i < n; ++i)
        {
            size_t bit = n >> 1;
            for (; (j & bit) != 0; bit >>= 1)
                j ^= bit;
            j ^= bit;

            if (i < j)
                std::swap (a[i], a[j]);
        }

        for (size_t len = 2; len <= n; len <<= 1)
        {
            const size_t step = n / len;

            for (size_t i = 0; i < n; i += len)
            {
                for (size_t k = 0; k < len / 2; ++k)
                {
                    auto w = twiddles[k * step];
                    if (inverse)
                        w = std::conj (w);

                    const auto u = a[i + k];
                    const auto v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                }
            }
        }

        if (inverse)
            for (auto& x : a)
                x /= (double) n;
    }

private:
    void prepare (size_t n)
    {
        if (twiddles.size() == n / 2)
            return;

        twiddles.resize (n / 2);
        for (size_t k = 0; k < n / 2; ++k)
            twiddles[k] = std::polar (1.0, -2.0 * pi * (double) k / (double) n);
    }

    std::vector<std::complex<double>> twiddles;
};

// The kernels for one EQ setting. Stereo output is
//   left  = leftToLeft * left + rightToLeft * right
//   right = rightToRight * right + leftToRight * left
// and the cross terms are only needed when a band works on mid or side.
struct EqKernels
{
    int length = 0, latency = 0;
    bool hasCrossTerms = false;
    std::vector<float> leftToLeft, rightToRight, rightToLeft, leftToRight, mono;
};

class KernelDesigner
{
public:
    template <size_t N>
    void design (const std::array<BandTarget, N>& bands, const std::array<double, N>& dynamicDb,
                 PhaseMode mode, int resolution, double sampleRate, EqKernels& out)
    {
        out.length = kernelLength (mode, resolution, sampleRate);
        out.latency = kernelLatency (mode, out.length, sampleRate);
        out.hasCrossTerms = false;

        if (out.length == 0)
            return;

        // Design on a grid twice the kernel length, so the parts of each
        // response that do not fit are cut off cleanly instead of wrapping.
        const size_t size = (size_t) out.length * 2, bins = size / 2 + 1;
        const bool linear = mode == PhaseMode::linear;

        for (auto* h : { &hLeft, &hRight, &hMid, &hSide, &hMono })
            h->assign (bins, { 1.0, 0.0 });

        for (size_t b = 0; b < N; ++b)
        {
            const auto& t = bands[b];
            if (! t.active)
                continue;

            auto p = t.params;
            if (t.isDynamic())
                p.gainDb += dynamicDb[b];

            const auto analog = analogPrototype (p);
            const double freq = std::clamp (p.freqHz, minFreqHz, maxFreqHz);

            for (size_t k = 0; k < bins; ++k)
            {
                const double x = (double) k * sampleRate / (double) size / freq;
                std::complex<double> h;

                if (linear)
                    h = std::sqrt (analog.powerAt (x));
                else
                    h = analog.responseAt (x);

                switch (t.placement)
                {
                    case Placement::stereo: hLeft[k] *= h; hRight[k] *= h; break;
                    case Placement::left: hLeft[k] *= h; break;
                    case Placement::right: hRight[k] *= h; break;
                    case Placement::mid: hMid[k] *= h; break;
                    case Placement::side: hSide[k] *= h; break;
                }

                if (t.placement != Placement::side)
                    hMono[k] *= h;
            }

            out.hasCrossTerms |= t.placement == Placement::mid || t.placement == Placement::side;
        }

        spectrum.resize (bins);
        const auto build = [&] (std::vector<float>& kernel, auto&& response)
        {
            for (size_t k = 0; k < bins; ++k)
                spectrum[k] = response (k);
            toKernel (kernel, linear, out.length, out.latency);
        };

        build (out.leftToLeft, [&] (size_t k) { return 0.5 * (hMid[k] + hSide[k]) * hLeft[k]; });
        build (out.rightToRight, [&] (size_t k) { return 0.5 * (hMid[k] + hSide[k]) * hRight[k]; });
        build (out.mono, [&] (size_t k) { return hMono[k]; });

        if (out.hasCrossTerms)
        {
            build (out.rightToLeft, [&] (size_t k) { return 0.5 * (hMid[k] - hSide[k]) * hRight[k]; });
            build (out.leftToRight, [&] (size_t k) { return 0.5 * (hMid[k] - hSide[k]) * hLeft[k]; });
        }
        else
        {
            out.rightToLeft.assign ((size_t) out.length, 0.0f);
            out.leftToRight.assign ((size_t) out.length, 0.0f);
        }
    }

private:
    // Turns `spectrum` (bins 0 to size/2) into a kernel of `length` taps
    // whose main tap sits at `delay`.
    void toKernel (std::vector<float>& kernel, bool linear, int length, int delay)
    {
        const size_t bins = spectrum.size(), size = (bins - 1) * 2;
        work.assign (size, {});

        for (size_t k = 0; k < bins; ++k)
        {
            auto h = spectrum[k];

            // DC and the top bin must be real for a real kernel.
            if (k == 0 || k == bins - 1)
                h = std::abs (h) * (h.real() < 0.0 ? -1.0 : 1.0);

            work[k] = h * std::polar (1.0, -2.0 * pi * (double) k * delay / (double) size);
            if (k > 0 && k < bins - 1)
                work[size - k] = std::conj (work[k]);
        }

        fft.transform (work, true);
        kernel.resize ((size_t) length);

        // Fade the ends so cutting the response short does not ripple. The
        // linear kernel fades symmetrically around its centre; the natural
        // kernel fades in over its short lead-in and out over its last eighth.
        const int fadeOut = length / 8;

        for (int n = 0; n < length; ++n)
        {
            double w = 1.0;

            if (linear)
            {
                const int fromEdge = std::min (n, length - 1 - n);
                if (fromEdge < fadeOut)
                    w = 0.5 - 0.5 * std::cos (pi * (fromEdge + 0.5) / fadeOut);
            }
            else
            {
                if (n < delay)
                    w = 0.5 - 0.5 * std::cos (pi * (n + 0.5) / delay);
                else if (length - 1 - n < fadeOut)
                    w = 0.5 - 0.5 * std::cos (pi * (length - 1 - n + 0.5) / fadeOut);
            }

            kernel[(size_t) n] = (float) (work[(size_t) n].real() * w);
        }
    }

    Fft fft;
    std::vector<std::complex<double>> hLeft, hRight, hMid, hSide, hMono, spectrum, work;
};
} // namespace cruz::dsp
