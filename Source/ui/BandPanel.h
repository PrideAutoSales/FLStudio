#pragma once

#include "../PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Controls for the selected band: shape, slope, stereo placement, frequency,
// gain, Q, bypass, and the dynamic EQ settings.
class BandPanel final : public juce::Component,
                        private juce::Timer
{
public:
    explicit BandPanel (CruzEQAudioProcessor&);

    void setBand (int index);
    std::function<void()> onRemove;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateEnablement();
    std::vector<juce::Component*> bandControls();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    CruzEQAudioProcessor& processor;
    int band = -1;

    juce::Label title, hint;
    juce::ComboBox shapeBox, slopeBox, placeBox;
    juce::Slider freqSlider, gainSlider, qSlider, rangeSlider, thresholdSlider, attackSlider, releaseSlider;
    juce::Label shapeLabel, slopeLabel, placeLabel, freqLabel, gainLabel, qLabel, rangeLabel, thresholdLabel, attackLabel, releaseLabel;
    juce::ToggleButton bypassButton { "Bypass" }, dynamicButton { "Dynamic" };
    juce::TextButton removeButton { "Remove" };

    std::unique_ptr<ComboBoxAttachment> shapeAttachment, slopeAttachment, placeAttachment;
    std::unique_ptr<SliderAttachment> freqAttachment, gainAttachment, qAttachment, rangeAttachment, thresholdAttachment, attackAttachment, releaseAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment, dynamicAttachment;

    std::vector<std::pair<juce::Label*, juce::Slider*>> knobs();
    juce::Rectangle<int> dynamicArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
};
