#pragma once

#include "PluginProcessor.h"
#include "ui/BandPanel.h"
#include "ui/EqDisplay.h"

class CruzEQAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit CruzEQAudioProcessorEditor (CruzEQAudioProcessor&);
    ~CruzEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

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

    juce::Slider outputGainSlider, gainScaleSlider;
    juce::Label outputGainLabel, gainScaleLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment outputGainAttachment, gainScaleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessorEditor)
};
