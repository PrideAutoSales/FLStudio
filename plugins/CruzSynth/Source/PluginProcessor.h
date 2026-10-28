#pragma once

#include "Effects.h"
#include "Engine.h"
#include <juce_audio_utils/juce_audio_utils.h>

class CruzSynthProcessor : public juce::AudioProcessor
{
public:
    CruzSynthProcessor();
    ~CruzSynthProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Wavetables (message thread)
    cs::WavetablePtr getTable (int osc) const { return uiTables[(size_t) osc]; }
    void setTable (int osc, cs::WavetablePtr table);

    // Custom LFO curves: "x,y,curve;x,y,curve;..." (message thread)
    juce::String getLfoPoints (int lfo) const { return lfoPoints[(size_t) lfo]; }
    void setLfoPoints (int lfo, const juce::String& points);
    static juce::String defaultLfoPoints();

    // Effects order
    std::array<int, cs::kNumFx> getFxOrder() const { return fx.getOrder(); }
    void setFxOrder (const std::array<int, cs::kNumFx>& order) { fx.setOrder (order); sendChangeMessageToEditor(); }

    // Modulation matrix helpers used by drag-and-drop
    int addModulation (int source, int destination, float amount = 0.5f);
    void clearModulationSlot (int slot);
    juce::Array<int> slotsForDestination (int destination) const;

    // Presets
    juce::StringArray getFactoryPresetNames() const;          // index 0 is Init
    juce::String getFactoryPresetCategory (int index) const;  // "" for Init
    void loadFactoryPreset (int index);
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);
    juce::String getPresetName() const { return presetName; }
    static juce::File getUserPresetFolder();

    // Output scope (written by the audio thread)
    static constexpr int kScopeSize = 4096;
    void copyScope (float* dest, int num) const;
    float getOutputPeak() const { return outputPeak.load(); }

    juce::AudioProcessorValueTreeState apvts;
    cs::LiveState live;
    cs::LfoTables lfoTables;
    juce::MidiKeyboardState keyboardState;

    // Lets the editor know when tables, curves or the FX order changed.
    juce::ChangeBroadcaster stateChanged;

private:
    void sendChangeMessageToEditor() { stateChanged.sendChangeMessage(); }
    void resetParametersToDefault();
    void setParam (const juce::String& pid, float value);
    void applyRecipe (const juce::String& recipe);
    void renderLfoTable (int lfo);

    cs::Engine engine;
    cs::FxRack fx;

    std::array<cs::WavetablePtr, cs::kNumOscs> uiTables, audioTables, pendingTables;
    std::array<bool, cs::kNumOscs> pendingFlags {};
    juce::SpinLock tableLock;

    std::array<juce::String, cs::kNumLfos> lfoPoints;

    std::array<float, kScopeSize> scope {};
    std::atomic<int> scopeWrite { 0 };
    std::atomic<float> outputPeak { 0.0f };

    int currentProgram = 0;
    juce::String presetName { "Init" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzSynthProcessor)
};
