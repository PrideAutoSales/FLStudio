#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

// A modal piano: every string partial is a decaying resonator whose frequency,
// level and decay come from a model of the hammer, the strings and the
// soundboard. The curves were tuned against measurements of a recorded concert grand.
namespace cs::piano
{
juce::StringArray modelNames();
juce::String modelDescription (int model);
enum Model { ConcertGrand, BrightGrand, Upright, Felt, TineEP, ReedEP, NumModels };

struct Settings
{
    int model = ConcertGrand;
    float tone = 0.0f;      // -1..1 hammer hardness
    float dynamics = 0.6f;  // 0..1 how much velocity changes level
    float sustain = 0.5f;   // 0..1 string decay time
    float damper = 0.5f;    // 0..1 how long notes ring after release
    float strings = 0.3f;   // 0..1 unison detune between a note's strings
    float body = 0.5f;      // 0..1 soundboard colour and knock
    float hammer = 0.5f;    // 0..1 hammer and action noise
    float width = 0.7f;     // 0..1 stereo spread across the keyboard
};

class Voice
{
public:
    void prepare (double sampleRate);
    void reset() noexcept { count = 0; noiseEnv = 0.0f; silentRun = 0; }

    // Strikes the strings. pitch is a MIDI note number (may be fractional).
    void strike (float pitch, float velocity, const Settings& s);
    // Lets the dampers fall back on the strings (key released, pedal up).
    void release (const Settings& s);
    // Re-pitches every partial by a ratio (pitch bend).
    void retune (float semitoneOffset);

    // Adds the next n samples to l and r.
    void render (float* l, float* r, int n, float gain) noexcept;

    bool isSilent() const noexcept { return count == 0 && noiseEnv < 1.0e-5f; }

    // The partials of the current strike (frequency and level), for drawing.
    int snapshot (float* freqs, float* levels, int maxCount) const noexcept;

private:
    static constexpr int kMax = 256;

    void add (float freq, float amp, float decayDbPerSec, float panL, float panR) noexcept;
    void refreshRotations() noexcept;
    void cull() noexcept;

    float sr = 48000.0f;
    int count = 0, stringCount = 0;
    // Resonator state and coefficients, structure-of-arrays for speed.
    alignas (16) float re[kMax] {}, im[kMax] {}, cr[kMax] {}, ci[kMax] {}, gl[kMax] {}, gr[kMax] {};
    float freq[kMax] {}, radius[kMax] {};

    // Electric piano pickup: the partials sum to mono and pass through a waveshaper.
    bool electric = false;
    float dcState = 0.0f, pickupIn = 1.0f, pickupDrive = 0.0f, pickupAsym = 0.0f, pickupL = 0.7f, pickupR = 0.7f, pickupNorm = 1.0f;

    // Hammer knock: decaying noise through a band-pass.
    juce::uint32 seed = 12345u;
    float noiseEnv = 0.0f, noiseDecay = 0.0f, noiseL = 0.0f, noiseR = 0.0f;
    float bpG = 0.1f, bpK = 1.0f, bpS1 = 0.0f, bpS2 = 0.0f, lpC = 0.5f, noiseLpState = 0.0f, noiseLpState2 = 0.0f;

    float bendSemis = 0.0f, note = 60.0f;
    int cullCounter = 0, silentRun = 0;
};
} // namespace cs::piano
