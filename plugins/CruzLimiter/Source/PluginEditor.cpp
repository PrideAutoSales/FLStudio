#include "PluginEditor.h"

namespace
{
    juce::String formatLoudness (float lufs)
    {
        return lufs <= -70.0f ? juce::String ("--") : juce::String (lufs, 1);
    }
}

//==============================================================================
LimiterLookAndFeel::LimiterLookAndFeel()
{
    using namespace LimiterColours;
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, dimText);
    setColour (juce::ComboBox::backgroundColourId, panelEdge);
    setColour (juce::ComboBox::outlineColourId, grid);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::TextButton::buttonColourId, panelEdge);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TooltipWindow::backgroundColourId, panel.brighter (0.15f));
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, grid);
}

void LimiterLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                           float startAngle, float endAngle, juce::Slider& slider)
{
    using namespace LimiterColours;
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto stroke = juce::jmax (3.0f, radius * 0.14f);
    const auto arcRadius = radius - stroke * 0.5f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (grid);
    g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (slider.isEnabled() ? accent : dimText);
    g.strokePath (value, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto knobRadius = arcRadius - stroke * 1.4f;
    g.setColour (panelEdge);
    g.fillEllipse (juce::Rectangle<float> (knobRadius * 2.0f, knobRadius * 2.0f).withCentre (centre));

    const auto tip = centre.getPointOnCircumference (knobRadius * 0.75f, angle);
    g.setColour (text);
    g.drawLine ({ centre.getPointOnCircumference (knobRadius * 0.2f, angle), tip }, 2.0f);
}

void LimiterLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                           float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    using namespace LimiterColours;
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto track = bounds.withSizeKeepingCentre (6.0f, bounds.getHeight());

    g.setColour (grid);
    g.fillRoundedRectangle (track, 3.0f);

    g.setColour (accent.withAlpha (0.8f));
    g.fillRoundedRectangle (track.withTop (sliderPos), 3.0f);

    const auto thumb = juce::Rectangle<float> (juce::jmin (bounds.getWidth(), 34.0f), 14.0f)
                           .withCentre ({ bounds.getCentreX(), sliderPos });
    g.setColour (text);
    g.fillRoundedRectangle (thumb, 4.0f);
    g.setColour (background);
    g.drawLine (thumb.getX() + 7.0f, thumb.getCentreY(), thumb.getRight() - 7.0f, thumb.getCentreY(), 1.5f);
}

void LimiterLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    using namespace LimiterColours;
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = button.getToggleState();

    g.setColour (on ? accent : (highlighted ? panelEdge.brighter (0.1f) : panelEdge));
    g.fillRoundedRectangle (bounds, bounds.getHeight() * 0.5f);

    g.setColour (on ? background : text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (13.0f, on ? juce::Font::bold : juce::Font::plain));
    g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
}

//==============================================================================
LevelDisplay::LevelDisplay (juce::AudioProcessorValueTreeState& s)
    : state (s), history (2048)
{
    setInterceptsMouseClicks (false, false);
}

void LevelDisplay::addFrames (const MeterFrame* frames, int count)
{
    for (int i = 0; i < count; ++i)
    {
        history[(size_t) writePos] = frames[i];
        writePos = (writePos + 1) % (int) history.size();
    }
}

float LevelDisplay::yForDb (float db, juce::Rectangle<float> area) const noexcept
{
    const auto clamped = juce::jlimit (bottomDb, topDb, db);
    return juce::jmap (clamped, topDb, bottomDb, area.getY(), area.getBottom());
}

void LevelDisplay::paint (juce::Graphics& g)
{
    using namespace LimiterColours;
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    // Leave room for the gain fader on the left and the dB scale on the right.
    const auto plot = bounds.reduced (10.0f).withTrimmedLeft (58.0f).withTrimmedRight (30.0f);

    g.setFont (juce::FontOptions (11.0f));
    for (int db = 0; db >= (int) bottomDb; db -= 6)
    {
        const auto y = yForDb ((float) db, plot);
        g.setColour (grid);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        g.setColour (dimText);
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getRight() + 4.0f, y - 7.0f, 26.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    // Newest frame at the right edge, two pixels per frame.
    constexpr float step = 2.0f;
    const auto columns = juce::jmin ((int) history.size(), (int) (plot.getWidth() / step));
    const auto bottom = plot.getBottom();

    juce::Path inputPath, outputPath, reductionPath;
    inputPath.startNewSubPath (plot.getRight(), bottom);
    outputPath.startNewSubPath (plot.getRight(), bottom);
    reductionPath.startNewSubPath (plot.getRight(), yForDb (0.0f, plot));

    for (int i = 0; i < columns; ++i)
    {
        const auto& f = history[(size_t) ((writePos - 1 - i + (int) history.size()) % (int) history.size())];
        const auto x = plot.getRight() - (float) i * step;
        inputPath.lineTo (x, yForDb (f.inputDb, plot));
        outputPath.lineTo (x, yForDb (f.outputDb, plot));
        reductionPath.lineTo (x, yForDb (-f.reductionDb, plot));
    }

    const auto left = plot.getRight() - (float) (columns - 1) * step;
    inputPath.lineTo (left, bottom);
    outputPath.lineTo (left, bottom);
    reductionPath.lineTo (left, yForDb (0.0f, plot));
    inputPath.closeSubPath();
    outputPath.closeSubPath();
    reductionPath.closeSubPath();

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    g.setColour (input);
    g.fillPath (inputPath);
    g.setColour (output.withAlpha (0.55f));
    g.fillPath (outputPath);
    g.setColour (output);
    g.strokePath (outputPath, juce::PathStrokeType (1.0f));
    g.setColour (reduction.withAlpha (0.45f));
    g.fillPath (reductionPath);
    g.setColour (reduction);
    g.strokePath (reductionPath, juce::PathStrokeType (1.2f));

    const auto ceilingDb = state.getRawParameterValue (ParamIDs::ceiling)->load();
    const auto ceilingY = yForDb (ceilingDb, plot);
    g.setColour (ceiling.withAlpha (0.9f));
    const float dashes[] { 6.0f, 4.0f };
    g.drawDashedLine ({ plot.getX(), ceilingY, plot.getRight(), ceilingY }, dashes, 2, 1.2f);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Ceiling " + juce::String (ceilingDb, 1) + " dB",
                juce::Rectangle<float> (plot.getX() + 6.0f, ceilingY + 2.0f, 140.0f, 14.0f),
                juce::Justification::centredLeft);

    g.restoreState();

    // Legend, on a backing so it stays readable over the level areas
    auto legend = juce::Rectangle<float> (plot.getX() + 6.0f, plot.getBottom() - 22.0f, 262.0f, 18.0f);
    g.setColour (panel.withAlpha (0.85f));
    g.fillRoundedRectangle (legend, 4.0f);
    legend.reduce (6.0f, 2.0f);
    auto drawKey = [&] (juce::Colour colour, const juce::String& name)
    {
        g.setColour (colour);
        g.fillRoundedRectangle (legend.removeFromLeft (10.0f).withSizeKeepingCentre (10.0f, 10.0f), 2.0f);
        legend.removeFromLeft (5.0f);
        g.setColour (dimText);
        g.drawText (name, legend.removeFromLeft (name.length() * 7.0f + 6.0f), juce::Justification::centredLeft);
    };
    drawKey (input.brighter (0.4f), "Input");
    drawKey (output, "Output");
    drawKey (reduction, "Reduction");
}

//==============================================================================
MeterPanel::MeterPanel (CruzLimiterAudioProcessor& p) : processor (p)
{
    resetButton.setTooltip ("Clears the loudness and true-peak readings so you can measure a new pass.");
    resetButton.onClick = [this] { processor.resetMeters(); };
    addAndMakeVisible (resetButton);
}

void MeterPanel::update()
{
    for (int c = 0; c < 2; ++c)
    {
        const auto db = juce::Decibels::gainToDecibels (processor.takeOutputPeak (c), -100.0f);
        levelDb[(size_t) c] = juce::jmax (db, levelDb[(size_t) c] - 0.6f);

        if (db >= holdDb[(size_t) c] || ++holdAge[(size_t) c] > 90)
        {
            holdDb[(size_t) c] = juce::jmax (db, levelDb[(size_t) c]);
            holdAge[(size_t) c] = 0;
        }
    }

    reductionDb = juce::jmax (processor.takeReduction(), reductionDb - 0.6f);
}

void MeterPanel::drawBar (juce::Graphics& g, juce::Rectangle<float> bar, float db, float hold, juce::Colour colour) const
{
    using namespace LimiterColours;
    constexpr float top = 0.0f, bottom = -48.0f;
    g.setColour (background);
    g.fillRoundedRectangle (bar, 3.0f);

    const auto level = juce::jmap (juce::jlimit (bottom, top, db), bottom, top, 0.0f, 1.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (bar.withTop (bar.getBottom() - bar.getHeight() * level), 3.0f);

    const auto holdY = bar.getBottom() - bar.getHeight() * juce::jmap (juce::jlimit (bottom, top, hold), bottom, top, 0.0f, 1.0f);
    g.setColour (text);
    g.drawHorizontalLine (juce::roundToInt (holdY), bar.getX(), bar.getRight());
}

void MeterPanel::paint (juce::Graphics& g)
{
    using namespace LimiterColours;
    auto bounds = getLocalBounds().toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    bounds.reduce (12.0f, 10.0f);

    auto meterArea = bounds.removeFromTop (bounds.getHeight() * 0.5f);
    auto labels = meterArea.removeFromBottom (32.0f);
    const auto barWidth = 16.0f;
    const auto gap = (meterArea.getWidth() - barWidth * 3.0f) / 4.0f;

    auto barL = meterArea.withX (meterArea.getX() + gap).withWidth (barWidth);
    auto barR = barL.translated (barWidth + gap, 0.0f);
    auto barGr = barR.translated (barWidth + gap, 0.0f);

    drawBar (g, barL, levelDb[0], holdDb[0], output);
    drawBar (g, barR, levelDb[1], holdDb[1], output);

    // Gain reduction hangs down from the top, 0 to 24 dB.
    g.setColour (background);
    g.fillRoundedRectangle (barGr, 3.0f);
    g.setColour (reduction);
    g.fillRoundedRectangle (barGr.withHeight (barGr.getHeight() * juce::jlimit (0.0f, 1.0f, reductionDb / 24.0f)), 3.0f);

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (dimText);
    g.drawText ("L", barL.withY (labels.getY()).withHeight (14.0f), juce::Justification::centred);
    g.drawText ("R", barR.withY (labels.getY()).withHeight (14.0f), juce::Justification::centred);
    g.drawText ("GR", barGr.withY (labels.getY()).withHeight (14.0f).expanded (6.0f, 0.0f), juce::Justification::centred);

    g.setColour (text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const auto outText = juce::jmax (holdDb[0], holdDb[1]) <= -99.0f ? juce::String ("--")
                                                                      : juce::String (juce::jmax (holdDb[0], holdDb[1]), 1);
    g.drawText (outText, juce::Rectangle<float> (barL.getX() - 6.0f, labels.getY() + 15.0f, barR.getRight() - barL.getX() + 12.0f, 16.0f),
                juce::Justification::centred);
    g.setColour (reduction);
    g.drawText (juce::String (-reductionDb, 1), juce::Rectangle<float> (barGr.getX() - 12.0f, labels.getY() + 15.0f, barWidth + 24.0f, 16.0f),
                juce::Justification::centred);

    // Loudness readouts
    bounds.removeFromTop (8.0f);
    const auto& loud = processor.getLoudness();
    const std::pair<const char*, juce::String> rows[] {
        { "Momentary", formatLoudness (loud.getMomentary()) + " LUFS" },
        { "Short-term", formatLoudness (loud.getShortTerm()) + " LUFS" },
        { "Integrated", formatLoudness (loud.getIntegrated()) + " LUFS" },
        { "Range", juce::String (loud.getRange(), 1) + " LU" },
        { "True peak", (processor.getTruePeakMaxDb() <= -99.0f ? juce::String ("--") : juce::String (processor.getTruePeakMaxDb(), 1)) + " dBTP" },
    };

    const auto rowHeight = juce::jmin (22.0f, (bounds.getHeight() - 34.0f) / 5.0f);
    for (const auto& [name, value] : rows)
    {
        auto row = bounds.removeFromTop (rowHeight);
        g.setFont (juce::FontOptions (11.5f));
        g.setColour (dimText);
        g.drawText (name, row, juce::Justification::centredLeft);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.setColour (juce::String (name) == "Integrated" ? accent : text);
        g.drawText (value, row, juce::Justification::centredRight);
    }
}

void MeterPanel::resized()
{
    resetButton.setBounds (getLocalBounds().reduced (12, 10).removeFromBottom (24));
}

//==============================================================================
CruzLimiterAudioProcessorEditor::CruzLimiterAudioProcessorEditor (CruzLimiterAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      display (p.getState()),
      meters (p),
      frameScratch (1024)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (display);
    addAndMakeVisible (meters);

    gainSlider.setSliderStyle (juce::Slider::LinearVertical);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    gainSlider.setTooltip ("Gain: how hard you push into the limiter. Raise it until the track is as loud as you want; "
                           "the red area shows how much is being held back.");
    gainAttachment = std::make_unique<SliderAttachment> (p.getState(), ParamIDs::gain, gainSlider);
    addAndMakeVisible (gainSlider);

    setUpMenu (style, ParamIDs::style, "Style",
               "The limiting character. Clean and Safe are the most transparent, Punch and Dynamic keep drums lively, "
               "Loud and Modern add gentle clipping for density, Bus is a light touch for groups.");
    setUpKnob (lookahead, ParamIDs::lookahead, "Lookahead",
               "How far ahead the limiter looks so it can turn down before a peak arrives. More is smoother, less is punchier.");
    setUpKnob (attack, ParamIDs::attack, "Attack",
               "How much of the lookahead time is used to turn the gain down. Low values let more transient snap through the shape.");
    setUpKnob (release, ParamIDs::release, "Release",
               "How quickly the gain comes back up after a peak. Short is louder but can pump or distort; long is smoother.");
    setUpKnob (linkTransients, ParamIDs::linkTransients, "Link Peaks",
               "How much left and right share fast peak limiting. 100% keeps the stereo image steady.");
    setUpKnob (linkRelease, ParamIDs::linkRelease, "Link Release",
               "How much left and right share the slower recovery. 100% keeps the stereo image steady.");
    setUpKnob (ceiling, ParamIDs::ceiling, "Ceiling",
               "The highest level the output can reach. -1 dB is a safe choice for streaming.");
    ceiling.slider.setDoubleClickReturnValue (true, -1.0);

    setUpMenu (oversampling, ParamIDs::oversampling, "Oversampling",
               "Runs the limiter at a higher internal rate for cleaner results. Costs more CPU. True Peak always uses at least 2x.");
    setUpMenu (dither, ParamIDs::dither, "Dither",
               "Adds tiny noise when exporting to a lower bit depth (16 bit for CD). Leave Off while mixing.");

    setUpToggle (truePeak, truePeakAttachment, ParamIDs::truePeak, "True Peak",
                 "Also catches peaks that happen between samples, so the ceiling holds after conversion to MP3 or streaming.");
    setUpToggle (noiseShaping, noiseShapingAttachment, ParamIDs::noiseShaping, "Shaped",
                 "Moves dither noise to frequencies you hear less. Only matters when Dither is on.");
    setUpToggle (dcFilter, dcFilterAttachment, ParamIDs::dcFilter, "DC Filter",
                 "Removes inaudible DC offset before limiting, which frees up a little headroom.");
    setUpToggle (unityGain, unityGainAttachment, ParamIDs::unityGain, "Unity Gain",
                 "Turns the output back down by the Gain amount so you can judge the sound of limiting without the loudness boost.");
    setUpToggle (audition, auditionAttachment, ParamIDs::audition, "Audition",
                 "Plays only what the limiter is removing, so you can hear if it is cutting too much.");

    setResizable (true, true);
    setResizeLimits (820, 520, 1800, 1100);
    setSize (980, 600);

    startTimerHz (60);
}

CruzLimiterAudioProcessorEditor::~CruzLimiterAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void CruzLimiterAudioProcessorEditor::setUpKnob (Knob& knob, const char* paramId, const juce::String& name, const juce::String& tip)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    knob.slider.setTooltip (tip);
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.attachment = std::make_unique<SliderAttachment> (processor.getState(), paramId, knob.slider);
    if (auto* param = processor.getState().getParameter (paramId))
        knob.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    addAndMakeVisible (knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (knob.label);
}

void CruzLimiterAudioProcessorEditor::setUpMenu (Menu& menu, const char* paramId, const juce::String& name, const juce::String& tip)
{
    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.getState().getParameter (paramId)))
        menu.box.addItemList (param->choices, 1);
    menu.box.setTooltip (tip);
    menu.box.setJustificationType (juce::Justification::centred);
    menu.attachment = std::make_unique<ComboAttachment> (processor.getState(), paramId, menu.box);
    addAndMakeVisible (menu.box);

    menu.label.setText (name, juce::dontSendNotification);
    menu.label.setJustificationType (juce::Justification::centred);
    menu.label.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (menu.label);
}

void CruzLimiterAudioProcessorEditor::setUpToggle (juce::ToggleButton& button, std::unique_ptr<ButtonAttachment>& attachment,
                                                   const char* paramId, const juce::String& name, const juce::String& tip)
{
    button.setButtonText (name);
    button.setTooltip (tip);
    attachment = std::make_unique<ButtonAttachment> (processor.getState(), paramId, button);
    addAndMakeVisible (button);
}

void CruzLimiterAudioProcessorEditor::timerCallback()
{
    const auto count = processor.readFrames (frameScratch.data(), (int) frameScratch.size());
    if (count > 0)
    {
        display.addFrames (frameScratch.data(), count);
        display.repaint();
    }

    meters.update();
    meters.repaint();

    if (processor.getLatencySamples() != shownLatency)
    {
        shownLatency = processor.getLatencySamples();
        repaint (getLocalBounds().removeFromTop (48));
    }
}

void CruzLimiterAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace LimiterColours;
    g.fillAll (background);

    auto header = getLocalBounds().reduced (16, 0).removeFromTop (44);
    g.setColour (text);
    g.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    g.drawText ("CRUZ", header, juce::Justification::centredLeft);
    g.setColour (accent);
    g.drawText ("LIMITER", header.withTrimmedLeft (64), juce::Justification::centredLeft);

    g.setColour (dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v" JucePlugin_VersionString "   latency " + juce::String (processor.getLatencySamples()) + " samples",
                header, juce::Justification::centredRight);

    // Panel behind the controls
    auto controls = getLocalBounds().reduced (12).removeFromBottom (148).toFloat();
    g.setColour (panel);
    g.fillRoundedRectangle (controls, 8.0f);
}

void CruzLimiterAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (34);

    auto controls = area.removeFromBottom (148).reduced (12, 8);
    area.removeFromBottom (10);

    meters.setBounds (area.removeFromRight (180));
    area.removeFromRight (10);
    display.setBounds (area);
    gainSlider.setBounds (area.getX() + 14, area.getY() + 12, 52, area.getHeight() - 24);

    // Bottom row: switches and menus
    auto switches = controls.removeFromBottom (28);
    const auto menuWidth = 96;
    auto placeMenu = [&switches, menuWidth] (Menu& menu)
    {
        auto slot = switches.removeFromLeft (menuWidth + 80);
        menu.label.setBounds (slot.removeFromLeft (80));
        menu.box.setBounds (slot.reduced (0, 2));
        switches.removeFromLeft (10);
    };
    placeMenu (oversampling);
    placeMenu (dither);

    const auto toggleWidth = juce::jmin (92, (switches.getWidth() - 4 * 6) / 5);
    for (auto* button : { &truePeak, &noiseShaping, &dcFilter, &unityGain, &audition })
    {
        button->setBounds (switches.removeFromLeft (toggleWidth).reduced (0, 2));
        switches.removeFromLeft (6);
    }

    controls.removeFromBottom (6);

    // Top row: style menu and knobs
    auto styleArea = controls.removeFromLeft (150);
    style.label.setBounds (styleArea.removeFromTop (18));
    style.box.setBounds (styleArea.withSizeKeepingCentre (styleArea.getWidth() - 16, 30));
    controls.removeFromLeft (8);

    Knob* knobs[] { &lookahead, &attack, &release, &linkTransients, &linkRelease, &ceiling };
    const auto knobWidth = controls.getWidth() / (int) std::size (knobs);
    for (auto* knob : knobs)
    {
        auto slot = controls.removeFromLeft (knobWidth);
        knob->label.setBounds (slot.removeFromTop (16));
        knob->slider.setBounds (slot);
    }
}
