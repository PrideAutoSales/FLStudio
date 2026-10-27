#pragma once

// Linear-phase polyphase oversampler with a whole-sample round-trip latency.
//
// The anti-imaging and anti-aliasing filter is flat to about 0.84 of the host
// Nyquist frequency (20 kHz at 48 kHz) and reaches about -80 dB at Nyquist, so
// nothing above Nyquist folds back into the audio band. Keeping that top band
// clean also makes the true-peak level of the result well defined.

#include <algorithm>
#include <cmath>
#include <vector>

// Lets clang vectorise dot products (it may then reorder the float additions).
#if defined (__clang__)
 #define CRUZ_VECTORISE_LOOP _Pragma ("clang loop vectorize(enable) interleave(enable)")
#else
 #define CRUZ_VECTORISE_LOOP
#endif

namespace cruz
{

inline double besselI0 (double x) noexcept
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 48; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
    }
    return sum;
}

/** Kaiser window value at position r in [-1, 1]. */
inline double kaiserWindow (double r, double beta) noexcept
{
    return besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
}

class Oversampler
{
public:
    static constexpr int maxChannels = 2;
    /** Filter length per phase, in host-rate samples. Also the round-trip latency. */
    static constexpr int tapsPerPhase = 64;

    void prepare (int newFactor, int maxBlockSize)
    {
        factor = std::max (1, newFactor);
        maxBlock = std::max (1, maxBlockSize);

        if (factor > 1)
            design();

        for (int c = 0; c < maxChannels; ++c)
        {
            upInput[c].assign ((size_t) (tapsPerPhase + maxBlock), 0.0f);
            downInput[c].assign ((size_t) (factor * tapsPerPhase + factor * maxBlock), 0.0f);
            upOutput[c].assign ((size_t) (factor * maxBlock), 0.0f);
        }
    }

    void reset() noexcept
    {
        for (int c = 0; c < maxChannels; ++c)
        {
            std::fill (upInput[c].begin(), upInput[c].end(), 0.0f);
            std::fill (downInput[c].begin(), downInput[c].end(), 0.0f);
        }
    }

    int getFactor() const noexcept  { return factor; }
    int getLatency() const noexcept { return factor > 1 ? tapsPerPhase : 0; }

    /** Upsamples numSamples host-rate samples; the result is available from getChannel(). */
    void upsample (const float* const* input, int numChannels, int numSamples) noexcept
    {
        for (int c = 0; c < numChannels; ++c)
        {
            auto* out = upOutput[c].data();

            if (factor == 1)
            {
                std::copy (input[c], input[c] + numSamples, out);
                continue;
            }

            auto* ext = upInput[c].data();
            std::copy (input[c], input[c] + numSamples, ext + tapsPerPhase);

            for (int n = 0; n < numSamples; ++n)
            {
                // newest input sample is ext[tapsPerPhase + n]; phase taps run newest-first
                const auto* x = ext + n + 1;
                for (int p = 0; p < factor; ++p)
                {
                    const auto* h = upPhases.data() + (size_t) p * (tapsPerPhase + 1);
                    float acc = 0.0f;
                    CRUZ_VECTORISE_LOOP
                    for (int m = 0; m <= tapsPerPhase; ++m)
                        acc += h[m] * x[tapsPerPhase - 1 - m];
                    out[n * factor + p] = acc;
                }
            }

            std::copy (ext + numSamples, ext + numSamples + tapsPerPhase, ext);
        }
    }

    float* getChannel (int channel) noexcept { return upOutput[channel].data(); }

    /** Filters and decimates the oversampled channels back into output. */
    void downsample (float* const* output, int numChannels, int numSamples) noexcept
    {
        for (int c = 0; c < numChannels; ++c)
        {
            const auto* in = upOutput[c].data();

            if (factor == 1)
            {
                std::copy (in, in + numSamples, output[c]);
                continue;
            }

            const auto history = factor * tapsPerPhase;
            auto* ext = downInput[c].data();
            std::copy (in, in + numSamples * factor, ext + history);

            for (int n = 0; n < numSamples; ++n)
            {
                // y[F n - k] for k = 0..history, read oldest-first against the reversed filter
                const auto* y = ext + n * factor;
                float acc = 0.0f;
                CRUZ_VECTORISE_LOOP
                for (int k = 0; k <= history; ++k)
                    acc += downFilter[(size_t) k] * y[k];
                output[c][n] = acc;
            }

            std::copy (ext + numSamples * factor, ext + numSamples * factor + history, ext);
        }
    }

private:
    void design()
    {
        // Odd, symmetric filter of factor * taps + 1 points: its delay is exactly
        // taps / 2 host samples each way, so up + down is a whole number of samples.
        constexpr double pi = 3.14159265358979323846;
        constexpr double beta = 8.0;
        const auto length = factor * tapsPerPhase + 1;
        const auto centre = (length - 1) * 0.5;
        const auto cutoff = 0.92 * 0.5 / factor; // cycles per oversampled sample

        std::vector<double> h ((size_t) length);
        double sum = 0.0;
        for (int k = 0; k < length; ++k)
        {
            const auto t = k - centre;
            const auto sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (2.0 * pi * cutoff * t) / (pi * t);
            h[(size_t) k] = (std::abs (t) < 1.0e-9 ? 2.0 * cutoff : sinc) * kaiserWindow (t / centre, beta);
            sum += h[(size_t) k];
        }
        for (auto& v : h)
            v /= sum;

        // Upsampling phases: y[F n + p] = F * sum_m h[F m + p] x[n - m]
        upPhases.assign ((size_t) (factor * (tapsPerPhase + 1)), 0.0f);
        for (int p = 0; p < factor; ++p)
            for (int m = 0; m <= tapsPerPhase; ++m)
            {
                const auto k = m * factor + p;
                if (k < length)
                    upPhases[(size_t) (p * (tapsPerPhase + 1) + m)] = (float) (h[(size_t) k] * factor);
            }

        // Decimation filter stored reversed so the inner loop walks forwards in time.
        downFilter.assign ((size_t) length, 0.0f);
        for (int k = 0; k < length; ++k)
            downFilter[(size_t) k] = (float) h[(size_t) (length - 1 - k)];
    }

    int factor = 1, maxBlock = 1;
    std::vector<float> upPhases, downFilter;
    std::vector<float> upInput[maxChannels], downInput[maxChannels], upOutput[maxChannels];
};

} // namespace cruz
