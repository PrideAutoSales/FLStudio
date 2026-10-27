#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <memory>
#include <vector>

namespace cs
{
// A wavetable is a stack of single-cycle frames. Each frame is stored at
// several band-limited copies (one per octave) so high notes don't alias.
class Wavetable
{
public:
    static constexpr int kSize = 2048;
    static constexpr int kLevels = 11;     // level L keeps harmonics up to 1024 >> L
    static constexpr int kMaxFrames = 256;

    // frames holds numFrames * kSize samples, one frame after another.
    Wavetable (juce::String name, std::vector<float> frames, int factoryIndex = -1);

    const juce::String& getName() const noexcept    { return name; }
    int getFactoryIndex() const noexcept            { return factoryIndex; }
    int getNumFrames() const noexcept               { return numFrames; }
    const std::vector<float>& getRawFrames() const  { return raw; }
    const float* getRawFrame (int f) const noexcept { return raw.data() + (size_t) f * kSize; }

    const float* getMip (int frame, int level) const noexcept
    {
        return mips.data() + ((size_t) frame * kLevels + (size_t) level) * (kSize + 1);
    }

    // phaseInc is cycles per sample. Picks the copy whose top harmonic stays under Nyquist.
    static int levelForIncrement (double phaseInc) noexcept
    {
        if (phaseInc <= 0.0)
            return 0;
        const double allowed = 0.5 / phaseInc;
        if (allowed >= 1024.0)
            return 0;
        const int l = (int) std::ceil (std::log2 (1024.0 / juce::jmax (1.0, allowed)));
        return juce::jlimit (0, kLevels - 1, l);
    }

    static float readCycle (const float* t, double phase) noexcept
    {
        const double x = phase * kSize;
        const int i = juce::jlimit (0, kSize - 1, (int) x);
        const float f = (float) (x - i);
        return t[i] + f * (t[i + 1] - t[i]);
    }

    // framePos runs 0..numFrames-1 and blends neighbouring frames.
    float read (float framePos, int level, double phase) const noexcept
    {
        int f0 = (int) framePos;
        float ff = framePos - (float) f0;
        if (f0 >= numFrames - 1) { f0 = numFrames - 1; ff = 0.0f; }
        const float a = readCycle (getMip (f0, level), phase);
        if (ff <= 0.0f)
            return a;
        const float b = readCycle (getMip (f0 + 1, level), phase);
        return a + ff * (b - a);
    }

private:
    void buildMips();

    juce::String name;
    int factoryIndex = -1;
    int numFrames = 1;
    std::vector<float> raw;
    std::vector<float> mips;
};

using WavetablePtr = std::shared_ptr<const Wavetable>;

//==============================================================================
namespace tables
{
    juce::StringArray factoryNames();
    std::vector<float> generateFactory (int index);
    WavetablePtr makeFactory (int index);

    // Spectrum helpers used by the editor's harmonic view.
    std::vector<float> harmonicMagnitudes (const float* frame, int numHarmonics);
    void setHarmonicMagnitudes (float* frame, const std::vector<float>& mags);

    // Reads a WAV/AIFF file and slices it into 2048-sample frames.
    // Returns an empty vector and fills error when the file can't be used.
    std::vector<float> loadFromFile (const juce::File& file, juce::String& error);
    bool saveToWav (const Wavetable& table, const juce::File& file);

    void normalise (std::vector<float>& frames);
    void removeDc (float* frame);
}
} // namespace cs
