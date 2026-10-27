#include "PluginEditor.h"

CruzVerbAudioProcessorEditor::CruzVerbAudioProcessorEditor (CruzVerbAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), display (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (display);

    using namespace ParamIDs;
    setUpKnob (knobs[0], space, "Space", " %", Theme::decay);
    setUpKnob (knobs[1], decay, "Decay", " s", Theme::decay);
    setUpKnob (knobs[2], predelay, "Pre-delay", " ms", Theme::decay);
    setUpKnob (knobs[3], distance, "Distance", " %", Theme::decay);
    setUpKnob (knobs[4], character, "Character", " %", Theme::decay);
    setUpKnob (knobs[5], brightness, "Brightness", " %", Theme::postEq);
    setUpKnob (knobs[6], width, "Width", " %", Theme::postEq);
    setUpKnob (knobs[7], ducking, "Ducking", " %", Theme::postEq);
    setUpKnob (knobs[8], mix, "Mix", " %", Theme::text);
    setUpKnob (knobs[9], outputGain, "Output", " dB", Theme::text);

    for (auto* b : { &decayModeButton, &postModeButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1);
        addAndMakeVisible (b);
    }

    decayModeButton.setColour (juce::TextButton::textColourOnId, Theme::decay);
    postModeButton.setColour (juce::TextButton::textColourOnId, Theme::postEq);
    decayModeButton.setToggleState (true, juce::dontSendNotification);
    decayModeButton.onClick = [this] { setDisplayMode (ReverbDisplay::Mode::decay); };
    postModeButton.onClick = [this] { setDisplayMode (ReverbDisplay::Mode::postEq); };

    freezeButton.setClickingTogglesState (true);
    freezeButton.setColour (juce::TextButton::textColourOnId, Theme::freeze);
    freezeButton.setTooltip ("Holds the current reverb tail forever and stops new sound from entering it.");
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.getState(), freeze, freezeButton);
    addAndMakeVisible (freezeButton);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        if (const auto id = presetBox.getSelectedId(); id > 0)
            processor.setCurrentProgram (id - 1);
    };
    refreshPresetBox();
    addAndMakeVisible (presetBox);

    setResizable (true, true);
    setResizeLimits (820, 480, 1800, 1100);
    setSize (980, 580);
}

CruzVerbAudioProcessorEditor::~CruzVerbAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzVerbAudioProcessorEditor::setUpKnob (Knob& k, const char* paramId, const juce::String& name,
                                              const juce::String& suffix, juce::Colour accent)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getState(), paramId, k.slider);
    k.slider.setTextValueSuffix (suffix);

    if (auto* param = processor.getState().getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));

    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void CruzVerbAudioProcessorEditor::refreshPresetBox()
{
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
}

void CruzVerbAudioProcessorEditor::setDisplayMode (ReverbDisplay::Mode m)
{
    display.setMode (m);
}

void CruzVerbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    auto header = getLocalBounds().reduced (16, 0).removeFromTop (52);

    g.setColour (Theme::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("CRUZ", header, juce::Justification::centredLeft);

    const auto cruzWidth = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), "CRUZ ");
    g.setColour (Theme::decay);
    g.drawText ("VERB", header.withTrimmedLeft (cruzWidth), juce::Justification::centredLeft);

    g.setColour (Theme::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v" JucePlugin_VersionString, header.withTrimmedLeft (cruzWidth + 70), juce::Justification::centredLeft);

    // Faint divider between the room controls and the output controls.
    if (knobs[8].slider.isVisible())
    {
        const auto x = (float) knobs[8].slider.getX() - 6.0f;
        g.setColour (Theme::panelEdge);
        g.drawVerticalLine (juce::roundToInt (x), (float) knobs[8].label.getY(), (float) knobs[8].slider.getBottom());
    }
}

void CruzVerbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    auto header = area.removeFromTop (52);

    freezeButton.setBounds (header.removeFromRight (90).withSizeKeepingCentre (90, 28));
    header.removeFromRight (10);
    presetBox.setBounds (header.removeFromRight (200).withSizeKeepingCentre (200, 28));

    area.removeFromBottom (12);
    auto controls = area.removeFromBottom (juce::jlimit (120, 170, getHeight() / 4));
    area.removeFromBottom (12);

    display.setBounds (area);

    auto modeArea = area.reduced (44, 18).removeFromTop (24).removeFromRight (180);
    postModeButton.setBounds (modeArea.removeFromRight (90));
    decayModeButton.setBounds (modeArea.removeFromRight (90));

    const auto knobWidth = controls.getWidth() / (int) knobs.size();

    for (auto& k : knobs)
    {
        auto slot = controls.removeFromLeft (knobWidth).reduced (4, 0);
        k.label.setBounds (slot.removeFromTop (18));
        k.slider.setBounds (slot);
    }
}
