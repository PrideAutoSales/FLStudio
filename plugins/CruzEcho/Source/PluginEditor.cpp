#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff17130f };
    const juce::Colour panel { 0xff221c16 };
    const juce::Colour panelEdge { 0xff3a3027 };
    const juce::Colour knobBody { 0xff2d251e };
    const juce::Colour text { 0xfff1e6d6 };
    const juce::Colour dimText { 0xff9c8f7f };
    const juce::Colour accent { 0xffffa94d };   // warm amber, like a tape machine's VU lamp
    const juce::Colour accent2 { 0xff5fd1c4 };  // right channel in ping-pong
}

namespace cruzui
{

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Colours::accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, Colours::dimText);
    setColour (juce::TextButton::textColourOffId, Colours::dimText);
    setColour (juce::TextButton::textColourOnId, Colours::background);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Colours::dimText);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto trackWidth = juce::jmax (3.0f, radius * 0.1f);
    const auto arcRadius = radius - trackWidth * 0.5f;
    const bool enabled = slider.isEnabled();

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::panelEdge);
    g.strokePath (track, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Bipolar knobs (feel, output) light up from the centre.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto from = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                              : startAngle;

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (enabled ? Colours::accent : Colours::dimText);
    g.strokePath (value, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    const auto bodyRadius = arcRadius - trackWidth * 1.6f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (Colours::knobBody.brighter (0.3f), centre.x, centre.y - bodyRadius,
                                             Colours::knobBody.darker (0.1f), centre.x, centre.y + bodyRadius, false));
    g.fillEllipse (body);
    g.setColour (Colours::panelEdge.brighter (0.2f));
    g.drawEllipse (body, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyRadius + 3.0f, 3.0f, bodyRadius * 0.45f, 1.5f);
    g.setColour (enabled ? Colours::text : Colours::dimText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                        bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = button.getToggleState();

    g.setColour (on ? Colours::accent : (highlighted || down ? Colours::knobBody.brighter (0.15f) : Colours::knobBody));
    g.fillRoundedRectangle (r, 5.0f);

    if (! on)
    {
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.5f), juce::Font::bold);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (13.0f));
    return label;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& titleText)
    : attachment (state, paramID, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);

    if (auto* param = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    addAndMakeVisible (slider);

    title.setText (titleText.toUpperCase(), juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (title);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID));
    jassert (param != nullptr);

    const int group = juce::Random::getSystemRandom().nextInt (1 << 20) + 1;

    for (int i = 0; i < param->choices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (param->choices[i]));
        b->setClickingTogglesState (false);
        b->setRadioGroupId (group);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        const int index = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == index, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());

    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w));
}

//==============================================================================
EchoDisplay::EchoDisplay (CruzEchoAudioProcessor& p) : processor (p)
{
    startTimerHz (30);
}

float EchoDisplay::param (const char* id) const
{
    return processor.getState().getRawParameterValue (id)->load();
}

void EchoDisplay::timerCallback()
{
    glow = 0.8f * glow + 0.2f * juce::jlimit (0.0f, 1.0f, processor.getWetLevel() * 3.0f);
    repaint();
}

void EchoDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (Colours::panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

    auto inner = area.reduced (18.0f, 14.0f);
    auto caption = inner.removeFromTop (20.0f);

    const float echoMs = processor.getEchoTimeMs();
    const bool synced = param (ParamIDs::sync) > 0.5f;
    const float fb = param (ParamIDs::feedback) / 100.0f;
    const float swing = param (ParamIDs::groove) / 100.0f * cruzecho::EchoEngine::grooveMax;
    const int mode = (int) param (ParamIDs::mode);

    juce::String timeText;
    if (synced)
        timeText << NoteValues::names()[(int) param (ParamIDs::timeNote)] << "  at  "
                 << juce::String (processor.getHostBpm(), 1) << " BPM  =  ";
    timeText << juce::String (echoMs, echoMs < 100.0f ? 1 : 0) << " ms";

    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.setColour (Colours::text);
    g.drawText (timeText, caption, juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (Colours::dimText);
    g.drawText (mode == 2 ? "LEFT above, RIGHT below" : "Repeats", caption, juce::Justification::centredRight);

    inner.removeFromTop (6.0f);
    const int repeats = 10;
    const float span = (float) (repeats + 1);
    const float midY = mode == 2 ? inner.getCentreY() : inner.getBottom();
    const float maxH = mode == 2 ? inner.getHeight() * 0.5f - 2.0f : inner.getHeight();

    // Beat grid, one line per straight echo.
    g.setColour (Colours::panelEdge);
    for (int n = 0; n <= repeats; ++n)
    {
        const float x = inner.getX() + inner.getWidth() * (float) n / span;
        g.drawVerticalLine ((int) x, inner.getY(), inner.getBottom());
    }
    g.drawHorizontalLine ((int) midY, inner.getX(), inner.getRight());

    // The dry hit at time zero.
    g.setColour (Colours::text.withAlpha (0.6f));
    g.fillRoundedRectangle (inner.getX() - 2.0f, midY - maxH, 4.0f, maxH * (mode == 2 ? 2.0f : 1.0f), 2.0f);

    float level = 1.0f;
    for (int n = 1; n <= repeats; ++n)
    {
        const float t = (float) n + ((n % 2) == 1 ? swing : 0.0f);
        const float x = inner.getX() + inner.getWidth() * t / span;
        // Bar height on a 48 dB scale, so quiet repeats still show.
        const float db = 20.0f * std::log10 (juce::jmax (level, 1.0e-6f));
        const float h = juce::jmax (2.0f, maxH * juce::jlimit (0.0f, 1.0f, 1.0f + db / 48.0f));
        const bool right = mode == 2 && (n % 2) == 0;
        const auto colour = right ? Colours::accent2 : Colours::accent;

        juce::Rectangle<float> bar (x - 4.0f, right ? midY : midY - h, 8.0f, h);
        g.setColour (colour.withAlpha (0.18f + 0.25f * glow));
        g.fillRoundedRectangle (bar.expanded (3.0f, 0.0f), 4.0f);
        g.setColour (colour.withAlpha (juce::jlimit (0.35f, 1.0f, 0.4f + level)));
        g.fillRoundedRectangle (bar, 3.0f);

        level *= fb;
    }
}

} // namespace cruzui

//==============================================================================
CruzEchoAudioProcessorEditor::CruzEchoAudioProcessorEditor (CruzEchoAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      styleButtons (p.getState(), ParamIDs::style),
      modeButtons (p.getState(), ParamIDs::mode),
      display (p),
      syncAttachment (p.getState(), ParamIDs::sync, syncButton),
      timeNote (p.getState(), ParamIDs::timeNote, "Time"),
      timeMs (p.getState(), ParamIDs::timeMs, "Time"),
      feel (p.getState(), ParamIDs::feel, "Feel"),
      groove (p.getState(), ParamIDs::groove, "Groove"),
      feedback (p.getState(), ParamIDs::feedback, "Feedback"),
      mix (p.getState(), ParamIDs::mix, "Mix"),
      output (p.getState(), ParamIDs::output, "Output"),
      saturation (p.getState(), ParamIDs::saturation, "Saturation"),
      wobble (p.getState(), ParamIDs::wobble, "Wobble"),
      lowCut (p.getState(), ParamIDs::lowCut, "Low Cut"),
      highCut (p.getState(), ParamIDs::highCut, "High Cut"),
      width (p.getState(), ParamIDs::width, "Width"),
      duck (p.getState(), ParamIDs::duck, "Duck"),
      syncWatcher (*p.getState().getParameter (ParamIDs::sync), [this] (float) { updateTimeKnob(); })
{
    addAndMakeVisible (styleButtons);
    addAndMakeVisible (modeButtons);
    addAndMakeVisible (display);
    addAndMakeVisible (syncButton);

    for (auto* k : { &timeNote, &timeMs, &feel, &groove, &feedback, &mix, &output,
                     &saturation, &wobble, &lowCut, &highCut, &width, &duck })
        addAndMakeVisible (k);

    // After the children are added, so their value boxes pick up these colours.
    setLookAndFeel (&lookAndFeel);
    syncWatcher.sendInitialUpdate();
    setSize (900, 580);
}

CruzEchoAudioProcessorEditor::~CruzEchoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzEchoAudioProcessorEditor::updateTimeKnob()
{
    const bool synced = processor.getState().getRawParameterValue (ParamIDs::sync)->load() > 0.5f;
    timeNote.setVisible (synced);
    timeMs.setVisible (! synced);
}

void CruzEchoAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto area = getLocalBounds().reduced (20);
    auto header = area.removeFromTop (44);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("CRUZ", header.removeFromLeft (70), juce::Justification::centredLeft);
    g.setColour (Colours::accent);
    g.drawText ("ECHO", header.removeFromLeft (80), juce::Justification::centredLeft);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v" JucePlugin_VersionString, header.removeFromRight (50), juce::Justification::centredRight);

    // Panels behind the two knob rows.
    auto knobs = getLocalBounds().reduced (20).removeFromBottom (300);
    for (auto row : { knobs.removeFromTop (146), knobs.withTrimmedTop (8) })
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (row.toFloat(), 8.0f);
    }

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    auto labels = getLocalBounds().reduced (20).withTrimmedTop (44);
    g.drawText ("STYLE", labels.removeFromTop (18), juce::Justification::centredLeft);
}

void CruzEchoAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    auto header = area.removeFromTop (44);
    header.removeFromLeft (170);
    header.removeFromRight (60);
    modeButtons.setBounds (header.removeFromRight (300).reduced (0, 6));

    area.removeFromTop (18);
    auto styleRow = area.removeFromTop (38);
    syncButton.setBounds (styleRow.removeFromRight (150).reduced (8, 4));
    styleButtons.setBounds (styleRow.withTrimmedRight (12));

    auto knobs = area.removeFromBottom (300);
    area.reduce (0, 12);
    display.setBounds (area);

    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<std::initializer_list<juce::Component*>> slots)
    {
        row.reduce (10, 10);
        const int w = row.getWidth() / (int) slots.size();
        for (auto& slot : slots)
        {
            auto cell = row.removeFromLeft (w);
            for (auto* c : slot)
                c->setBounds (cell);
        }
    };

    layoutRow (knobs.removeFromTop (146), { { &timeNote, &timeMs }, { &feel }, { &groove }, { &feedback }, { &mix }, { &output } });
    layoutRow (knobs.withTrimmedTop (8), { { &saturation }, { &wobble }, { &lowCut }, { &highCut }, { &width }, { &duck } });
}
