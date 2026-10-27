#include "BandPanel.h"
#include "Theme.h"

using Params::BandField;

namespace
{
void setUpKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 18);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void setUpLabel (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setColour (juce::Label::textColourId, Theme::dimText);
    l.setFont (juce::FontOptions (12.0f));
}
} // namespace

BandPanel::BandPanel (CruzEQAudioProcessor& p)
    : processor (p)
{
    title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    title.setColour (juce::Label::textColourId, Theme::text);
    addAndMakeVisible (title);

    hint.setText ("Double-click the display to add a band, then click a dot to edit it here.", juce::dontSendNotification);
    hint.setColour (juce::Label::textColourId, Theme::dimText);
    hint.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hint);

    shapeBox.addItemList (Params::shapeNames(), 1);
    slopeBox.addItemList (Params::slopeNames(), 1);

    for (auto* s : { &freqSlider, &gainSlider, &qSlider })
        setUpKnob (*s);

    setUpLabel (shapeLabel, "Shape");
    setUpLabel (slopeLabel, "Slope");
    setUpLabel (freqLabel, "Frequency");
    setUpLabel (gainLabel, "Gain");
    setUpLabel (qLabel, "Q");

    removeButton.onClick = [this]
    {
        if (band < 0)
            return;

        auto& used = *processor.getState().getParameter (Params::bandId (band, BandField::used));
        used.beginChangeGesture();
        used.setValueNotifyingHost (0.0f);
        used.endChangeGesture();

        if (onRemove != nullptr)
            onRemove();
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> { &shapeBox, &slopeBox, &freqSlider, &gainSlider, &qSlider,
                                                                          &shapeLabel, &slopeLabel, &freqLabel, &gainLabel, &qLabel,
                                                                          &bypassButton, &removeButton })
        addChildComponent (c);

    setBand (-1);
    startTimerHz (15);
}

void BandPanel::setBand (int index)
{
    shapeAttachment.reset();
    slopeAttachment.reset();
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    bypassAttachment.reset();

    band = index;
    const bool show = band >= 0;

    for (juce::Component* c : std::initializer_list<juce::Component*> { &shapeBox, &slopeBox, &freqSlider, &gainSlider, &qSlider,
                                                                          &shapeLabel, &slopeLabel, &freqLabel, &gainLabel, &qLabel,
                                                                          &bypassButton, &removeButton })
        c->setVisible (show);

    hint.setVisible (! show);

    if (show)
    {
        auto& state = processor.getState();
        shapeAttachment = std::make_unique<ComboBoxAttachment> (state, Params::bandId (band, BandField::shape), shapeBox);
        slopeAttachment = std::make_unique<ComboBoxAttachment> (state, Params::bandId (band, BandField::slope), slopeBox);
        freqAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::freq), freqSlider);
        gainAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::gain), gainSlider);
        qAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::q), qSlider);
        bypassAttachment = std::make_unique<ButtonAttachment> (state, Params::bandId (band, BandField::bypass), bypassButton);

        const auto colour = Theme::band (band);
        title.setText ("Band " + juce::String (band + 1), juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, colour);

        for (auto* s : { &freqSlider, &gainSlider, &qSlider })
        {
            s->setColour (juce::Slider::rotarySliderFillColourId, colour);
            s->setColour (juce::Slider::thumbColourId, colour);
        }

        freqSlider.setDoubleClickReturnValue (true, 1000.0);
        gainSlider.setDoubleClickReturnValue (true, 0.0);
        qSlider.setDoubleClickReturnValue (true, 1.0);
    }
    else
    {
        title.setText ("No band selected", juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, Theme::text);
    }

    updateEnablement();
    resized();
}

void BandPanel::timerCallback()
{
    updateEnablement();
}

void BandPanel::updateEnablement()
{
    if (band < 0)
        return;

    const auto shape = processor.getBand (band).read().shape;
    gainSlider.setEnabled (cruz::dsp::shapeUsesGain (shape));
    slopeBox.setEnabled (cruz::dsp::shapeUsesSlope (shape));
    gainLabel.setEnabled (gainSlider.isEnabled());
    slopeLabel.setEnabled (slopeBox.isEnabled());
}

void BandPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    auto left = area.removeFromLeft (150);

    title.setBounds (left.removeFromTop (24));

    if (band < 0)
    {
        hint.setBounds (area);
        return;
    }

    left.removeFromTop (6);
    bypassButton.setBounds (left.removeFromTop (24));
    left.removeFromTop (6);
    removeButton.setBounds (left.removeFromTop (24).withWidth (90));

    auto combos = area.removeFromLeft (150);
    shapeLabel.setBounds (combos.removeFromTop (16));
    shapeBox.setBounds (combos.removeFromTop (26));
    combos.removeFromTop (8);
    slopeLabel.setBounds (combos.removeFromTop (16));
    slopeBox.setBounds (combos.removeFromTop (26));

    area.removeFromLeft (12);
    const int knobWidth = juce::jmin (110, area.getWidth() / 3);

    for (auto [label, slider] : { std::pair { &freqLabel, &freqSlider }, std::pair { &gainLabel, &gainSlider }, std::pair { &qLabel, &qSlider } })
    {
        auto column = area.removeFromLeft (knobWidth);
        label->setBounds (column.removeFromTop (16));
        slider->setBounds (column);
    }
}
