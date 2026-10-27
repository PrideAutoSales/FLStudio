#include "PluginEditor.h"

namespace Palette
{
    const juce::Colour body { 0xff2a2724 };
    const juce::Colour bodyDark { 0xff1c1a18 };
    const juce::Colour deck { 0xff34302b };
    const juce::Colour cream { 0xffece3cf };
    const juce::Colour dimCream { 0xff9d9484 };
    const juce::Colour amber { 0xffe3a246 };
    const juce::Colour red { 0xffc8483a };
    const juce::Colour meterFace { 0xfff1e4bd };
    const juce::Colour tape { 0xff4a3426 };
    const juce::Colour metal { 0xffb9b6ae };
}

//==============================================================================
TapeLookAndFeel::TapeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Palette::cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Palette::dimCream);
    setColour (juce::TextButton::buttonColourId, Palette::bodyDark);
    setColour (juce::TextButton::buttonOnColourId, Palette::amber);
    setColour (juce::TextButton::textColourOffId, Palette::cream);
    setColour (juce::TextButton::textColourOnId, Palette::bodyDark);
    setColour (juce::ToggleButton::textColourId, Palette::cream);
    setColour (juce::ToggleButton::tickColourId, Palette::amber);
    setColour (juce::ToggleButton::tickDisabledColourId, Palette::dimCream);
}

void TapeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Scale ticks around the knob.
    g.setColour (Palette::dimCream.withAlpha (0.6f));
    for (int i = 0; i <= 10; ++i)
    {
        const auto a = rotaryStartAngle + (float) i / 10.0f * (rotaryEndAngle - rotaryStartAngle);
        const auto p1 = centre.getPointOnCircumference (radius - 1.0f, a);
        const auto p2 = centre.getPointOnCircumference (radius - (i % 5 == 0 ? 7.0f : 4.0f), a);
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    // Value arc. Controls that go both ways (like Bias) draw it from zero.
    const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto zeroPos = (float) slider.valueToProportionOfLength (0.0);
    const auto arcStart = bipolar ? rotaryStartAngle + zeroPos * (rotaryEndAngle - rotaryStartAngle) : rotaryStartAngle;

    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius - 10.0f, radius - 10.0f, 0.0f, arcStart, angle, true);
    g.setColour (Palette::amber.withAlpha (0.85f));
    g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Knob cap with a soft top light.
    const auto capRadius = radius - 15.0f;
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillEllipse (cap.translated (0.0f, 2.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4540), cap.getX(), cap.getY(),
                                             juce::Colour (0xff1e1c1a), cap.getRight(), cap.getBottom(), false));
    g.fillEllipse (cap);
    g.setColour (juce::Colour (0xff5c564f));
    g.drawEllipse (cap.reduced (0.5f), 1.0f);

    // Pointer.
    const auto p1 = centre.getPointOnCircumference (capRadius * 0.25f, angle);
    const auto p2 = centre.getPointOnCircumference (capRadius * 0.85f, angle);
    g.setColour (Palette::amber);
    g.drawLine ({ p1, p2 }, 3.0f);
}

void TapeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                            bool isMouseOver, bool isButtonDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto colour = button.getToggleState() ? Palette::amber : Palette::bodyDark;

    if (isButtonDown)
        colour = colour.brighter (0.15f);
    else if (isMouseOver)
        colour = colour.brighter (0.07f);

    const auto flatLeft = button.isConnectedOnLeft();
    const auto flatRight = button.isConnectedOnRight();
    const auto r = 5.0f;

    juce::Path p;
    p.addRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), r, r,
                           ! flatLeft, ! flatRight, ! flatLeft, ! flatRight);
    g.setColour (colour);
    g.fillPath (p);
    g.setColour (juce::Colour (0xff4d4740));
    g.strokePath (p, juce::PathStrokeType (1.0f));
}

juce::Font TapeLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.5f), juce::Font::bold);
}

juce::Font TapeLookAndFeel::getLabelFont (juce::Label&)
{
    return juce::FontOptions (12.5f, juce::Font::bold);
}

//==============================================================================
Selector::Selector (juce::RangedAudioParameter& parameter, const juce::String& t)
    : title (t),
      attachment (parameter, [this] (float v) { update (v); })
{
    const auto choices = parameter.getAllValueStrings();

    for (int i = 0; i < choices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (choices[i].upToFirstOccurrenceOf (" ips", false, false)));
        b->setClickingTogglesState (false);
        b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0)
                              | (i < choices.size() - 1 ? juce::Button::ConnectedOnRight : 0));
        b->onClick = [this, i] { attachment.setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment.sendInitialUpdate();
}

void Selector::update (float value)
{
    const auto index = juce::roundToInt (value);

    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setToggleState (i == index, juce::dontSendNotification);
}

void Selector::paint (juce::Graphics& g)
{
    g.setColour (Palette::dimCream);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (title, getLocalBounds().removeFromTop (18), juce::Justification::centredLeft);
}

void Selector::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (20);
    const auto w = area.getWidth() / juce::jmax (1, buttons.size());

    for (auto* b : buttons)
        b->setBounds (area.removeFromLeft (w));

    if (! buttons.isEmpty())
        buttons.getLast()->setBounds (buttons.getLast()->getBounds().withRight (getWidth()));
}

//==============================================================================
namespace
{
    constexpr float vuMin = -20.0f, vuMax = 3.0f;

    // VU scales are linear in voltage, which bunches the low end together.
    float vuPosition (float db)
    {
        const auto v = std::pow (10.0f, juce::jlimit (vuMin - 1.0f, vuMax + 0.4f, db) / 20.0f);
        const auto lo = std::pow (10.0f, vuMin / 20.0f), hi = std::pow (10.0f, vuMax / 20.0f);
        return (v - lo) / (hi - lo);
    }
}

void VuMeter::push (float rmsLinear, float dt)
{
    // 0 VU sits at -18 dBFS RMS, the usual reference for mixing.
    const auto target = juce::Decibels::gainToDecibels (rmsLinear, -60.0f) + 18.0f;
    const auto coeff = 1.0f - std::exp (-dt / 0.065f);
    vu += (target - vu) * coeff;
}

void VuMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour (Palette::bodyDark);
    g.fillRoundedRectangle (bounds, 6.0f);

    const auto face = bounds.reduced (5.0f);
    g.setGradientFill (juce::ColourGradient (Palette::meterFace, face.getCentreX(), face.getY(),
                                             Palette::meterFace.darker (0.18f), face.getCentreX(), face.getBottom(), false));
    g.fillRoundedRectangle (face, 4.0f);

    const auto pivot = juce::Point<float> (face.getCentreX(), face.getBottom() + face.getHeight() * 0.35f);
    const auto radius = face.getHeight() * 1.05f;
    const auto spread = 0.62f;
    const auto angleFor = [spread] (float db) { return -spread + 2.0f * spread * vuPosition (db); };

    // Red zone above 0 VU.
    juce::Path redZone;
    redZone.addCentredArc (pivot.x, pivot.y, radius * 0.86f, radius * 0.86f, 0.0f, angleFor (0.0f), angleFor (vuMax), true);
    g.setColour (Palette::red);
    g.strokePath (redZone, juce::PathStrokeType (4.0f));

    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));

    for (auto db : { -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3 })
    {
        const auto a = angleFor ((float) db);
        g.setColour (db > 0 ? Palette::red : Palette::bodyDark);
        g.drawLine ({ pivot.getPointOnCircumference (radius * 0.80f, a), pivot.getPointOnCircumference (radius * 0.92f, a) }, 1.2f);

        if (db == -20 || db == -10 || db == -5 || db == -3 || db == 0 || db == 3)
        {
            const auto t = pivot.getPointOnCircumference (radius * 1.0f, a);
            const auto text = db > 0 ? "+" + juce::String (db) : juce::String (db);
            g.drawText (text, juce::Rectangle<float> (26.0f, 12.0f).withCentre (t), juce::Justification::centred);
        }
    }

    g.setColour (Palette::bodyDark.withAlpha (0.8f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("VU  " + label, face.withTrimmedTop (face.getHeight() * 0.52f).withTrimmedBottom (6.0f),
                juce::Justification::centredBottom);

    // Needle, clipped to the face.
    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    const auto a = angleFor (vu);
    g.setColour (juce::Colour (0xff141210));
    g.drawLine ({ pivot.getPointOnCircumference (radius * 0.35f, a), pivot.getPointOnCircumference (radius * 0.97f, a) }, 1.6f);
    g.restoreState();

    // Glass shade.
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (face.withHeight (face.getHeight() * 0.4f), 4.0f);
}

//==============================================================================
void Reels::advance (float revolutionsPerSecond, float dt)
{
    angle = std::fmod (angle + revolutionsPerSecond * dt * juce::MathConstants<float>::twoPi,
                       juce::MathConstants<float>::twoPi);
}

void Reels::drawReel (juce::Graphics& g, juce::Point<float> centre, float radius, float fill, float a) const
{
    const auto hubRadius = radius * 0.28f;
    const auto packRadius = hubRadius + (radius * 0.92f - hubRadius) * fill;

    // Tape pack.
    g.setColour (Palette::tape);
    g.fillEllipse (juce::Rectangle<float> (packRadius * 2.0f, packRadius * 2.0f).withCentre (centre));
    g.setColour (Palette::tape.brighter (0.25f));
    for (float r = hubRadius + 4.0f; r < packRadius; r += 5.0f)
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre), 0.4f);

    // Flange with three windows that turn with the reel.
    juce::Path flange;
    flange.addEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    flange.setUsingNonZeroWinding (false);

    for (int i = 0; i < 3; ++i)
    {
        const auto start = a + (float) i * juce::MathConstants<float>::twoPi / 3.0f + 0.25f;
        juce::Path window;
        window.addCentredArc (centre.x, centre.y, radius * 0.86f, radius * 0.86f, 0.0f, start, start + 1.45f, true);
        window.addCentredArc (centre.x, centre.y, hubRadius * 1.5f, hubRadius * 1.5f, 0.0f, start + 1.45f, start, false);
        window.closeSubPath();
        flange.addPath (window);
    }

    g.setColour (Palette::metal.withAlpha (0.9f));
    g.fillPath (flange);
    g.setColour (juce::Colour (0xff7a776f));
    g.strokePath (flange, juce::PathStrokeType (1.0f));

    // Hub.
    g.setColour (Palette::bodyDark);
    g.fillEllipse (juce::Rectangle<float> (hubRadius * 2.0f, hubRadius * 2.0f).withCentre (centre));
    g.setColour (Palette::metal);
    for (int i = 0; i < 3; ++i)
    {
        const auto sa = a + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
        g.drawLine ({ centre.getPointOnCircumference (hubRadius * 0.3f, sa), centre.getPointOnCircumference (hubRadius * 0.95f, sa) }, 3.0f);
    }
    g.fillEllipse (juce::Rectangle<float> (hubRadius * 0.5f, hubRadius * 0.5f).withCentre (centre));
}

void Reels::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto radius = juce::jmin (bounds.getHeight() * 0.44f, bounds.getWidth() * 0.13f);
    const auto leftCentre = juce::Point<float> (bounds.getX() + radius + 14.0f, bounds.getCentreY() - 4.0f);
    const auto rightCentre = juce::Point<float> (bounds.getRight() - radius - 14.0f, bounds.getCentreY() - 4.0f);

    // Tape path along the bottom, past the head block.
    const auto headY = bounds.getBottom() - 12.0f;
    const auto leftPack = radius * 0.28f + (radius * 0.92f - radius * 0.28f) * 0.85f;
    const auto rightPack = radius * 0.28f + (radius * 0.92f - radius * 0.28f) * 0.45f;

    juce::Path path;
    path.startNewSubPath (leftCentre.x - leftPack * 0.2f, leftCentre.y + leftPack);
    path.lineTo (leftCentre.x + radius * 0.7f, headY);
    path.lineTo (rightCentre.x - radius * 0.7f, headY);
    path.lineTo (rightCentre.x + rightPack * 0.2f, rightCentre.y + rightPack);
    g.setColour (Palette::tape.brighter (0.15f));
    g.strokePath (path, juce::PathStrokeType (2.5f));

    for (auto gx : { leftCentre.x + radius * 0.7f, rightCentre.x - radius * 0.7f })
    {
        g.setColour (Palette::metal);
        g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ gx, headY }));
    }

    const auto headArea = juce::Rectangle<float> (70.0f, 16.0f).withCentre ({ bounds.getCentreX(), headY + 2.0f });
    g.setColour (juce::Colour (0xff4e4943));
    g.fillRoundedRectangle (headArea, 3.0f);
    g.setColour (Palette::metal);
    for (int i = 0; i < 3; ++i)
        g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 9.0f).withCentre ({ headArea.getX() + 14.0f + (float) i * 21.0f, headArea.getY() + 3.0f }), 2.0f);

    drawReel (g, leftCentre, radius, 0.85f, angle);
    // The take-up reel turns a little faster because it holds less tape.
    drawReel (g, rightCentre, radius, 0.45f, angle * 1.3f);
}

//==============================================================================
CruzTapeAudioProcessorEditor::CruzTapeAudioProcessorEditor (CruzTapeAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      speed (*p.getState().getParameter (ParamIDs::speed), "SPEED  (ips)"),
      formula (*p.getState().getParameter (ParamIDs::formula), "TAPE"),
      calibration (*p.getState().getParameter (ParamIDs::calibration), "CALIBRATION"),
      emphasis (*p.getState().getParameter (ParamIDs::emphasis), "EQ CURVE"),
      autoGainAttachment (p.getState(), ParamIDs::autoGain, autoGain)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (reels);
    addAndMakeVisible (leftMeter);
    addAndMakeVisible (rightMeter);

    for (auto* s : { &speed, &formula, &calibration, &emphasis })
        addAndMakeVisible (s);

    addAndMakeVisible (autoGain);
    autoGain.setTooltip ("Keeps the level steady when you turn Input up, so you only hear the added saturation.");

    addKnob (input, ParamIDs::input, "INPUT", " dB");
    addKnob (bias, ParamIDs::bias, "BIAS", " %");
    addKnob (wow, ParamIDs::wow, "WOW", " %");
    addKnob (flutter, ParamIDs::flutter, "FLUTTER", " %");
    addKnob (hiss, ParamIDs::hiss, "HISS", " %");
    addKnob (mix, ParamIDs::mix, "MIX", " %");
    addKnob (output, ParamIDs::output, "OUTPUT", " dB");

    setSize (840, 540);
    lastTimerTime = juce::Time::getMillisecondCounter();
    startTimerHz (30);
}

CruzTapeAudioProcessorEditor::~CruzTapeAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzTapeAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId, const juce::String& name,
                                            const juce::String& suffix)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Palette::cream);
    k.slider.setRotaryParameters (juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
    addAndMakeVisible (k.slider);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getState(), paramId, k.slider);
    k.slider.setNumDecimalPlacesToDisplay (1);
    k.slider.setTextValueSuffix (suffix);

    if (auto* param = processor.getState().getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);
}

void CruzTapeAudioProcessorEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    const auto dt = juce::jlimit (0.001f, 0.2f, (float) (now - lastTimerTime) / 1000.0f);
    lastTimerTime = now;

    const auto running = now - processor.getLastProcessTime() < 300;
    leftMeter.push (running ? processor.getOutputLevel (0) : 0.0f, dt);
    rightMeter.push (running ? processor.getOutputLevel (1) : 0.0f, dt);

    if (running)
    {
        static constexpr float ips[] = { 7.5f, 15.0f, 30.0f };
        const auto index = juce::jlimit (0, 2, juce::roundToInt (processor.getState().getRawParameterValue (ParamIDs::speed)->load()));
        reels.advance (ips[index] / 15.0f * 0.6f, dt);
        reels.repaint();
    }

    leftMeter.repaint();
    rightMeter.repaint();
}

void CruzTapeAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Palette::body.brighter (0.06f), 0.0f, 0.0f,
                                             Palette::body.darker (0.25f), 0.0f, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().removeFromTop (54).reduced (22, 0);
    g.setColour (Palette::cream);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("CRUZ TAPE", header, juce::Justification::centredLeft);

    g.setColour (Palette::dimCream);
    g.setFont (juce::FontOptions (12.5f));
    g.drawText ("two-track mastering recorder   v" JucePlugin_VersionString, header, juce::Justification::centredRight);

    // Deck plate behind the reels and meters.
    const auto deck = getLocalBounds().withTrimmedTop (54).removeFromTop (220).reduced (14, 0).toFloat();
    g.setColour (Palette::deck);
    g.fillRoundedRectangle (deck, 10.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (deck.reduced (0.5f), 10.0f, 1.0f);

    // Control panel.
    const auto panel = getLocalBounds().withTrimmedTop (54 + 220 + 12).reduced (14, 0).withTrimmedBottom (14).toFloat();
    g.setColour (Palette::bodyDark.withAlpha (0.55f));
    g.fillRoundedRectangle (panel, 10.0f);

    // Screws in the corners, a small touch of hardware.
    g.setColour (juce::Colour (0xff57514a));
    for (auto pt : { deck.getTopLeft() + juce::Point<float> (12, 12), deck.getTopRight() + juce::Point<float> (-12, 12),
                     deck.getBottomLeft() + juce::Point<float> (12, -12), deck.getBottomRight() + juce::Point<float> (-12, -12) })
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
}

void CruzTapeAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (54);

    auto deck = area.removeFromTop (220).reduced (14, 0);
    reels.setBounds (deck.reduced (12, 10));

    auto meters = deck.withSizeKeepingCentre (340, 120).translated (0, -18);
    leftMeter.setBounds (meters.removeFromLeft (165));
    meters.removeFromLeft (10);
    rightMeter.setBounds (meters);

    area.removeFromTop (12);
    auto panel = area.reduced (14, 0).withTrimmedBottom (14).reduced (18, 12);

    auto selectors = panel.removeFromTop (52);
    speed.setBounds (selectors.removeFromLeft (132));
    selectors.removeFromLeft (14);
    formula.setBounds (selectors.removeFromLeft (256));
    selectors.removeFromLeft (14);
    calibration.setBounds (selectors.removeFromLeft (132));
    selectors.removeFromLeft (14);
    emphasis.setBounds (selectors.removeFromLeft (96));
    autoGain.setBounds (selectors.withTrimmedTop (20).withTrimmedLeft (14));

    panel.removeFromTop (12);
    auto knobs = panel;
    const auto w = knobs.getWidth() / 7;

    for (auto* k : { &input, &bias, &wow, &flutter, &hiss, &mix, &output })
    {
        auto cell = knobs.removeFromLeft (w);
        k->label.setBounds (cell.removeFromTop (16));
        k->slider.setBounds (cell);
    }
}
