#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Theme
{
    const juce::Colour background { 0xff0f1216 };
    const juce::Colour panel { 0xff161a20 };
    const juce::Colour panelEdge { 0xff232932 };
    const juce::Colour grid { 0xff222831 };
    const juce::Colour text { 0xffe8e6e1 };
    const juce::Colour dimText { 0xff858c96 };
    const juce::Colour decay { 0xffffb04f };   // warm amber for everything about the tail
    const juce::Colour postEq { 0xff62d0c6 };  // cool teal for the tone curve
    const juce::Colour freeze { 0xff8fb8ff };
}

class CruzVerbLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CruzVerbLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, Theme::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Label::textColourId, Theme::dimText);
        setColour (juce::ComboBox::backgroundColourId, Theme::panel);
        setColour (juce::ComboBox::outlineColourId, Theme::panelEdge);
        setColour (juce::ComboBox::textColourId, Theme::text);
        setColour (juce::ComboBox::arrowColourId, Theme::dimText);
        setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
        setColour (juce::PopupMenu::textColourId, Theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::decay.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId, Theme::text);
        setColour (juce::TextButton::buttonColourId, Theme::panel);
        setColour (juce::TextButton::buttonOnColourId, Theme::panelEdge);
        setColour (juce::TextButton::textColourOffId, Theme::dimText);
        setColour (juce::TextButton::textColourOnId, Theme::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);
        const auto radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle = startAngle + pos * (endAngle - startAngle);
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const auto thickness = std::max (2.5f, radius * 0.14f);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, endAngle, true);
        g.setColour (Theme::panelEdge);
        g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - thickness, radius - thickness, 0.0f, startAngle, angle, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const auto capRadius = radius - thickness * 2.4f;
        g.setColour (Theme::panel.brighter (0.08f));
        g.fillEllipse (juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre));

        const auto tip = centre.getPointOnCircumference (capRadius * 0.8f, angle);
        const auto base = centre.getPointOnCircumference (capRadius * 0.25f, angle);
        g.setColour (Theme::text);
        g.drawLine ({ base, tip }, 2.0f);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (13.0f, juce::Font::bold); }
};
