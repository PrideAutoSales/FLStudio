#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "BuiltInSynth.h"
#include "ChordEngine.h"
#include "Presets.h"

// Cruz Chords: pick a key, build or load a chord progression, and hear it
// through the built-in sound or any synth loaded inside the plugin. It also
// sends the notes out as MIDI and can export the progression as a MIDI file.
class CruzChordsAudioProcessor final : public juce::AudioProcessor, public juce::ChangeBroadcaster
{
public:
    CruzChordsAudioProcessor();
    ~CruzChordsAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParams() noexcept { return params; }

    //==============================================================================
    // Everything the window edits. Only touch it on the message thread, then
    // call modelChanged().
    struct Model
    {
        int key = 9;                       // A
        int scale = cruzchords::sMinor;
        int size = cruzchords::Sevenths;   // chord size for the pads
        int rhythm = cruzchords::rSustain;
        int spread = cruzchords::Close;
        bool smooth = true;
        bool bass = false;
        bool keysPlayChords = true;
        bool followHost = true;            // play whenever FL Studio plays
        int octave = 0;                    // moves the whole progression, -3..3
        int invert = 0;                    // inverts every chord up or down, -4..4
        int feel = cruzchords::FeelAny;    // shuffle: the mood it writes in
        float adventure = 0.3f;            // shuffle: 0 traditional .. 1 adventurous
        int shuffleBars = 4;               // shuffle length, 1..32 bars
        double chordEvery = 4.0;           // shuffle: beats per chord
        float velocity = 0.8f;
        int sound = 0;                     // built-in sound: 0 keys, 1 pad
        int presetIndex = -1;
        std::vector<cruzchords::Chord> chords;
    };

    Model& model() noexcept { return m; }
    void modelChanged();
    void loadPreset (int index);
    void shuffle();

    // Notes of the chords as they will play (after voicing), for the window.
    std::vector<int> slotNotes (int slot) const;
    std::vector<int> padNotes (int degree) const;
    cruzchords::Chord padChord (int degree) const;
    bool useFlats() const { return cruzchords::keyUsesFlats (m.key, m.scale); }

    // Plays notes from the window (pads and progression slots).
    void audition (const std::vector<int>& notes, bool on);

    std::atomic<bool> playing { false };
    int getPlayingSlot() const noexcept { return playingSlot.load(); }
    double getHostBpm() const noexcept { return hostBpm.load(); }
    bool isHostPlaying() const noexcept { return hostPlaying.load(); }
    bool isSounding() const noexcept { return progressionSounding.load(); }

    //==============================================================================
    // The synth that plays the chords.
    static juce::Array<juce::File> findPluginFiles();
    void loadPluginFile (const juce::File&);
    void useBuiltInSound (int sound);
    bool hasHostedSynth() const;
    juce::String getSoundName() const;
    juce::String getLastLoadError() const { return lastLoadError; }
    void showSynthWindow();
    void closeSynthWindow();

    // MIDI file of the progression (one pass, with the chosen rhythm).
    bool writeMidiFile (const juce::File&) const;
    juce::String suggestedMidiName() const;

private:
    struct Live
    {
        cruzchords::Sequence sequence;
        int keyOffsets[12][cruzchords::Sequence::maxNotes + 1] {};
        int keyCounts[12] {};
        bool keysPlayChords = true;
        float velocity = 0.8f;
    };

    struct AuditionEvent
    {
        int note;
        bool on;
    };

    void rebuildLive();
    cruzchords::RenderOptions renderOptions() const;
    void addNote (juce::MidiBuffer&, int sample, int note, float velocity, bool on);
    void allNotesOff (juce::MidiBuffer&, int sample);
    void installSynth (std::unique_ptr<juce::AudioPluginInstance>, const juce::MemoryBlock& state);
    void removeSynth();
    void loadPluginDescription (const juce::PluginDescription&, const juce::MemoryBlock& state);
    juce::ValueTree modelToTree() const;
    void applyState (const juce::ValueTree&);

    juce::AudioProcessorValueTreeState params;
    std::atomic<float>* volume = nullptr;

    Model m;

    // Audio-thread data, swapped in under liveLock.
    std::unique_ptr<Live> live, spareLive;
    juce::SpinLock liveLock;

    juce::AbstractFifo auditionFifo { 512 };
    std::array<AuditionEvent, 512> auditionEvents {};

    cruzchords::Player player;
    bool wasPlaying = false, wasHostPlaying = false;
    double freePpq = 0, expectedPpq = -1;
    bool sounding[128] {};
    int heldKeyNotes[128][cruzchords::Sequence::maxNotes + 1] {};
    int heldKeyCounts[128] {};
    juce::MidiBuffer generated;

    std::atomic<int> playingSlot { -1 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<bool> hostPlaying { false };
    std::atomic<bool> progressionSounding { false };
    std::atomic<bool> followHost { true };
    std::atomic<bool> panicRequested { false };

    // Built-in sound.
    std::atomic<int> builtInChoice { 0 };
    juce::Synthesiser builtIn;

    // Hosted synth. Swapped on the message thread; the audio thread uses it under hostLock.
    juce::AudioPluginFormatManager formatManager;
    std::unique_ptr<juce::AudioPluginInstance> hosted;
    juce::PluginDescription hostedDescription;
    std::atomic<bool> hasHosted { false };
    juce::SpinLock hostLock;
    juce::CriticalSection hostLifetime;
    juce::AudioBuffer<float> hostBuffer;
    juce::MidiBuffer hostMidi;
    juce::String lastLoadError;
    std::unique_ptr<juce::DocumentWindow> synthWindow;

    float lastGain = 1.0f;
    double currentRate = 44100.0;
    int currentBlock = 512;

    juce::CriticalSection savedStateLock;
    juce::String savedModelXml;

    std::shared_ptr<bool> alive = std::make_shared<bool> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzChordsAudioProcessor)
};
