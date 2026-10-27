#pragma once

#include "../AnalyzerFifo.h"

#include <juce_dsp/juce_dsp.h>

// Turns the audio arriving from the processor into a magnitude spectrum,
// using the most recent 8192 samples each time new audio has arrived.
class SpectrumAnalyzer
{
public:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;

    SpectrumAnalyzer() { binsDb.fill (-140.0f); }

    // Returns true when a new spectrum was computed.
    bool update (AnalyzerFifo& fifo)
    {
        int received = 0;

        for (;;)
        {
            const int n = fifo.pull (incoming.data(), (int) incoming.size());
            if (n == 0)
                break;

            for (int i = 0; i < n; ++i)
            {
                history[(size_t) writePos] = incoming[(size_t) i];
                writePos = (writePos + 1) % fftSize;
            }

            received += n;
        }

        if (received == 0)
            return false;

        for (int i = 0; i < fftSize; ++i)
            fftData[(size_t) i] = history[(size_t) ((writePos + i) % fftSize)];

        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        // A full-scale sine reads 0 dB: Hann window gain is 0.5, FFT gain N/2.
        const float norm = 4.0f / (float) fftSize;
        for (int i = 0; i <= fftSize / 2; ++i)
            binsDb[(size_t) i] = juce::Decibels::gainToDecibels (fftData[(size_t) i] * norm, -140.0f);

        return true;
    }

    // Level in dB over a frequency span: the loudest bin inside it, or an
    // interpolated value when the span falls between two bins.
    float levelDb (double fromHz, double toHz, double sampleRate) const noexcept
    {
        const double binWidth = sampleRate / fftSize;
        const double a = fromHz / binWidth, b = toHz / binWidth;
        const int last = fftSize / 2;

        if (a >= last)
            return -140.0f;

        if (b - a < 1.0)
        {
            const double centre = 0.5 * (a + b);
            const int i = juce::jlimit (0, last - 1, (int) centre);
            const float t = (float) (centre - i);
            return binsDb[(size_t) i] + t * (binsDb[(size_t) i + 1] - binsDb[(size_t) i]);
        }

        float level = -140.0f;
        for (int i = juce::jmax (0, (int) std::ceil (a)); i <= juce::jmin (last, (int) b); ++i)
            level = juce::jmax (level, binsDb[(size_t) i]);

        return level;
    }

private:
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::array<float, 4096> incoming {};
    std::array<float, fftSize> history {};
    std::array<float, 2 * fftSize> fftData {};
    std::array<float, fftSize / 2 + 1> binsDb {};
    int writePos = 0;
};
