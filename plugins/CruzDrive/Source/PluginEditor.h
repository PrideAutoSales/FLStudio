#pragma once

#include "PluginProcessor.h"
#include "EditorRedrawGuard.h"

namespace DriveUI
{
    // Rotary knob with its caption above and value readout below.
    struct Knob : juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
              const juce::String& caption, juce::Colour accent, bool large = false);

        void resized() override;

        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
        bool isLarge;
    };

    // Pill-shaped on/off switch bound to a bool parameter.
    struct Switch : juce::TextButton
    {
        Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                const juce::String& caption, juce::Colour accent);

        juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
    };

    // Level bar, vertical or horizontal. Reduction meters fill from the top/right.
    struct Meter : juce::Component
    {
        Meter (juce::String caption, bool horizontal, bool isReduction);

        void setLevel (float newValue);
        void paint (juce::Graphics&) override;

        juce::String caption;
        bool horizontal, reduction;
        float value = 0.0f, shown = 0.0f;
    };

    // Draws the selected style's transfer curve at the current drive, with a
    // dot that rides the curve at the incoming level.
    struct CurveDisplay : juce::Component
    {
        explicit CurveDisplay (CruzDriveProcessor&);

        void refresh();
        void paint (juce::Graphics&) override;

        CruzDriveProcessor& processor;
        float level = 0.0f;
    };
}

class CruzDriveEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit CruzDriveEditor (CruzDriveProcessor&);
    ~CruzDriveEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void timerCallback() override;
    void refreshPresetBox();

    CruzDriveProcessor& processor;
    std::unique_ptr<juce::LookAndFeel> lookAndFeel;

    juce::ComboBox presetBox;

    DriveUI::CurveDisplay curve;
    juce::OwnedArray<juce::TextButton> styleButtons;
    std::unique_ptr<juce::ParameterAttachment> styleAttachment;

    DriveUI::Knob drive, lowCut, tone, highCut, crush, darkness, mix, output;
    DriveUI::Switch punish, thump, steep, autoGain;
    DriveUI::Meter inMeter, outMeter, crushMeter;

    // Panel rectangles, set in resized() and drawn in paint().
    juce::Rectangle<int> characterPanel, drivePanel, crushPanel, outputPanel;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzDriveEditor)
};
