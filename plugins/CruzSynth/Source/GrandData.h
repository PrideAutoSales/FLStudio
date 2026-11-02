#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Measurements of a real concert grand, used by the piano engine to rebuild it.
//
// Every recorded note (30 keys, a minor third apart, each struck at 16
// strengths) was taken apart into its string modes: each overtone is a few
// slightly detuned, decaying sine waves whose sum gives that overtone's real
// beating and two-stage decay, with its own level and phase in each stereo
// channel. What the modes cannot describe (the hammer's knock and the first
// few milliseconds of the strike) is kept as a short attack residual.
// tools/piano/ has the analysis and the packer that writes the data file.
//
// Source recordings: Salamander Grand Piano V3 by Alexander Holm (Yamaha C5),
// CC BY 3.0, https://github.com/sfzinstruments/SalamanderGrandPiano
//
// The Bass Guitar is measured the same way from a fingered 5-string bass: all 42
// notes from its low B (30.9 Hz) up, at 4 strengths with 4 takes each, plus the
// sound of the finger damping each string (tools/bass/). Source recordings:
// Black And Blue Basses by Karoryfer Samples, public domain (CC0),
// https://github.com/sfzinstruments/karoryfer.black-and-blue-basses
namespace cs::piano
{
struct GrandData
{
    struct Mode
    {
        float centre;  // the overtone this mode belongs to, Hz
        float offset;  // this mode's detune from the overtone, Hz
        float decay;   // amplitude decay rate, 1/s
        float lRe, lIm, rRe, rIm; // complex amplitude in each channel at the strike
    };

    struct Overtone { float freq, level; }; // for drawing

    struct Record
    {
        float f0 = 0.0f;        // fundamental of the recorded string, Hz
        float loudness = 0.0f;  // dB, RMS of the first 300 ms of the recording
        float level = 0.0f;     // dB the key should play at: evened across the keyboard
        float attackTime = 0.0f; // seconds the attack table and residual cover
        float attack[24] {};    // gain of the modes over attackTime (they swell in)
        int firstMode = 0, numModes = 0;
        int firstResidual = 0, residualLength = 0; // frames of `channels` samples
        float residualScale = 0.0f;                // residual sample value of 1
        int firstOvertone = 0, numOvertones = 0;
    };

    // A recording of the string being damped (the bass's finger release).
    struct Release
    {
        int first = 0, length = 0; // frames of `channels` samples in releaseSamples
        float scale = 0.0f;        // sample value of 1
        float level = 0.0f;        // dB, RMS of its first 60 ms
    };

    static constexpr int kGainPoints = 24;

    int numKeys = 0, numLayers = 0, numTakes = 1, channels = 2;
    float sampleRate = 48000.0f;
    std::vector<int> keyNotes;          // MIDI note of each recorded key
    std::vector<float> keyF0;           // average fundamental of each key's recordings, Hz
    std::vector<Record> records;        // key-major: records[(key * numLayers + layer) * numTakes + take]
    std::vector<Mode> modes;
    std::vector<juce::int16> residual;  // interleaved by channel
    std::vector<Overtone> overtones;
    std::vector<Release> releases;      // releases[key * numTakes + take], if recorded
    std::vector<juce::int16> releaseSamples;

    const Record& record (int key, int layer, int take = 0) const noexcept
    {
        return records[(size_t) ((key * numLayers + layer) * numTakes + take)];
    }

    // The nearest recorded key to a MIDI note.
    int nearestKey (float note) const noexcept;
    // The level (dB) a key plays at for a fractional strike strength 0..numLayers-1.
    float levelAt (int key, float layer) const noexcept; // layer-major levels (the same for every take)

    // Decoded once and shared by every voice. Returns nullptr if the data is missing.
    static const GrandData* get();   // the concert grand
    static const GrandData* bass();  // the bass guitar
};
} // namespace cs::piano
