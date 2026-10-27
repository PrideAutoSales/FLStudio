#pragma once

// UI pieces shared by Cruz Opto and Cruz FET: a needle meter, knob drawing,
// and a row of buttons bound to a choice parameter.

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace cruz
{
struct Palette
{
    juce::Colour panel, panelEdge, text, accent, knobBody, knobCap, meterFace, meterInk;
};

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit KnobLookAndFeel (const Palette& p) : palette (p)
    {
        setColour (juce::Slider::textBoxTextColourId, p.text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonColourId, p.panel.darker (0.35f));
        setColour (juce::TextButton::buttonOnColourId, p.accent);
        setColour (juce::TextButton::textColourOffId, p.text.withAlpha (0.75f));
        setColour (juce::TextButton::textColourOnId, p.panel.darker (0.8f));
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);

        // Scale ticks around the knob.
        g.setColour (palette.text.withAlpha (0.55f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (float) i / 10.0f * (endAngle - startAngle);
            const auto p1 = centre.getPointOnCircumference (radius * 0.93f, a);
            const auto p2 = centre.getPointOnCircumference (radius * (i % 5 == 0 ? 0.78f : 0.84f), a);
            g.drawLine ({ p1, p2 }, i % 5 == 0 ? 1.6f : 1.0f);
        }

        // Body with a soft shadow and a skirt.
        const float bodyR = radius * 0.72f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre.translated (0.0f, 2.5f)));

        juce::ColourGradient body (palette.knobBody.brighter (0.25f), centre.x, centre.y - bodyR,
                                   palette.knobBody.darker (0.45f), centre.x, centre.y + bodyR, false);
        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (bodyR * 2.0f, bodyR * 2.0f).withCentre (centre));

        const float capR = bodyR * 0.68f;
        juce::ColourGradient cap (palette.knobCap.brighter (0.3f), centre.x - capR, centre.y - capR,
                                  palette.knobCap.darker (0.3f), centre.x + capR, centre.y + capR, false);
        g.setGradientFill (cap);
        g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre));

        // Pointer line.
        g.setColour (palette.accent);
        const auto tip = centre.getPointOnCircumference (bodyR * 0.92f, angle);
        const auto base = centre.getPointOnCircumference (bodyR * 0.25f, angle);
        g.drawLine ({ base, tip }, juce::jmax (2.0f, radius * 0.06f));
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return juce::Font (juce::FontOptions (juce::jmin (13.0f, (float) buttonHeight * 0.55f), juce::Font::bold));
    }

    Palette palette;
};

// Analog-style needle meter with VU-like ballistics (about 300 ms).
class NeedleMeter : public juce::Component, private juce::Timer
{
public:
    enum class Mode { gainReduction, output };

    explicit NeedleMeter (const Palette& p) : palette (p) { startTimerHz (60); }

    void setMode (Mode m) { mode = m; repaint(); }

    // Called from the editor timer with the latest reading in dB.
    void setTargetDb (float db) { targetDb = db; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (palette.panelEdge);
        g.fillRoundedRectangle (r, 8.0f);
        auto face = r.reduced (6.0f);
        juce::ColourGradient grad (palette.meterFace.brighter (0.15f), face.getCentreX(), face.getY(),
                                   palette.meterFace.darker (0.12f), face.getCentreX(), face.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (face, 5.0f);

        const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.18f);
        const float radius = face.getHeight() * 0.95f;
        const float halfSweep = 0.85f;

        // Scale: -20 .. +3 in both modes. In GR mode the needle rests at 0
        // and falls to the left; in output mode 0 VU is -18 dBFS.
        const float marks[] = { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f };
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.0f, face.getHeight() * 0.09f))));
        for (float m : marks)
        {
            const float a = angleFor (m, halfSweep);
            const bool red = m > 0.0f;
            g.setColour (red ? juce::Colour (0xffc0392b) : palette.meterInk);
            const auto p1 = pivot.getPointOnCircumference (radius, a);
            const auto p2 = pivot.getPointOnCircumference (radius * 0.9f, a);
            g.drawLine ({ p1, p2 }, 1.4f);
            const auto tp = pivot.getPointOnCircumference (radius * 1.08f, a);
            g.drawText (juce::String ((int) std::abs (m)), juce::Rectangle<float> (24.0f, 12.0f).withCentre (tp),
                        juce::Justification::centred);
        }

        g.setColour (palette.meterInk.withAlpha (0.7f));
        g.drawText (mode == Mode::gainReduction ? "GAIN REDUCTION dB" : "OUTPUT VU",
                    face.withTrimmedTop (face.getHeight() * 0.55f).withHeight (14.0f), juce::Justification::centred);

        // Needle.
        const float a = angleFor (shownDb, halfSweep);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (face.toNearestInt());
        g.drawLine ({ pivot, pivot.getPointOnCircumference (radius * 1.02f, a) }, 1.6f);
    }

private:
    static float angleFor (float db, float halfSweep)
    {
        // Rough VU spacing: compress the low end, open up near zero.
        const float clamped = juce::jlimit (-20.0f, 3.0f, db);
        const float lin = std::pow (10.0f, clamped / 20.0f);       // 0.1 .. 1.41
        const float norm = (lin - 0.1f) / (1.413f - 0.1f);          // 0 .. 1
        return -halfSweep + norm * 2.0f * halfSweep;
    }

    void timerCallback() override
    {
        const float wanted = mode == Mode::gainReduction ? targetDb : targetDb + 18.0f;
        shownDb += 0.25f * (wanted - shownDb); // ~300 ms to settle at 60 Hz
        if (std::abs (shownDb - lastPaintedDb) > 0.01f)
        {
            lastPaintedDb = shownDb;
            repaint();
        }
    }

    Palette palette;
    Mode mode = Mode::gainReduction;
    float targetDb = 0.0f, shownDb = 0.0f, lastPaintedDb = 1000.0f;
};

// Buttons that act like a hardware switch for a choice parameter.
class ChoiceButtons : public juce::Component
{
public:
    ChoiceButtons (juce::RangedAudioParameter& param, const juce::StringArray& labels, bool vertical)
        : isVertical (vertical),
          attachment (param, [this] (float v) { select ((int) std::lround (v)); })
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* b = buttons.add (new juce::TextButton (labels[i]));
            b->setClickingTogglesState (false);
            b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (b);
        }
        attachment.sendInitialUpdate();
    }

    std::function<void (int)> onChange;

    void resized() override
    {
        auto r = getLocalBounds();
        const int n = buttons.size();
        for (int i = 0; i < n; ++i)
        {
            auto cell = isVertical ? r.removeFromTop (getHeight() / n) : r.removeFromLeft (getWidth() / n);
            buttons[i]->setBounds (cell.reduced (2));
        }
    }

private:
    void select (int index)
    {
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == index, juce::dontSendNotification);
        if (onChange)
            onChange (index);
    }

    bool isVertical;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

// A knob with its caption underneath.
struct LabelledKnob : public juce::Component
{
    LabelledKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& caption)
        : attachment (state, paramId, knob)
    {
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        knob.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        label.setText (caption, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
        addAndMakeVisible (knob);
        addAndMakeVisible (label);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromBottom (18));
        knob.setBounds (r);
    }

    juce::Slider knob;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};
} // namespace cruz
