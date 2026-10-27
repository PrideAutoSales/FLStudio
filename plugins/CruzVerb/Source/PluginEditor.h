#pragma once

#include "EditorRedrawGuard.h"
#include "PluginProcessor.h"
#include "ReverbDisplay.h"
#include "Theme.h"

class CruzVerbAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CruzVerbAudioProcessorEditor (CruzVerbAudioProcessor&);
    ~CruzVerbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setUpKnob (Knob&, const char* paramId, const juce::String& name, const juce::String& suffix,
                    juce::Colour accent);
    void refreshPresetBox();
    void setDisplayMode (ReverbDisplay::Mode);

    CruzVerbAudioProcessor& processor;
    CruzVerbLookAndFeel lookAndFeel;

    ReverbDisplay display;
    juce::TextButton decayModeButton { "DECAY EQ" }, postModeButton { "POST EQ" };
    juce::TextButton freezeButton { "FREEZE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;
    juce::ComboBox presetBox;

    std::array<Knob, 10> knobs;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzVerbAudioProcessorEditor)
};
