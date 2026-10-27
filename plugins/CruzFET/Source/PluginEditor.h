#pragma once

#include "PluginProcessor.h"
#include "CruzCompUI.h"
#include "EditorRedrawGuard.h"

class CruzFETAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CruzFETAudioProcessorEditor (CruzFETAudioProcessor&);
    ~CruzFETAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void timerCallback() override;

    CruzFETAudioProcessor& processor;
    cruz::Palette palette;
    cruz::KnobLookAndFeel lookAndFeel;
    cruz::NeedleMeter meter;
    cruz::LabelledKnob inputKnob, outputKnob, attackKnob, releaseKnob, mixKnob;
    cruz::ChoiceButtons ratioButtons, meterButtons;
    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzFETAudioProcessorEditor)
};
