#pragma once

#include "PluginProcessor.h"
#include "ui/BandPanel.h"
#include "ui/EditorRedrawGuard.h"
#include "ui/EqDisplay.h"

class CruzEQAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit CruzEQAudioProcessorEditor (CruzEQAudioProcessor&);
    ~CruzEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void timerCallback() override;

    CruzEQAudioProcessor& processor;

    juce::LookAndFeel_V4 lookAndFeel;
    EqDisplay display;
    BandPanel bandPanel;
    juce::ComboBox rangeBox;
    juce::Label rangeLabel;

    juce::ToggleButton autoGainButton { "Auto gain" };
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAttachment;

    juce::ComboBox phaseModeBox, resolutionBox;
    juce::Label latencyLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> phaseModeAttachment, resolutionAttachment;

    juce::Slider outputGainSlider, gainScaleSlider;
    juce::Label outputGainLabel, gainScaleLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment outputGainAttachment, gainScaleAttachment;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessorEditor)
};
