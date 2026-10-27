#include "Theme.h"

namespace cs::ui
{
juce::Font font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}

CruzLookAndFeel::CruzLookAndFeel()
{
    using namespace colours;
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::ComboBox::backgroundColourId, panelHi);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, panelHi);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panelHi);
    setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.3f));
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, accent);
    setColour (juce::Label::textColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, display);
    setColour (juce::BubbleComponent::backgroundColourId, panelHi);
    setColour (juce::BubbleComponent::outlineColourId, outline);
    setColour (juce::TooltipWindow::backgroundColourId, panelHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, outline);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, display);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, outline);
}

juce::Slider::SliderLayout CruzLookAndFeel::getSliderLayout (juce::Slider& s)
{
    auto layout = LookAndFeel_V4::getSliderLayout (s);
    if (s.getProperties().contains ("labelHeight"))
    {
        auto b = s.getLocalBounds();
        b.removeFromBottom ((int) s.getProperties()["labelHeight"]);
        layout.sliderBounds = b;
    }
    return layout;
}

void CruzLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float start, float end, juce::Slider& s)
{
    using namespace colours;
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (area.getWidth(), area.getHeight()) - 12.0f;
    const auto c = area.getCentre();
    const float r = size * 0.5f;
    const float angle = start + pos * (end - start);
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zeroAngle = bipolar ? start + (float) s.valueToProportionOfLength (0.0) * (end - start) : start;
    const auto col = accent;
    const bool enabled = s.isEnabled();

    // Track
    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
    g.setColour (track.isEmpty() ? juce::Colours::transparentBlack : colours::track);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r, r, 0.0f, juce::jmin (zeroAngle, angle), juce::jmax (zeroAngle, angle), true);
    g.setColour (enabled ? col : textDim);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Body
    const float br = r - 5.0f;
    g.setGradientFill (juce::ColourGradient (knobBody.brighter (0.25f), c.x, c.y - br, knobBody.darker (0.4f), c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (outline);
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.0f);

    // Pointer
    const auto tip = c.getPointOnCircumference (br - 2.0f, angle);
    const auto from = c.getPointOnCircumference (br * 0.35f, angle);
    g.setColour (enabled ? text : textDim);
    g.drawLine ({ from, tip }, 2.0f);
}

void CruzLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float,
                                        float, juce::Slider::SliderStyle style, juce::Slider& s)
{
    using namespace colours;
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 0, style, s);
        return;
    }
    const auto r = juce::Rectangle<float> ((float) x, (float) y + (float) h * 0.5f - 3.0f, (float) w, 6.0f);
    g.setColour (track);
    g.fillRoundedRectangle (r, 3.0f);

    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? (float) x + (float) s.valueToProportionOfLength (0.0) * (float) w : (float) x;
    const float px = juce::jlimit ((float) x, (float) (x + w), pos);
    auto fill = juce::Rectangle<float>::leftTopRightBottom (juce::jmin (zero, px), r.getY(), juce::jmax (zero, px), r.getBottom());
    g.setColour (accent);
    g.fillRoundedRectangle (fill, 3.0f);
    g.setColour (text);
    g.fillEllipse (px - 5.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
}

void CruzLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    const bool power = b.getProperties().contains ("power");

    if (power)
    {
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        auto c = r.withSizeKeepingCentre (d, d);
        g.setColour (on ? accent : track);
        g.fillEllipse (c.reduced (2.0f));
        if (on)
        {
            g.setColour (accent.withAlpha (0.3f));
            g.drawEllipse (c, 2.0f);
        }
        return;
    }

    g.setColour (on ? accent.withAlpha (0.22f) : panelHi.brighter (highlighted ? 0.08f : 0.0f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent : outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    g.setColour (on ? accent : textDim);
    g.setFont (font (11.5f, true));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
}

void CruzLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    using namespace colours;
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setColour ((on ? accent.withAlpha (0.22f) : panelHi).brighter (down ? 0.15f : (highlighted ? 0.07f : 0.0f)));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent : outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void CruzLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? colours::accent : colours::text);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1);
}

void CruzLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    using namespace colours;
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (true) ? accent.withAlpha (0.6f) : outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 12.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
    g.setColour (textDim);
    g.fillPath (arrow);
}

juce::Font CruzLookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (12.5f); }

void CruzLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 22, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font CruzLookAndFeel::getPopupMenuFont()                    { return font (14.0f); }
juce::Font CruzLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return font (12.0f, true); }
juce::Font CruzLookAndFeel::getLabelFont (juce::Label& l)          { return font (juce::jmin (13.0f, (float) l.getHeight() * 0.8f)); }

void CruzLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (colours::panelHi);
    g.setColour (colours::outline);
    g.drawRect (0, 0, w, h);
}

void CruzLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.setColour (colours::panelHi);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) w, (float) h, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (font (13.0f));
    g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 6);
}

juce::Rectangle<int> CruzLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (text, font (13.0f));
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 280.0f);
    const int w = (int) tl.getWidth() + 20, h = (int) tl.getHeight() + 12;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 16,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
        .constrainedWithin (parent);
}

void drawPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, bool active)
{
    using namespace colours;
    auto rf = r.toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (rf, 6.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (rf.reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (active ? text : textDim);
        g.setFont (font (12.5f, true));
        g.drawText (title, r.getX() + 30, r.getY() + 4, 200, 18, juce::Justification::centredLeft);
    }
}
} // namespace cs::ui
