#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>

// Hands a mono mix of the audio from the audio thread to the analyzer on the
// UI thread without locks. If the UI falls behind, new audio is dropped.
class AnalyzerFifo
{
public:
    void push (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numChannels = buffer.getNumChannels();
        if (numChannels == 0)
            return;

        const float scale = 1.0f / (float) numChannels;
        const auto scope = fifo.write (buffer.getNumSamples());
        mixInto (buffer, 0, scope.startIndex1, scope.blockSize1, scale);
        mixInto (buffer, scope.blockSize1, scope.startIndex2, scope.blockSize2, scale);
    }

    int pull (float* dest, int maxSamples) noexcept
    {
        const auto scope = fifo.read (std::min (maxSamples, fifo.getNumReady()));
        std::copy_n (data.begin() + scope.startIndex1, scope.blockSize1, dest);
        std::copy_n (data.begin() + scope.startIndex2, scope.blockSize2, dest + scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    void mixInto (const juce::AudioBuffer<float>& buffer, int sourceStart, int destStart, int count, float scale) noexcept
    {
        for (int i = 0; i < count; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                sum += buffer.getSample (ch, sourceStart + i);

            data[(size_t) (destStart + i)] = sum * scale;
        }
    }

    static constexpr int capacity = 1 << 15;
    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> data {};
};
