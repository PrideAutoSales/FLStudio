#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff14161a };
    const juce::Colour panel { 0xff1d2026 };
    const juce::Colour text { 0xffe6e8eb };
    const juce::Colour dimText { 0xff8a9099 };
    const juce::Colour accent { 0xff4fc3a1 };
}

CruzEQAudioProcessorEditor::CruzEQAudioProcessorEditor (CruzEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      outputGainAttachment (p.getState(), ParamIDs::outputGain, outputGainSlider)
{
    outputGainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    outputGainSlider.setTextValueSuffix (" dB");
    outputGainSlider.setDoubleClickReturnValue (true, 0.0);
    outputGainSlider.setColour (juce::Slider::rotarySliderFillColourId, Colours::accent);
    outputGainSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (outputGainSlider);

    outputGainLabel.setText ("Output", juce::dontSendNotification);
    outputGainLabel.setJustificationType (juce::Justification::centred);
    outputGainLabel.setColour (juce::Label::textColourId, Colours::dimText);
    addAndMakeVisible (outputGainLabel);

    setResizable (true, true);
    setResizeLimits (640, 360, 1920, 1080);
    setSize (900, 500);
}

void CruzEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto area = getLocalBounds().reduced (16);
    auto header = area.removeFromTop (40);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Cruz EQ", header, juce::Justification::centredLeft);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("v" JucePlugin_VersionString, header, juce::Justification::centredRight);

    auto display = area.withTrimmedBottom (120).reduced (0, 8);
    g.setColour (Colours::panel);
    g.fillRoundedRectangle (display.toFloat(), 8.0f);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("Build pipeline working. The EQ engine and analyzer arrive in step 2.",
                display, juce::Justification::centred);
}

void CruzEQAudioProcessorEditor::resized()
{
    auto controls = getLocalBounds().reduced (16).removeFromBottom (112);
    auto knob = controls.withSizeKeepingCentre (110, 112);

    outputGainLabel.setBounds (knob.removeFromTop (18));
    outputGainSlider.setBounds (knob);
}
