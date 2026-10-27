#pragma once

// Loudness metering after ITU-R BS.1770 / EBU R128: momentary (400 ms),
// short-term (3 s), gated integrated loudness and loudness range.
// Runs on the audio thread without allocating; results are read through atomics.

#include <array>
#include <atomic>
#include <vector>

namespace cruz
{

class LoudnessMeter
{
public:
    static constexpr float silence = -100.0f;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Feed output audio. Call requestReset() from any thread to clear the readings. */
    void process (const float* const* channels, int numChannels, int numSamples);
    void requestReset() noexcept { resetRequested.store (true); }

    float getMomentary() const noexcept  { return momentary.load(); }
    float getShortTerm() const noexcept  { return shortTerm.load(); }
    float getIntegrated() const noexcept { return integrated.load(); }
    float getRange() const noexcept      { return range.load(); }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        double process (double x) noexcept
        {
            const auto y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    static constexpr int maxChannels = 2;
    static constexpr int numBins = 800;        // 0.1 LU bins from -70 to +10 LUFS
    static constexpr int shortTermBlocks = 30; // 30 x 100 ms

    void finishSubBlock();
    void updateIntegrated();
    void updateRange();

    static int binFor (double lufs) noexcept;
    static double binCentre (int bin) noexcept { return -70.0 + (bin + 0.5) * 0.1; }

    std::array<Biquad, maxChannels> shelf, highpass;
    int channels = 2, hopLength = 4800, hopCount = 0;
    double hopEnergy = 0.0;

    std::array<double, shortTermBlocks> subBlocks {};
    int subBlockPos = 0, subBlocksFilled = 0;

    std::array<double, numBins> gatedEnergy {};
    std::array<int, numBins> gatedCount {};
    std::array<int, numBins> shortTermCount {};

    std::atomic<float> momentary { silence }, shortTerm { silence }, integrated { silence }, range { 0.0f };
    std::atomic<bool> resetRequested { false };
};

} // namespace cruz
