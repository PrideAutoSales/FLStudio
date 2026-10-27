#pragma once

#include "DSP.h"
#include "Parameters.h"
#include "Wavetable.h"
#include <array>
#include <atomic>

namespace cs
{
// Values the editor reads to draw live feedback. Written by the audio thread.
struct LiveState
{
    std::array<std::atomic<float>, DstCount> dstNorm {};      // modulated knob positions (0..1)
    std::array<std::atomic<float>, kNumEnvs> envValue {};
    std::array<std::atomic<int>, kNumEnvs> envStage {};
    std::array<std::atomic<float>, kNumLfos> lfoPhase {};
    std::array<std::atomic<float>, kNumLfos> lfoValue {};
    std::array<std::atomic<float>, kNumOscs> wtPos {};         // modulated table position (0..1)
    std::atomic<int> activeVoices { 0 };
    std::atomic<bool> anyVoice { false };
};

// Custom LFO curves, drawn in the editor. Values are 0..1.
using LfoTables = std::array<std::array<std::atomic<float>, kLfoTableSize>, kNumLfos>;

struct ModSlot { int src = 0, dst = 0; float amount = 0.0f; bool bipolar = false; };

//==============================================================================
class Engine
{
public:
    Engine (juce::AudioProcessorValueTreeState& state, LiveState& live, LfoTables& lfoTables);
    ~Engine();

    void prepare (double sampleRate, int maxBlock);
    void reset();

    void setTables (const Wavetable* a, const Wavetable* b) noexcept { tables[0] = a; tables[1] = b; }

    struct Transport { double bpm = 120.0; double ppq = 0.0; bool playing = false; };

    // Renders voices into out (stereo), handling the MIDI in the block.
    void render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Transport& transport);

    void allNotesOff();

    struct Block;   // per-block parameter snapshot (defined in Engine.cpp)
    class Voice;

private:
    void readParameters (const Transport& transport);
    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    Voice* findVoiceFor (int note);
    void publishLiveState();

    juce::AudioProcessorValueTreeState& apvts;
    LiveState& live;
    LfoTables& lfoTables;

    std::unique_ptr<Block> block;
    std::vector<std::unique_ptr<Voice>> voices;
    const Wavetable* tables[kNumOscs] { nullptr, nullptr };

    double sampleRate = 48000.0;
    juce::uint32 noteCounter = 0;
    float pitchBend = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    bool sustainDown = false;
    float lastNote = -1.0f;
    juce::Array<int> monoStack;
    std::array<double, kNumLfos> freeLfoPhase {};
    juce::Random rng;
};
} // namespace cs
