#pragma once

#include "PluginProcessor.h"
#include "CruzCompUI.h"

class CruzFETAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CruzFETAudioProcessorEditor (CruzFETAudioProcessor&);
    ~CruzFETAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CruzFETAudioProcessor& processor;
    cruz::Palette palette;
    cruz::KnobLookAndFeel lookAndFeel;
    cruz::NeedleMeter meter;
    cruz::LabelledKnob inputKnob, outputKnob, attackKnob, releaseKnob, mixKnob;
    cruz::ChoiceButtons ratioButtons, meterButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzFETAudioProcessorEditor)
};
