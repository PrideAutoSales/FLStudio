#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace cs::ui
{
namespace colours
{
    const juce::Colour background { 0xff121118 };
    const juce::Colour panel      { 0xff1d1b26 };
    const juce::Colour panelHi    { 0xff262333 };
    const juce::Colour outline    { 0xff35314a };
    const juce::Colour text       { 0xffe8e4f0 };
    const juce::Colour textDim    { 0xff8d88a3 };
    const juce::Colour accent     { 0xffffa53d };   // amber
    const juce::Colour accent2    { 0xff4fd8c4 };   // teal
    const juce::Colour knobBody   { 0xff2c2938 };
    const juce::Colour track      { 0xff3a3650 };
    const juce::Colour display    { 0xff0d0c12 };
}

juce::Font font (float height, bool bold = false);

class CruzLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CruzLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override;
};

// A rounded panel with a title strip.
void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, bool active = true);
} // namespace cs::ui
