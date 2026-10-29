#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

// A modal piano: every string partial is a decaying resonator.
//
// The Concert Grand is rebuilt from measurements of a real grand (GrandData):
// every overtone's own modes with their measured levels, decays, detuning and
// stereo phase, plus the recorded hammer attack. The other models compute their
// partials from a physical model of the hammer, strings and soundboard.
namespace cs::piano
{
struct GrandData;

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
    void reset() noexcept { count = 0; noiseEnv = 0.0f; silentRun = 0; residual = nullptr; }

    // Strikes the strings. pitch is a MIDI note number (may be fractional).
    void strike (float pitch, float velocity, const Settings& s);
    // Lets the dampers fall back on the strings (key released, pedal up).
    void release (const Settings& s);
    // The same key struck again: the hammer stops what this strike left ringing.
    void choke() noexcept;
    // Re-pitches every partial by a ratio (pitch bend).
    void retune (float semitoneOffset);

    // Adds the next n samples to l and r.
    void render (float* l, float* r, int n, float gain) noexcept;

    bool isSilent() const noexcept { return count == 0 && noiseEnv < 1.0e-5f && residual == nullptr; }

    // The partials of the current strike (frequency and level), for drawing.
    int snapshot (float* freqs, float* levels, int maxCount) const noexcept;

private:
    static constexpr int kMax = 1024;

    void add (float freq, float amp, float decayDbPerSec, float panL, float panR) noexcept;
    void strikeGrand (const GrandData& data, float pitch, float velocity, const Settings& s);
    void renderGrand (float* bufL, float* bufR, int n) noexcept;
    void addDecay (float dbPerSecAt0Hz, float dbPerSecPerKHz) noexcept;
    void padModes() noexcept;
    void refreshRotations() noexcept;
    void cull() noexcept;

    float sr = 48000.0f;
    int count = 0, stringCount = 0;
    // Resonator state and coefficients, structure-of-arrays for speed.
    alignas (16) float re[kMax] {}, im[kMax] {}, cr[kMax] {}, ci[kMax] {}, gl[kMax] {}, gr[kMax] {};
    float freq[kMax] {}, radius[kMax] {};

    // Measured grand: each mode has a complex gain per channel (gl/gr real parts,
    // gli/gri imaginary), the modes swell in along a measured attack curve, and
    // the recorded hammer attack plays alongside.
    bool measured = false;
    alignas (16) float gli[kMax] {}, gri[kMax] {};
    const float* attackCurve = nullptr;
    const float* residual = nullptr;
    int residualLength = 0;
    double residualPos = 0.0, residualStep = 1.0, attackPos = 0.0;
    float strikeLevel = 0.0f;
    float attackEnd = 0.0f, residualLowGain = 1.0f, residualHighGain = 1.0f;
    float residualSplit = 0.1f, residualLowL = 0.0f, residualLowR = 0.0f;
    const void* overtoneTable = nullptr; // GrandData::Overtone, for snapshot()
    int numOvertones = 0;
    float overtoneRatio = 1.0f, overtoneGain = 1.0f;

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
