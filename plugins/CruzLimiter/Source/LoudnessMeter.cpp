#include "LoudnessMeter.h"

#include <algorithm>
#include <cmath>

namespace cruz
{

namespace
{
    constexpr double pi = 3.14159265358979323846;

    double energyToLufs (double energy) noexcept
    {
        return energy > 1.0e-20 ? -0.691 + 10.0 * std::log10 (energy) : (double) LoudnessMeter::silence;
    }

    double lufsToEnergy (double lufs) noexcept
    {
        return std::pow (10.0, (lufs + 0.691) / 10.0);
    }
}

void LoudnessMeter::prepare (double sampleRate, int numChannels)
{
    channels = std::clamp (numChannels, 1, maxChannels);
    hopLength = std::max (1, (int) std::lround (sampleRate * 0.1));

    // K-weighting stage 1: high shelf (+4 dB above ~1.7 kHz).
    {
        const double f0 = 1681.974450955533, gainDb = 3.999843853973347, q = 0.7071752369554196;
        const double k = std::tan (pi * f0 / sampleRate);
        const double vh = std::pow (10.0, gainDb / 20.0);
        const double vb = std::pow (vh, 0.4996667741545416);
        const double a0 = 1.0 + k / q + k * k;

        for (auto& f : shelf)
        {
            f.b0 = (vh + vb * k / q + k * k) / a0;
            f.b1 = 2.0 * (k * k - vh) / a0;
            f.b2 = (vh - vb * k / q + k * k) / a0;
            f.a1 = 2.0 * (k * k - 1.0) / a0;
            f.a2 = (1.0 - k / q + k * k) / a0;
        }
    }

    // K-weighting stage 2: high pass (~38 Hz).
    {
        const double f0 = 38.13547087602444, q = 0.5003270373238773;
        const double k = std::tan (pi * f0 / sampleRate);
        const double a0 = 1.0 + k / q + k * k;

        for (auto& f : highpass)
        {
            f.b0 = 1.0;
            f.b1 = -2.0;
            f.b2 = 1.0;
            f.a1 = 2.0 * (k * k - 1.0) / a0;
            f.a2 = (1.0 - k / q + k * k) / a0;
        }
    }

    reset();
}

void LoudnessMeter::reset()
{
    for (auto& f : shelf)    f.z1 = f.z2 = 0.0;
    for (auto& f : highpass) f.z1 = f.z2 = 0.0;

    hopCount = 0;
    hopEnergy = 0.0;
    subBlocks.fill (0.0);
    subBlockPos = subBlocksFilled = 0;
    gatedEnergy.fill (0.0);
    gatedCount.fill (0);
    shortTermCount.fill (0);

    momentary = shortTerm = integrated = silence;
    range = 0.0f;
}

int LoudnessMeter::binFor (double lufs) noexcept
{
    return std::clamp ((int) std::floor ((lufs + 70.0) * 10.0), 0, numBins - 1);
}

void LoudnessMeter::process (const float* const* data, int numChannels, int numSamples)
{
    if (resetRequested.exchange (false))
        reset();

    numChannels = std::min (numChannels, channels);

    for (int i = 0; i < numSamples; ++i)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            const auto y = highpass[(size_t) c].process (shelf[(size_t) c].process (data[c][i]));
            hopEnergy += y * y;
        }

        if (++hopCount == hopLength)
            finishSubBlock();
    }
}

void LoudnessMeter::finishSubBlock()
{
    subBlocks[(size_t) subBlockPos] = hopEnergy / hopLength;
    subBlockPos = (subBlockPos + 1) % shortTermBlocks;
    subBlocksFilled = std::min (subBlocksFilled + 1, shortTermBlocks);
    hopEnergy = 0.0;
    hopCount = 0;

    auto meanOfLast = [this] (int count)
    {
        double sum = 0.0;
        for (int k = 1; k <= count; ++k)
            sum += subBlocks[(size_t) ((subBlockPos - k + shortTermBlocks) % shortTermBlocks)];
        return sum / count;
    };

    if (subBlocksFilled >= 4)
    {
        const auto energy = meanOfLast (4);
        const auto lufs = energyToLufs (energy);
        momentary = (float) lufs;

        if (lufs > -70.0)
        {
            const auto bin = binFor (lufs);
            gatedEnergy[(size_t) bin] += energy;
            ++gatedCount[(size_t) bin];
            updateIntegrated();
        }
    }

    if (subBlocksFilled >= shortTermBlocks)
    {
        const auto lufs = energyToLufs (meanOfLast (shortTermBlocks));
        shortTerm = (float) lufs;

        if (lufs > -70.0)
        {
            ++shortTermCount[(size_t) binFor (lufs)];
            updateRange();
        }
    }
}

void LoudnessMeter::updateIntegrated()
{
    double energy = 0.0;
    int count = 0;
    for (int b = 0; b < numBins; ++b)
    {
        energy += gatedEnergy[(size_t) b];
        count += gatedCount[(size_t) b];
    }

    if (count == 0)
        return;

    const auto relativeGate = energyToLufs (energy / count) - 10.0;

    energy = 0.0;
    count = 0;
    for (int b = 0; b < numBins; ++b)
    {
        if (binCentre (b) >= relativeGate)
        {
            energy += gatedEnergy[(size_t) b];
            count += gatedCount[(size_t) b];
        }
    }

    if (count > 0)
        integrated = (float) energyToLufs (energy / count);
}

void LoudnessMeter::updateRange()
{
    double energy = 0.0;
    int count = 0;
    for (int b = 0; b < numBins; ++b)
    {
        energy += shortTermCount[(size_t) b] * lufsToEnergy (binCentre (b));
        count += shortTermCount[(size_t) b];
    }

    if (count == 0)
        return;

    const auto relativeGate = energyToLufs (energy / count) - 20.0;

    int total = 0;
    for (int b = 0; b < numBins; ++b)
        if (binCentre (b) >= relativeGate)
            total += shortTermCount[(size_t) b];

    if (total < 2)
    {
        range = 0.0f;
        return;
    }

    auto percentile = [&] (double fraction)
    {
        const auto target = fraction * (total - 1);
        int seen = 0;
        for (int b = 0; b < numBins; ++b)
        {
            if (binCentre (b) < relativeGate)
                continue;
            seen += shortTermCount[(size_t) b];
            if (seen > target)
                return binCentre (b);
        }
        return binCentre (numBins - 1);
    };

    range = (float) std::max (0.0, percentile (0.95) - percentile (0.10));
}

} // namespace cruz
