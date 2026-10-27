#pragma once

#include "PluginProcessor.h"
#include "ui/BandPanel.h"
#include "ui/EqDisplay.h"

class CruzEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CruzEQAudioProcessorEditor (CruzEQAudioProcessor&);
    ~CruzEQAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    CruzEQAudioProcessor& processor;

    juce::LookAndFeel_V4 lookAndFeel;
    EqDisplay display;
    BandPanel bandPanel;
    juce::ComboBox rangeBox;
    juce::Label rangeLabel;

    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessorEditor)
};
