#pragma once

// Runs the natural phase and linear phase kernels: a uniformly partitioned
// FFT convolution with a fixed latency of one block (128 samples).
//
// The input history is kept in the frequency domain and shared by every
// kernel, so new kernels can be swapped in at any time. During a swap the
// output is worked out with both the old and the new kernels and crossfaded,
// which keeps changes click-free even for long linear phase kernels.

#include "PhaseModes.h"

#include <vector>

namespace cruz::dsp
{
// Fixed-size complex FFT on separate real and imaginary arrays.
class SmallFft
{
public:
    explicit SmallFft (int size)
        : n (size), bitReverse ((size_t) size), cosTable ((size_t) size / 2), sinTable ((size_t) size / 2)
    {
        int bits = 0;
        while ((1 << bits) < n)
            ++bits;

        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < bits; ++b)
                r |= ((i >> b) & 1) << (bits - 1 - b);
            bitReverse[(size_t) i] = r;
        }

        for (int k = 0; k < n / 2; ++k)
        {
            cosTable[(size_t) k] = std::cos (2.0 * pi * k / n);
            sinTable[(size_t) k] = -std::sin (2.0 * pi * k / n);
        }
    }

    void forward (double* re, double* im) const noexcept { run (re, im, false); }

    // Inverse transform, scaled by 1/size.
    void inverse (double* re, double* im) const noexcept
    {
        run (re, im, true);
        const double scale = 1.0 / n;
        for (int i = 0; i < n; ++i)
        {
            re[i] *= scale;
            im[i] *= scale;
        }
    }

private:
    void run (double* re, double* im, bool inverse) const noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const int j = bitReverse[(size_t) i];
            if (i < j)
            {
                std::swap (re[i], re[j]);
                std::swap (im[i], im[j]);
            }
        }

        const double sign = inverse ? -1.0 : 1.0;

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;

            for (int i = 0; i < n; i += len)
            {
                for (int k = 0; k < half; ++k)
                {
                    const double wr = cosTable[(size_t) (k * step)], wi = sign * sinTable[(size_t) (k * step)];
                    const int a = i + k, b = a + half;
                    const double vr = re[b] * wr - im[b] * wi;
                    const double vi = re[b] * wi + im[b] * wr;
                    re[b] = re[a] - vr;
                    im[b] = im[a] - vi;
                    re[a] += vr;
                    im[a] += vi;
                }
            }
        }
    }

    int n;
    std::vector<int> bitReverse;
    std::vector<double> cosTable, sinTable;
};

// The kernels cut into partitions and transformed, ready for the convolver.
// Kernel 0 is left to left (or the mono kernel), 1 right to right, 2 right to
// left and 3 left to right.
struct KernelSpectra
{
    static constexpr int numKernels = 4;

    int numPartitions = 0;
    bool hasCrossTerms = false;
    std::array<std::vector<double>, numKernels> re, im; // [partition * bins + bin]

    void allocate (int maxPartitions, int bins)
    {
        for (int k = 0; k < numKernels; ++k)
        {
            re[(size_t) k].assign ((size_t) (maxPartitions * bins), 0.0);
            im[(size_t) k].assign ((size_t) (maxPartitions * bins), 0.0);
        }
    }

    int capacity (int bins) const noexcept { return (int) re[0].size() / bins; }
};

class Convolver
{
public:
    static constexpr int blockSize = 128;
    static constexpr int fftSize = 2 * blockSize;
    static constexpr int bins = blockSize + 1;

    Convolver() : fft (fftSize) {}

    static int partitionsFor (int kernelLength) noexcept { return (kernelLength + blockSize - 1) / blockSize; }

    // Cuts the kernels into partitions and transforms them. Not real-time safe.
    static void makeSpectra (const EqKernels& k, bool mono, KernelSpectra& out)
    {
        const SmallFft fft (fftSize);
        const int partitions = partitionsFor (k.length);

        if (out.capacity (bins) < partitions)
            out.allocate (partitions, bins);

        out.numPartitions = partitions;
        out.hasCrossTerms = k.hasCrossTerms && ! mono;

        const std::vector<float>* sources[] { mono ? &k.mono : &k.leftToLeft, &k.rightToRight, &k.rightToLeft, &k.leftToRight };
        std::vector<double> re ((size_t) fftSize), im ((size_t) fftSize);

        for (int kernel = 0; kernel < KernelSpectra::numKernels; ++kernel)
        {
            const auto& taps = *sources[kernel];

            for (int p = 0; p < partitions; ++p)
            {
                std::fill (re.begin(), re.end(), 0.0);
                std::fill (im.begin(), im.end(), 0.0);

                for (int i = 0; i < blockSize; ++i)
                {
                    const size_t t = (size_t) (p * blockSize + i);
                    re[(size_t) i] = t < taps.size() ? (double) taps[t] : 0.0;
                }

                fft.forward (re.data(), im.data());

                for (int b = 0; b < bins; ++b)
                {
                    out.re[(size_t) kernel][(size_t) (p * bins + b)] = re[(size_t) b];
                    out.im[(size_t) kernel][(size_t) (p * bins + b)] = im[(size_t) b];
                }
            }
        }
    }

    // Allocates the input history for kernels up to maxPartitions long.
    void prepare (int maxPartitions, int crossfadeSamples)
    {
        historyLength = std::max (1, maxPartitions);
        fadeLength = std::max (blockSize, crossfadeSamples);

        for (int ch = 0; ch < 2; ++ch)
        {
            historyRe[(size_t) ch].assign ((size_t) (historyLength * bins), 0.0);
            historyIm[(size_t) ch].assign ((size_t) (historyLength * bins), 0.0);
            input[(size_t) ch].assign ((size_t) fftSize, 0.0);
            output[(size_t) ch].assign ((size_t) blockSize, 0.0);
            fadeOutput[(size_t) ch].assign ((size_t) blockSize, 0.0);
        }

        for (auto* v : { &workRe, &workIm, &accLeftRe, &accLeftIm, &accRightRe, &accRightIm })
            v->assign ((size_t) fftSize, 0.0);

        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (historyRe[(size_t) ch].begin(), historyRe[(size_t) ch].end(), 0.0);
            std::fill (historyIm[(size_t) ch].begin(), historyIm[(size_t) ch].end(), 0.0);
            std::fill (input[(size_t) ch].begin(), input[(size_t) ch].end(), 0.0);
            std::fill (output[(size_t) ch].begin(), output[(size_t) ch].end(), 0.0);
        }

        head = 0;
        position = 0;
        previous = nullptr;
    }

    // Starts using new kernels. The old ones keep running until the crossfade
    // ends; call only when isFading() is false. The kernels must stay alive
    // while they are current or previous.
    void setKernels (const KernelSpectra* kernels) noexcept
    {
        if (current != nullptr && kernels != nullptr)
        {
            previous = current;
            fadePosition = 0;
        }

        current = kernels;
    }

    bool isFading() const noexcept { return previous != nullptr; }
    const KernelSpectra* currentKernels() const noexcept { return current; }
    const KernelSpectra* previousKernels() const noexcept { return previous; }

    // Filters one or two channels in place, delayed by blockSize samples.
    void process (float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = std::min (numChannels, 2);

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                input[(size_t) ch][(size_t) (blockSize + position)] = ch < numChannels ? channels[ch][i] : 0.0f;

                if (ch < numChannels)
                    channels[ch][i] = (float) output[(size_t) ch][(size_t) position];
            }

            if (++position == blockSize)
            {
                processBlock();
                position = 0;
            }
        }
    }

private:
    void processBlock() noexcept
    {
        // Transform the newest two blocks of input, both channels at once:
        // left in the real part, right in the imaginary part.
        for (int i = 0; i < fftSize; ++i)
        {
            workRe[(size_t) i] = input[0][(size_t) i];
            workIm[(size_t) i] = input[1][(size_t) i];
        }

        fft.forward (workRe.data(), workIm.data());
        head = (head + 1) % historyLength;

        for (int b = 0; b < bins; ++b)
        {
            const int mirror = (fftSize - b) % fftSize;
            const double xr = workRe[(size_t) b], xi = workIm[(size_t) b];
            const double mr = workRe[(size_t) mirror], mi = workIm[(size_t) mirror];
            const size_t at = (size_t) (head * bins + b);
            historyRe[0][at] = 0.5 * (xr + mr);
            historyIm[0][at] = 0.5 * (xi - mi);
            historyRe[1][at] = 0.5 * (xi + mi);
            historyIm[1][at] = 0.5 * (mr - xr);
        }

        for (int ch = 0; ch < 2; ++ch)
            std::copy (input[(size_t) ch].begin() + blockSize, input[(size_t) ch].end(), input[(size_t) ch].begin());

        if (current == nullptr)
        {
            // No kernels yet: pass the sound through with the same delay.
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < blockSize; ++i)
                    output[(size_t) ch][(size_t) i] = input[(size_t) ch][(size_t) i];
            return;
        }

        filter (*current, output);

        if (previous != nullptr)
        {
            filter (*previous, fadeOutput);

            for (int i = 0; i < blockSize; ++i)
            {
                const double t = std::min (1.0, (fadePosition + i + 1) / (double) fadeLength);
                for (int ch = 0; ch < 2; ++ch)
                    output[(size_t) ch][(size_t) i] = t * output[(size_t) ch][(size_t) i] + (1.0 - t) * fadeOutput[(size_t) ch][(size_t) i];
            }

            fadePosition += blockSize;
            if (fadePosition >= fadeLength)
                previous = nullptr;
        }
    }

    // Multiplies the input history by the kernels' partitions and transforms back.
    void filter (const KernelSpectra& k, std::array<std::vector<double>, 2>& out) noexcept
    {
        std::fill (accLeftRe.begin(), accLeftRe.begin() + bins, 0.0);
        std::fill (accLeftIm.begin(), accLeftIm.begin() + bins, 0.0);
        std::fill (accRightRe.begin(), accRightRe.begin() + bins, 0.0);
        std::fill (accRightIm.begin(), accRightIm.begin() + bins, 0.0);

        const int partitions = std::min (k.numPartitions, historyLength);

        const auto accumulate = [&] (int kernel, int fromChannel, double* accRe, double* accIm, int p, int slot)
        {
            const double* kr = k.re[(size_t) kernel].data() + p * bins;
            const double* ki = k.im[(size_t) kernel].data() + p * bins;
            const double* hr = historyRe[(size_t) fromChannel].data() + slot * bins;
            const double* hi = historyIm[(size_t) fromChannel].data() + slot * bins;

            for (int b = 0; b < bins; ++b)
            {
                accRe[b] += hr[b] * kr[b] - hi[b] * ki[b];
                accIm[b] += hr[b] * ki[b] + hi[b] * kr[b];
            }
        };

        for (int p = 0; p < partitions; ++p)
        {
            const int slot = (head - p + historyLength) % historyLength;
            accumulate (0, 0, accLeftRe.data(), accLeftIm.data(), p, slot);
            accumulate (1, 1, accRightRe.data(), accRightIm.data(), p, slot);

            if (k.hasCrossTerms)
            {
                accumulate (2, 1, accLeftRe.data(), accLeftIm.data(), p, slot);
                accumulate (3, 0, accRightRe.data(), accRightIm.data(), p, slot);
            }
        }

        // Pack left + j * right into one spectrum and transform back.
        for (int b = 0; b < fftSize; ++b)
        {
            const bool upper = b > blockSize;
            const int src = upper ? fftSize - b : b;
            const double lr = accLeftRe[(size_t) src], li = upper ? -accLeftIm[(size_t) src] : accLeftIm[(size_t) src];
            const double rr = accRightRe[(size_t) src], ri = upper ? -accRightIm[(size_t) src] : accRightIm[(size_t) src];
            workRe[(size_t) b] = lr - ri;
            workIm[(size_t) b] = li + rr;
        }

        fft.inverse (workRe.data(), workIm.data());

        for (int i = 0; i < blockSize; ++i)
        {
            out[0][(size_t) i] = workRe[(size_t) (blockSize + i)];
            out[1][(size_t) i] = workIm[(size_t) (blockSize + i)];
        }
    }

    SmallFft fft;
    int historyLength = 1, head = 0, position = 0, fadeLength = 2048, fadePosition = 0;
    std::array<std::vector<double>, 2> historyRe, historyIm, input, output, fadeOutput;
    std::vector<double> workRe, workIm, accLeftRe, accLeftIm, accRightRe, accRightIm;
    const KernelSpectra* current = nullptr;
    const KernelSpectra* previous = nullptr;
};
} // namespace cruz::dsp
