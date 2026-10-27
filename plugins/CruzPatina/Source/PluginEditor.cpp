#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff17130f };
    const juce::Colour backgroundTop { 0xff221b15 };
    const juce::Colour panel { 0xff241d17 };
    const juce::Colour panelEdge { 0xff3b3028 };
    const juce::Colour knobBody { 0xff342a21 };
    const juce::Colour knobBodyDark { 0xff1f1914 };
    const juce::Colour track { 0xff40352b };
    const juce::Colour text { 0xfff1e6d2 };
    const juce::Colour dimText { 0xffa8977f };

    const juce::Colour magnitude { 0xffe8b45f };
    const juce::Colour noise { 0xffd9a35b };
    const juce::Colour wobble { 0xff8fb996 };
    const juce::Colour distort { 0xffd9674e };
    const juce::Colour digital { 0xff6fa8c7 };
    const juce::Colour space { 0xffa38ccf };
    const juce::Colour magnetic { 0xffc9b458 };
    const juce::Colour global { 0xffd8cbb4 };
}

namespace patina::ui
{
//==============================================================================
PatinaLookAndFeel::PatinaLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::panelEdge);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::dimText);
    setColour (juce::Label::textColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::dimText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::backgroundColourId, Colours::knobBodyDark);
    setColour (juce::TextEditor::textColourId, Colours::text);
    setColour (juce::TextEditor::highlightColourId, Colours::panelEdge);
    setColour (juce::CaretComponent::caretColourId, Colours::text);
}

void PatinaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (3.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto lineWidth = juce::jmax (2.5f, radius * 0.09f);
    const auto arcRadius = radius - lineWidth * 0.5f;

    juce::Path trackArc;
    trackArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::track);
    g.strokePath (trackArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
    const auto from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.01f)
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const auto bodyRadius = radius - lineWidth * 2.2f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (Colours::knobBody, centre.x, body.getY(),
                                             Colours::knobBodyDark, centre.x, body.getBottom(), false));
    g.fillEllipse (body);
    g.setColour (Colours::panelEdge.brighter (0.15f));
    g.drawEllipse (body, 1.0f);

    const auto tip = centre.getPointOnCircumference (bodyRadius * 0.8f, angle);
    const auto base = centre.getPointOnCircumference (bodyRadius * 0.3f, angle);
    g.setColour (slider.isEnabled() ? Colours::text : Colours::dimText);
    g.drawLine ({ base, tip }, juce::jmax (2.0f, radius * 0.07f));
}

void PatinaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto accent = button.findColour (juce::ToggleButton::tickColourId);
    const auto bounds = button.getLocalBounds().toFloat().reduced (4.0f);
    const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto led = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());

    if (button.getToggleState())
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (led.expanded (3.0f));
        g.setColour (accent);
        g.fillEllipse (led);
    }
    else
    {
        g.setColour (Colours::knobBodyDark);
        g.fillEllipse (led);
        g.setColour (highlighted ? Colours::dimText : Colours::track);
        g.drawEllipse (led, 1.5f);
    }
}

void PatinaLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
    g.setColour (Colours::knobBodyDark);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (box.hasKeyboardFocus (false) ? Colours::dimText : Colours::panelEdge.brighter (0.2f));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

    const auto arrow = bounds.removeFromRight (20.0f).reduced (6.0f, 0.0f).withSizeKeepingCentre (8.0f, 5.0f);
    juce::Path p;
    p.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour (Colours::dimText);
    g.fillPath (p);
}

juce::Font PatinaLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::FontOptions (13.0f);
}

void PatinaLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Label* PatinaLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (12.0f));
    return label;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name,
            juce::Colour accent, bool large)
    : attachment (state, paramId, slider),
      isLarge (large)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, large ? 20 : 16);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxTextColourId, Colours::dimText);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);

    if (auto* param = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        if (param->getLabel().isNotEmpty())
            slider.setTextValueSuffix (" " + param->getLabel());
        if (param->getNormalisableRange().start < 0.0f)
            slider.getProperties().set ("bipolar", true);
    }

    addAndMakeVisible (slider);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (large ? 15.0f : 12.5f, juce::Font::bold));
    label.setColour (juce::Label::textColourId, large ? accent : Colours::text);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (isLarge ? 22 : 16));
    slider.setBounds (area);
}

//==============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& name,
                const juce::StringArray& items)
    // The items must exist before the attachment picks the current one.
    : attachment ((box.addItemList (items, 1), state), paramId, box)
{
    addAndMakeVisible (box);

    label.setText (name, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    addAndMakeVisible (label);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));
    box.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 8, 88), 24).translated (0, -8));
}

//==============================================================================
ModulePanel::ModulePanel (juce::AudioProcessorValueTreeState& s, const juce::String& t, const juce::String& powerId,
                          juce::Colour a)
    : state (s), title (t), accent (a), powerAttachment (s, powerId, power)
{
    power.setColour (juce::ToggleButton::tickColourId, accent);
    power.setTooltip ("Turn " + title + " on or off");
    addAndMakeVisible (power);
}

void ModulePanel::addKnob (const juce::String& paramId, const juce::String& name)
{
    controls.push_back (std::make_unique<Knob> (state, paramId, name, accent));
    addAndMakeVisible (*controls.back());
}

void ModulePanel::addChoice (const juce::String& paramId, const juce::String& name, const juce::StringArray& items)
{
    controls.push_back (std::make_unique<Choice> (state, paramId, name, items));
    addAndMakeVisible (*controls.back());
}

void ModulePanel::refreshPower()
{
    const auto on = power.getToggleState();
    if (on == lastPower)
        return;

    lastPower = on;
    for (auto& c : controls)
        c->setAlpha (on ? 1.0f : 0.4f);
    repaint();
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (lastPower ? accent.withAlpha (0.45f) : Colours::panelEdge);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    auto header = getLocalBounds().removeFromTop (34).reduced (10, 0);
    header.removeFromLeft (26);
    g.setColour (lastPower ? accent : Colours::dimText);
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    g.drawText (title.toUpperCase(), header, juce::Justification::centredLeft);
}

void ModulePanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    auto header = area.removeFromTop (26);
    power.setBounds (header.removeFromLeft (26).withSizeKeepingCentre (24, 24));
    area.removeFromTop (4);

    const auto cellW = area.getWidth() / 2;
    const auto cellH = area.getHeight() / 2;
    const auto count = (int) controls.size();

    for (int i = 0; i < count; ++i)
    {
        const auto row = i / 2;
        auto col = i % 2;
        auto cell = juce::Rectangle<int> (area.getX() + col * cellW, area.getY() + row * cellH, cellW, cellH);

        // A lone control on the last row sits in the middle.
        if (count % 2 == 1 && i == count - 1)
            cell = cell.withX (area.getX() + cellW / 2);

        controls[(size_t) i]->setBounds (cell.reduced (2));
    }
}
} // namespace patina::ui

//==============================================================================
CruzPatinaAudioProcessorEditor::Content::Content (CruzPatinaAudioProcessor& p)
    : processor (p)
{
    using namespace ParamIDs;
    using patina::ui::ModulePanel;
    auto& s = p.getState();

    auto add = [&] (const juce::String& title, const char* powerId, juce::Colour accent) -> ModulePanel&
    {
        modules.push_back (std::make_unique<ModulePanel> (s, title, powerId, accent));
        addAndMakeVisible (*modules.back());
        return *modules.back();
    };

    auto& n = add ("Noise", noiseOn, Colours::noise);
    n.addKnob (noiseAmount, "Amount");
    n.addKnob (noiseTone, "Tone");
    n.addKnob (noiseFollow, "Follow");
    n.addChoice (noiseType, "Type", CruzPatinaAudioProcessor::getNoiseTypeNames());

    auto& w = add ("Wobble", wobbleOn, Colours::wobble);
    w.addKnob (wobbleWow, "Wow");
    w.addKnob (wobbleRate, "Rate");
    w.addKnob (wobbleFlutter, "Flutter");
    w.addKnob (wobbleStereo, "Stereo");

    auto& d = add ("Distort", distortOn, Colours::distort);
    d.addKnob (distortDrive, "Drive");
    d.addKnob (distortTone, "Tone");
    d.addKnob (distortMix, "Mix");
    d.addChoice (distortType, "Type", CruzPatinaAudioProcessor::getDistortTypeNames());

    auto& dg = add ("Digital", digitalOn, Colours::digital);
    dg.addKnob (digitalRate, "Rate");
    dg.addKnob (digitalBits, "Bits");
    dg.addKnob (digitalMix, "Mix");

    auto& sp = add ("Space", spaceOn, Colours::space);
    sp.addKnob (spaceSize, "Size");
    sp.addKnob (spaceDecay, "Decay");
    sp.addKnob (spaceMix, "Mix");

    auto& m = add ("Magnetic", magneticOn, Colours::magnetic);
    m.addKnob (magneticSaturation, "Saturate");
    m.addKnob (magneticWear, "Wear");
    m.addKnob (magneticDropouts, "Dropouts");

    magnitude = std::make_unique<patina::ui::Knob> (s, ParamIDs::magnitude, "MAGNITUDE", Colours::magnitude, true);
    addAndMakeVisible (*magnitude);

    const std::pair<const char*, const char*> globalKnobs[] {
        { inputGain, "Input" }, { tone, "Tone" }, { width, "Width" }, { mix, "Mix" }, { outputGain, "Output" }
    };
    for (const auto& [id, name] : globalKnobs)
    {
        globals.push_back (std::make_unique<patina::ui::Knob> (s, id, name, Colours::global));
        addAndMakeVisible (*globals.back());
    }

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presets.addItem (processor.getProgramName (i), i + 1);
    presets.setTooltip ("Factory presets");
    presets.onChange = [this]
    {
        const auto index = presets.getSelectedId() - 1;
        if (index >= 0 && index != processor.getCurrentProgram())
            processor.setCurrentProgram (index);
        shownProgram = index;
    };
    addAndMakeVisible (presets);

    refresh();
}

void CruzPatinaAudioProcessorEditor::Content::refresh()
{
    for (auto& m : modules)
        m->refreshPower();

    const auto program = processor.getCurrentProgram();
    if (program != shownProgram)
    {
        shownProgram = program;
        presets.setSelectedId (program + 1, juce::dontSendNotification);
    }
}

void CruzPatinaAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (Colours::backgroundTop, 0.0f, 0.0f,
                                             Colours::background, 0.0f, (float) getHeight(), false));
    g.fillAll();

    auto header = getLocalBounds().reduced (20, 0).removeFromTop (64);
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText ("PATINA", header.removeFromLeft (130), juce::Justification::centredLeft);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String::fromUTF8 ("Cruz Audio  \xc2\xb7  vintage colour"), header.removeFromLeft (220),
                juce::Justification::centredLeft);
    g.drawText ("v" JucePlugin_VersionString, header, juce::Justification::centredRight);

    const auto globalPanel = juce::Rectangle<int> (676, 72, 248, 468).toFloat().reduced (0.5f);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (globalPanel, 8.0f);
    g.setColour (Colours::magnitude.withAlpha (0.45f));
    g.drawRoundedRectangle (globalPanel, 8.0f, 1.0f);

    g.setColour (Colours::panelEdge);
    g.drawHorizontalLine (316, globalPanel.getX() + 16.0f, globalPanel.getRight() - 16.0f);
}

void CruzPatinaAudioProcessorEditor::Content::resized()
{
    presets.setBounds (450, 20, 200, 26);

    constexpr int panelW = 208, panelH = 228, gap = 12, left = 16, top = 72;
    for (int i = 0; i < (int) modules.size(); ++i)
        modules[(size_t) i]->setBounds (left + (i % 3) * (panelW + gap), top + (i / 3) * (panelH + gap),
                                        panelW, panelH);

    magnitude->setBounds (juce::Rectangle<int> (676, 84, 248, 220).withSizeKeepingCentre (170, 220));

    const auto gridX = 676 + 8, gridY = 328, cellW = 232 / 3, cellH = 100;
    for (int i = 0; i < (int) globals.size(); ++i)
    {
        auto cell = juce::Rectangle<int> (gridX + (i % 3) * cellW, gridY + (i / 3) * cellH, cellW, cellH);
        if (i >= 3)
            cell = cell.translated (cellW / 2, 0); // centre the two knobs on the bottom row
        globals[(size_t) i]->setBounds (cell.reduced (3));
    }
}

//==============================================================================
CruzPatinaAudioProcessorEditor::CruzPatinaAudioProcessorEditor (CruzPatinaAudioProcessor& p)
    : AudioProcessorEditor (&p),
      content (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);
    content.setSize (baseWidth, baseHeight);

    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (baseWidth, baseHeight);

    startTimerHz (12);
}

CruzPatinaAudioProcessorEditor::~CruzPatinaAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzPatinaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);
}

void CruzPatinaAudioProcessorEditor::resized()
{
    const auto scale = (float) getWidth() / (float) baseWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
}

void CruzPatinaAudioProcessorEditor::timerCallback()
{
    content.refresh();
}
