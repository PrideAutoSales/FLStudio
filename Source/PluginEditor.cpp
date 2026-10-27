#include "PluginEditor.h"
#include "ui/Theme.h"

namespace
{
constexpr float displayRanges[] { 3.0f, 6.0f, 12.0f, 30.0f };
const juce::Identifier rangeProperty { "displayRangeDb" };
} // namespace

CruzEQAudioProcessorEditor::CruzEQAudioProcessorEditor (CruzEQAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      display (p),
      bandPanel (p),
      outputGainAttachment (p.getState(), Params::outputGain, outputGainSlider)
{
    lookAndFeel.setColourScheme ({ Theme::background, Theme::panel, Theme::panel, Theme::dimText, Theme::text, Theme::accent,
                                   juce::Colours::black, Theme::accent.withAlpha (0.4f), Theme::text });
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, Theme::display);
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, Theme::panel);
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (display);
    addAndMakeVisible (bandPanel);

    display.onSelectionChanged = [this] (int band) { bandPanel.setBand (band); };
    bandPanel.onRemove = [this]
    {
        display.setSelectedBand (-1);
        bandPanel.setBand (-1);
    };

    for (float r : displayRanges)
        rangeBox.addItem ("+/- " + juce::String ((int) r) + " dB", (int) r);

    auto& tree = processor.getState().state;
    const float savedRange = (float) tree.getProperty (rangeProperty, 12.0f);
    rangeBox.setSelectedId ((int) savedRange, juce::dontSendNotification);
    display.setRangeDb (savedRange);
    rangeBox.onChange = [this]
    {
        const float range = (float) rangeBox.getSelectedId();
        display.setRangeDb (range);
        processor.getState().state.setProperty (rangeProperty, range, nullptr);
    };
    addAndMakeVisible (rangeBox);

    rangeLabel.setText ("Scale", juce::dontSendNotification);
    rangeLabel.setColour (juce::Label::textColourId, Theme::dimText);
    rangeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (rangeLabel);

    outputGainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    outputGainSlider.setTextValueSuffix (" dB");
    outputGainSlider.setDoubleClickReturnValue (true, 0.0);
    outputGainSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (outputGainSlider);

    outputGainLabel.setText ("Output", juce::dontSendNotification);
    outputGainLabel.setJustificationType (juce::Justification::centred);
    outputGainLabel.setColour (juce::Label::textColourId, Theme::dimText);
    addAndMakeVisible (outputGainLabel);

    setResizable (true, true);
    setResizeLimits (760, 460, 2400, 1400);
    setSize (980, 600);
}

CruzEQAudioProcessorEditor::~CruzEQAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void CruzEQAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Theme::background);

    auto header = getLocalBounds().reduced (14, 0).removeFromTop (40);

    g.setColour (Theme::text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("Cruz EQ", header, juce::Justification::centredLeft);

    g.setColour (Theme::dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("v" JucePlugin_VersionString, header.withTrimmedLeft (92), juce::Justification::centredLeft);
}

void CruzEQAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12, 0);
    auto header = area.removeFromTop (40);

    rangeBox.setBounds (header.removeFromRight (120).withSizeKeepingCentre (120, 24));
    rangeLabel.setBounds (header.removeFromRight (60));

    area.removeFromBottom (12);
    auto bottom = area.removeFromBottom (130);
    area.removeFromBottom (10);
    display.setBounds (area);

    auto output = bottom.removeFromRight (110);
    outputGainLabel.setBounds (output.removeFromTop (20));
    outputGainSlider.setBounds (output);
    bottom.removeFromRight (10);
    bandPanel.setBounds (bottom);
}
