#pragma once

#include "PluginProcessor.h"

class CruzEQAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CruzEQAudioProcessorEditor (CruzEQAudioProcessor&);
    ~CruzEQAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEQAudioProcessorEditor)
};
