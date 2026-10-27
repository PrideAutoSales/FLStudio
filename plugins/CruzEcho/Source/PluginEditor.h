#pragma once

#include "PluginProcessor.h"

namespace cruzui
{
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };

    // A knob with its name above it and its value below.
    class Knob final : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& title);
        void resized() override;

        juce::Slider slider;

    private:
        juce::Label title;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // A row of buttons that picks one value of a choice parameter.
    class ChoiceButtons final : public juce::Component
    {
    public:
        ChoiceButtons (juce::AudioProcessorValueTreeState&, const juce::String& paramID);
        void resized() override;

    private:
        juce::OwnedArray<juce::TextButton> buttons;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    // Draws where the repeats land and how fast they fade.
    class EchoDisplay final : public juce::Component, private juce::Timer
    {
    public:
        EchoDisplay (CruzEchoAudioProcessor&);
        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;
        float param (const char* id) const;

        CruzEchoAudioProcessor& processor;
        float glow = 0.0f;
    };
}

class CruzEchoAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CruzEchoAudioProcessorEditor (CruzEchoAudioProcessor&);
    ~CruzEchoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void updateTimeKnob();

    CruzEchoAudioProcessor& processor;
    cruzui::LookAndFeel lookAndFeel;

    cruzui::ChoiceButtons styleButtons, modeButtons;
    cruzui::EchoDisplay display;

    juce::ToggleButton syncButton { "Sync to tempo" };
    juce::AudioProcessorValueTreeState::ButtonAttachment syncAttachment;

    cruzui::Knob timeNote, timeMs, feel, groove, feedback, mix, output;
    cruzui::Knob saturation, wobble, lowCut, highCut, width, duck;

    juce::ParameterAttachment syncWatcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzEchoAudioProcessorEditor)
};
