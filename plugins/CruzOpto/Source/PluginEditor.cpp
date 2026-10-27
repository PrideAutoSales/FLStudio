#include "PluginEditor.h"

namespace
{
cruz::Palette makePalette()
{
    // Deep teal panel, brass knobs, warm ivory meter.
    return { juce::Colour (0xff1f3b3a), juce::Colour (0xff122524), juce::Colour (0xffe9e2cf), juce::Colour (0xffe0a33a),
             juce::Colour (0xff2a2a2a), juce::Colour (0xffb8934a), juce::Colour (0xfff2e6c4), juce::Colour (0xff2b2418) };
}
} // namespace

CruzOptoAudioProcessorEditor::CruzOptoAudioProcessorEditor (CruzOptoAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      palette (makePalette()),
      lookAndFeel (palette),
      meter (palette),
      gainKnob (p.getState(), ParamIDs::gain, "GAIN"),
      peakKnob (p.getState(), ParamIDs::peakReduction, "PEAK REDUCTION"),
      emphasisKnob (p.getState(), ParamIDs::emphasis, "HF EMPHASIS"),
      mixKnob (p.getState(), ParamIDs::mix, "MIX"),
      modeButtons (*p.getState().getParameter (ParamIDs::mode), { "COMPRESS", "LIMIT" }, false),
      meterButtons (*p.getState().getParameter (ParamIDs::meter), { "GR", "OUTPUT" }, false)
{
    setLookAndFeel (&lookAndFeel);

    meterButtons.onChange = [this] (int i)
    { meter.setMode (i == 0 ? cruz::NeedleMeter::Mode::gainReduction : cruz::NeedleMeter::Mode::output); };

    for (juce::Component* c : { (juce::Component*) &meter, (juce::Component*) &gainKnob, (juce::Component*) &peakKnob,
                                (juce::Component*) &emphasisKnob, (juce::Component*) &mixKnob,
                                (juce::Component*) &modeButtons, (juce::Component*) &meterButtons })
        addAndMakeVisible (c);

    for (auto* l : { &gainKnob.label, &peakKnob.label, &emphasisKnob.label, &mixKnob.label })
        l->setColour (juce::Label::textColourId, palette.text);

    setSize (720, 380);
    startTimerHz (30);
}

CruzOptoAudioProcessorEditor::~CruzOptoAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzOptoAudioProcessorEditor::timerCallback()
{
    const bool showGr = processor.getState().getRawParameterValue (ParamIDs::meter)->load() < 0.5f;
    meter.setTargetDb (showGr ? processor.gainReductionDb.load() : processor.outputDb.load());
}

void CruzOptoAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient bg (palette.panel.brighter (0.12f), 0.0f, 0.0f, palette.panel.darker (0.25f), 0.0f, r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Rack ears with screws.
    g.setColour (palette.panelEdge);
    g.fillRect (r.removeFromLeft (22.0f));
    g.fillRect (r.removeFromRight (22.0f));
    g.setColour (palette.knobCap.withAlpha (0.8f));
    for (float x : { 11.0f, (float) getWidth() - 11.0f })
        for (float y : { 24.0f, (float) getHeight() - 24.0f })
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);

    g.setColour (palette.text);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("CRUZ OPTO", 40, 14, 300, 30, juce::Justification::left);
    g.setColour (palette.accent);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("OPTICAL LEVELING AMPLIFIER", 42, 42, 300, 16, juce::Justification::left);
}

void CruzOptoAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (34, 12);
    r.removeFromTop (54);

    auto main = r.removeFromTop (230);
    gainKnob.setBounds (main.removeFromLeft (180));
    peakKnob.setBounds (main.removeFromRight (180));
    meter.setBounds (main.reduced (12, 6));

    r.removeFromTop (8);
    auto bottom = r;
    modeButtons.setBounds (bottom.removeFromLeft (180).withSizeKeepingCentre (176, 30));
    meterButtons.setBounds (bottom.removeFromRight (180).withSizeKeepingCentre (176, 30));
    emphasisKnob.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2));
    mixKnob.setBounds (bottom);
}
