#pragma once

#include "PluginProcessor.h"

namespace patina::ui
{
class PatinaLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PatinaLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// A rotary knob with its name above and its value below.
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name,
          juce::Colour accent, bool large = false);

    void resized() override;

private:
    juce::Slider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    bool isLarge;
};

class Choice final : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& name,
            const juce::StringArray& items);

    void resized() override;

private:
    juce::ComboBox box;
    juce::Label label;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment attachment;
};

// One module: a title with a power switch, then a 2 x 2 grid of controls.
class ModulePanel final : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState&, const juce::String& title, const juce::String& powerId,
                 juce::Colour accent);

    void addKnob (const juce::String& paramId, const juce::String& name);
    void addChoice (const juce::String& paramId, const juce::String& name, const juce::StringArray& items);
    void refreshPower();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String title;
    juce::Colour accent;
    juce::ToggleButton power;
    juce::AudioProcessorValueTreeState::ButtonAttachment powerAttachment;
    std::vector<std::unique_ptr<juce::Component>> controls;
    bool lastPower = true;
};
} // namespace patina::ui

class CruzPatinaAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit CruzPatinaAudioProcessorEditor (CruzPatinaAudioProcessor&);
    ~CruzPatinaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 940;
    static constexpr int baseHeight = 560;

private:
    class Content final : public juce::Component
    {
    public:
        explicit Content (CruzPatinaAudioProcessor&);
        void paint (juce::Graphics&) override;
        void resized() override;
        void refresh();

    private:
        CruzPatinaAudioProcessor& processor;
        std::vector<std::unique_ptr<patina::ui::ModulePanel>> modules;
        std::unique_ptr<patina::ui::Knob> magnitude;
        std::vector<std::unique_ptr<patina::ui::Knob>> globals;
        juce::ComboBox presets;
        int shownProgram = -1;
    };

    void timerCallback() override;

    patina::ui::PatinaLookAndFeel lookAndFeel;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzPatinaAudioProcessorEditor)
};
