#pragma once

#include "EditorRedrawGuard.h"
#include "PluginProcessor.h"

class TapeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    TapeLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOver, bool isButtonDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;
};

// A row of buttons that picks one value of a choice parameter.
class Selector final : public juce::Component
{
public:
    Selector (juce::RangedAudioParameter& parameter, const juce::String& title);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void update (float value);

    juce::String title;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

class VuMeter final : public juce::Component
{
public:
    explicit VuMeter (juce::String name) : label (std::move (name)) {}

    void paint (juce::Graphics&) override;

    // Feeds a new RMS level, dt seconds after the last one.
    void push (float rmsLinear, float dt);

private:
    juce::String label;
    float vu = -40.0f;
};

class Reels final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void advance (float revolutionsPerSecond, float dt);

private:
    void drawReel (juce::Graphics&, juce::Point<float> centre, float radius, float fill, float angle) const;
    float angle = 0.0f;
};

class CruzTapeAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit CruzTapeAudioProcessorEditor (CruzTapeAudioProcessor&);
    ~CruzTapeAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (Knob&, const juce::String& paramId, const juce::String& name, const juce::String& suffix);
    void timerCallback() override;

    CruzTapeAudioProcessor& processor;
    TapeLookAndFeel lookAndFeel;

    Reels reels;
    VuMeter leftMeter { "L" }, rightMeter { "R" };

    Selector speed, formula, calibration, emphasis;
    juce::ToggleButton autoGain { "Auto Gain" };
    juce::AudioProcessorValueTreeState::ButtonAttachment autoGainAttachment;

    Knob input, bias, wow, flutter, hiss, mix, output;

    juce::uint32 lastTimerTime = 0;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzTapeAudioProcessorEditor)
};
