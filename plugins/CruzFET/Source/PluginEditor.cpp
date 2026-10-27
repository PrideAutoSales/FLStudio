#include "PluginEditor.h"

namespace
{
cruz::Palette makePalette()
{
    // Graphite panel, orange accents, cool white meter.
    return { juce::Colour (0xff2b2d31), juce::Colour (0xff17181b), juce::Colour (0xffe6e8ec), juce::Colour (0xffff7a1a),
             juce::Colour (0xff111214), juce::Colour (0xff4a4e57), juce::Colour (0xffeef1f4), juce::Colour (0xff1c1f24) };
}
} // namespace

CruzFETAudioProcessorEditor::CruzFETAudioProcessorEditor (CruzFETAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      palette (makePalette()),
      lookAndFeel (palette),
      meter (palette),
      inputKnob (p.getState(), ParamIDs::input, "INPUT"),
      outputKnob (p.getState(), ParamIDs::output, "OUTPUT"),
      attackKnob (p.getState(), ParamIDs::attack, "ATTACK"),
      releaseKnob (p.getState(), ParamIDs::release, "RELEASE"),
      mixKnob (p.getState(), ParamIDs::mix, "MIX"),
      ratioButtons (*p.getState().getParameter (ParamIDs::ratio), { "4", "8", "12", "20", "ALL" }, true),
      meterButtons (*p.getState().getParameter (ParamIDs::meter), { "GR", "OUTPUT" }, false)
{
    setLookAndFeel (&lookAndFeel);

    meterButtons.onChange = [this] (int i)
    { meter.setMode (i == 0 ? cruz::NeedleMeter::Mode::gainReduction : cruz::NeedleMeter::Mode::output); };

    for (juce::Component* c : { (juce::Component*) &meter, (juce::Component*) &inputKnob, (juce::Component*) &outputKnob,
                                (juce::Component*) &attackKnob, (juce::Component*) &releaseKnob, (juce::Component*) &mixKnob,
                                (juce::Component*) &ratioButtons, (juce::Component*) &meterButtons })
        addAndMakeVisible (c);

    for (auto* l : { &inputKnob.label, &outputKnob.label, &attackKnob.label, &releaseKnob.label, &mixKnob.label })
        l->setColour (juce::Label::textColourId, palette.text);

    setSize (820, 400);
    startTimerHz (30);
}

CruzFETAudioProcessorEditor::~CruzFETAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzFETAudioProcessorEditor::timerCallback()
{
    const bool showGr = processor.getState().getRawParameterValue (ParamIDs::meter)->load() < 0.5f;
    meter.setTargetDb (showGr ? processor.gainReductionDb.load() : processor.outputDb.load());
}

void CruzFETAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient bg (palette.panel.brighter (0.1f), 0.0f, 0.0f, palette.panel.darker (0.3f), 0.0f, r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillAll();

    // Thin accent stripe along the top.
    g.setColour (palette.accent);
    g.fillRect (0.0f, 0.0f, r.getWidth(), 4.0f);

    g.setColour (palette.text);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("CRUZ FET", 24, 14, 300, 30, juce::Justification::left);
    g.setColour (palette.accent);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("PEAK LIMITING AMPLIFIER", 26, 42, 300, 16, juce::Justification::left);

    // Caption above the ratio switch.
    g.setColour (palette.text.withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    g.drawText ("RATIO", ratioButtons.getBounds().withY (ratioButtons.getY() - 20).withHeight (18), juce::Justification::centred);
}

void CruzFETAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (20, 12);
    r.removeFromTop (54);

    auto top = r.removeFromTop (150);
    inputKnob.setBounds (top.removeFromLeft (150));
    outputKnob.setBounds (top.removeFromRight (150));
    meter.setBounds (top.reduced (16, 0));

    r.removeFromTop (10);
    auto bottom = r;
    auto ratioArea = bottom.removeFromRight (90);
    ratioButtons.setBounds (ratioArea.withTrimmedTop (20).withTrimmedBottom (4));
    meterButtons.setBounds (bottom.removeFromRight (170).withSizeKeepingCentre (160, 30));
    const int w = bottom.getWidth() / 3;
    attackKnob.setBounds (bottom.removeFromLeft (w));
    releaseKnob.setBounds (bottom.removeFromLeft (w));
    mixKnob.setBounds (bottom);
}
