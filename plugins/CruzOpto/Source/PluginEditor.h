#pragma once

#include "PluginProcessor.h"
#include "CruzCompUI.h"
#include "EditorRedrawGuard.h"

class CruzOptoAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CruzOptoAudioProcessorEditor (CruzOptoAudioProcessor&);
    ~CruzOptoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void timerCallback() override;

    CruzOptoAudioProcessor& processor;
    cruz::Palette palette;
    cruz::KnobLookAndFeel lookAndFeel;
    cruz::NeedleMeter meter;
    cruz::LabelledKnob gainKnob, peakKnob, emphasisKnob, mixKnob;
    cruz::ChoiceButtons modeButtons, meterButtons;
    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzOptoAudioProcessorEditor)
};
