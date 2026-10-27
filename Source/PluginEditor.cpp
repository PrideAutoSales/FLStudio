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
      autoGainAttachment (p.getState(), Params::autoGain, autoGainButton),
      outputGainAttachment (p.getState(), Params::outputGain, outputGainSlider),
      gainScaleAttachment (p.getState(), Params::gainScale, gainScaleSlider)
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

    rangeLabel.setText ("Range", juce::dontSendNotification);
    rangeLabel.setColour (juce::Label::textColourId, Theme::dimText);
    rangeLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (rangeLabel);

    autoGainButton.setTooltip ("Keeps the overall level about the same while you boost or cut");
    addAndMakeVisible (autoGainButton);

    phaseModeBox.addItemList (Params::phaseModeNames(), 1);
    phaseModeBox.setTooltip ("Zero Latency: no delay. Natural Phase: analog-like sound with a few ms of delay. "
                             "Linear Phase: no phase shift at all, with more delay at higher resolutions");
    resolutionBox.addItemList (Params::resolutionNames(), 1);
    resolutionBox.setTooltip ("Linear phase resolution: higher keeps narrow bass bands exact, but adds more delay and uses more CPU");
    phaseModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getState(), Params::phaseMode, phaseModeBox);
    resolutionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.getState(), Params::resolution, resolutionBox);
    addAndMakeVisible (phaseModeBox);
    addAndMakeVisible (resolutionBox);

    latencyLabel.setColour (juce::Label::textColourId, Theme::dimText);
    latencyLabel.setJustificationType (juce::Justification::centredLeft);
    latencyLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (latencyLabel);

    for (auto [slider, label, text] : { std::tuple { &outputGainSlider, &outputGainLabel, "Output" },
                                        std::tuple { &gainScaleSlider, &gainScaleLabel, "Gain Scale" } })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (slider);

        label->setText (text, juce::dontSendNotification);
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, Theme::dimText);
        addAndMakeVisible (label);
    }

    outputGainSlider.setTextValueSuffix (" dB");
    outputGainSlider.setDoubleClickReturnValue (true, 0.0);
    gainScaleSlider.setDoubleClickReturnValue (true, 100.0);

    setResizable (true, true);
    setResizeLimits (960, 500, 2400, 1400);
    setSize (1120, 640);
    startTimerHz (10);
}

void CruzEQAudioProcessorEditor::timerCallback()
{
    // Show how much auto gain is turning the output up or down.
    juce::String text ("Auto gain");

    if (autoGainButton.getToggleState())
    {
        const float db = processor.getAutoGainDb();
        text << "  " << (db > 0.0f ? "+" : "") << juce::String (db, 1) << " dB";
    }

    if (autoGainButton.getButtonText() != text)
        autoGainButton.setButtonText (text);

    resolutionBox.setEnabled (phaseModeBox.getSelectedItemIndex() == (int) cruz::dsp::PhaseMode::linear);

    const double ms = 1000.0 * processor.getLatencySamples() / juce::jmax (1.0, processor.getCurrentSampleRate());
    const auto latency = "Latency " + (ms < 10.0 ? juce::String (ms, 1) : juce::String (juce::roundToInt (ms))) + " ms";
    if (latencyLabel.getText() != latency)
        latencyLabel.setText (latency, juce::dontSendNotification);
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
    header.removeFromRight (16);
    autoGainButton.setBounds (header.removeFromRight (160).withSizeKeepingCentre (160, 24));
    header.removeFromRight (12);
    latencyLabel.setBounds (header.removeFromRight (100));
    resolutionBox.setBounds (header.removeFromRight (104).withSizeKeepingCentre (104, 24));
    header.removeFromRight (6);
    phaseModeBox.setBounds (header.removeFromRight (136).withSizeKeepingCentre (136, 24));

    area.removeFromBottom (12);
    auto bottom = area.removeFromBottom (140);
    area.removeFromBottom (10);
    display.setBounds (area);

    for (auto [slider, label] : { std::pair { &outputGainSlider, &outputGainLabel }, std::pair { &gainScaleSlider, &gainScaleLabel } })
    {
        auto column = bottom.removeFromRight (100);
        label->setBounds (column.removeFromTop (20));
        slider->setBounds (column);
    }

    bottom.removeFromRight (10);
    bandPanel.setBounds (bottom);
}
