#pragma once

#include "PluginProcessor.h"
#include "CruzCompUI.h"

class CruzOptoAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CruzOptoAudioProcessorEditor (CruzOptoAudioProcessor&);
    ~CruzOptoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    CruzOptoAudioProcessor& processor;
    cruz::Palette palette;
    cruz::KnobLookAndFeel lookAndFeel;
    cruz::NeedleMeter meter;
    cruz::LabelledKnob gainKnob, peakKnob, emphasisKnob, mixKnob;
    cruz::ChoiceButtons modeButtons, meterButtons;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzOptoAudioProcessorEditor)
};
