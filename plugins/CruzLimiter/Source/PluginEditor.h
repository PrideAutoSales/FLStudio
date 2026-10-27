#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

namespace LimiterColours
{
    const juce::Colour background { 0xff111418 };
    const juce::Colour panel { 0xff191d23 };
    const juce::Colour panelEdge { 0xff262c34 };
    const juce::Colour grid { 0xff2a3039 };
    const juce::Colour text { 0xffe6e8eb };
    const juce::Colour dimText { 0xff8a9099 };
    const juce::Colour accent { 0xff4fc3a1 };
    const juce::Colour input { 0xff3b4553 };
    const juce::Colour output { 0xff7fb8d6 };
    const juce::Colour reduction { 0xffff6b4a };
    const juce::Colour ceiling { 0xffffc857 };
}

/** Knobs, buttons and menus in the Cruz look. */
class LimiterLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LimiterLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (13.0f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (14.0f); }
};

/** Scrolling history of input level, output level and gain reduction. */
class LevelDisplay final : public juce::Component
{
public:
    explicit LevelDisplay (juce::AudioProcessorValueTreeState& state);

    void addFrames (const MeterFrame* frames, int count);
    void paint (juce::Graphics&) override;

private:
    float yForDb (float db, juce::Rectangle<float> area) const noexcept;

    juce::AudioProcessorValueTreeState& state;
    std::vector<MeterFrame> history;
    int writePos = 0;
    static constexpr float topDb = 3.0f, bottomDb = -36.0f;
};

/** Output meters, gain reduction meter and loudness readouts. */
class MeterPanel final : public juce::Component
{
public:
    explicit MeterPanel (CruzLimiterAudioProcessor&);

    void update();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void drawBar (juce::Graphics&, juce::Rectangle<float> bar, float db, float holdDb, juce::Colour) const;

    CruzLimiterAudioProcessor& processor;
    juce::TextButton resetButton { "Reset" };
    std::array<float, 2> levelDb { -100.0f, -100.0f }, holdDb { -100.0f, -100.0f };
    std::array<int, 2> holdAge {};
    float reductionDb = 0.0f;
};

class CruzLimiterAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit CruzLimiterAudioProcessorEditor (CruzLimiterAudioProcessor&);
    ~CruzLimiterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    struct Menu
    {
        juce::ComboBox box;
        juce::Label label;
        std::unique_ptr<ComboAttachment> attachment;
    };

    void timerCallback() override;
    void setUpKnob (Knob&, const char* paramId, const juce::String& name, const juce::String& tip);
    void setUpMenu (Menu&, const char* paramId, const juce::String& name, const juce::String& tip);
    void setUpToggle (juce::ToggleButton&, std::unique_ptr<ButtonAttachment>&, const char* paramId,
                      const juce::String& name, const juce::String& tip);

    CruzLimiterAudioProcessor& processor;
    LimiterLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    LevelDisplay display;
    MeterPanel meters;

    juce::Slider gainSlider;
    std::unique_ptr<SliderAttachment> gainAttachment;

    Knob lookahead, attack, release, linkTransients, linkRelease, ceiling;
    Menu style, oversampling, dither;
    juce::ToggleButton truePeak, noiseShaping, dcFilter, unityGain, audition;
    std::unique_ptr<ButtonAttachment> truePeakAttachment, noiseShapingAttachment, dcFilterAttachment,
                                      unityGainAttachment, auditionAttachment;

    std::vector<MeterFrame> frameScratch;
    int shownLatency = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzLimiterAudioProcessorEditor)
};
