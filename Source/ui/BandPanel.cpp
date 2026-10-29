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
    placeBox.addItemList (Params::placementNames(), 1);

    for (auto [label, slider] : knobs())
        setUpKnob (*slider);

    setUpLabel (shapeLabel, "Shape");
    setUpLabel (slopeLabel, "Slope");
    setUpLabel (placeLabel, "Stereo");
    setUpLabel (freqLabel, "Frequency");
    setUpLabel (gainLabel, "Gain");
    setUpLabel (qLabel, "Q");
    setUpLabel (rangeLabel, "Range");
    setUpLabel (thresholdLabel, "Threshold");
    setUpLabel (attackLabel, "Attack");
    setUpLabel (releaseLabel, "Release");

    dynamicButton.setTooltip ("Let this band's gain follow the music: it moves by up to Range while the sound around its frequency is above Threshold");
    thresholdSlider.setTooltip ("Turn fully up for Auto: the threshold follows the band's own level, so the band reacts to whatever stands out");

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

    for (auto* c : bandControls())
        addChildComponent (c);

    setBand (-1);
    startTimerHz (15);
}

std::vector<std::pair<juce::Label*, juce::Slider*>> BandPanel::knobs()
{
    return { { &freqLabel, &freqSlider }, { &gainLabel, &gainSlider }, { &qLabel, &qSlider },
             { &rangeLabel, &rangeSlider }, { &thresholdLabel, &thresholdSlider },
             { &attackLabel, &attackSlider }, { &releaseLabel, &releaseSlider } };
}

std::vector<juce::Component*> BandPanel::bandControls()
{
    std::vector<juce::Component*> result { &shapeBox, &slopeBox, &placeBox, &shapeLabel, &slopeLabel, &placeLabel,
                                           &bypassButton, &dynamicButton, &removeButton };

    for (auto [label, slider] : knobs())
    {
        result.push_back (label);
        result.push_back (slider);
    }

    return result;
}

void BandPanel::setBand (int index)
{
    shapeAttachment.reset();
    slopeAttachment.reset();
    placeAttachment.reset();
    freqAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    rangeAttachment.reset();
    thresholdAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    bypassAttachment.reset();
    dynamicAttachment.reset();

    band = index;
    const bool show = band >= 0;

    for (auto* c : bandControls())
        c->setVisible (show);

    hint.setVisible (! show);

    if (show)
    {
        auto& state = processor.getState();
        shapeAttachment = std::make_unique<ComboBoxAttachment> (state, Params::bandId (band, BandField::shape), shapeBox);
        slopeAttachment = std::make_unique<ComboBoxAttachment> (state, Params::bandId (band, BandField::slope), slopeBox);
        placeAttachment = std::make_unique<ComboBoxAttachment> (state, Params::bandId (band, BandField::place), placeBox);
        freqAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::freq), freqSlider);
        gainAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::gain), gainSlider);
        qAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::q), qSlider);
        rangeAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::range), rangeSlider);
        thresholdAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::threshold), thresholdSlider);
        attackAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::attack), attackSlider);
        releaseAttachment = std::make_unique<SliderAttachment> (state, Params::bandId (band, BandField::release), releaseSlider);
        bypassAttachment = std::make_unique<ButtonAttachment> (state, Params::bandId (band, BandField::bypass), bypassButton);
        dynamicAttachment = std::make_unique<ButtonAttachment> (state, Params::bandId (band, BandField::dynamic), dynamicButton);

        const auto colour = Theme::band (band);
        title.setText ("Band " + juce::String (band + 1), juce::dontSendNotification);
        title.setColour (juce::Label::textColourId, colour);

        for (auto [label, slider] : knobs())
        {
            slider->setColour (juce::Slider::rotarySliderFillColourId, colour);
            slider->setColour (juce::Slider::thumbColourId, colour);
        }

        freqSlider.setDoubleClickReturnValue (true, 1000.0);
        gainSlider.setDoubleClickReturnValue (true, 0.0);
        qSlider.setDoubleClickReturnValue (true, 1.0);
        rangeSlider.setDoubleClickReturnValue (true, -6.0);
        thresholdSlider.setDoubleClickReturnValue (true, cruz::dsp::maxThresholdDb); // Auto
        attackSlider.setDoubleClickReturnValue (true, 10.0);
        releaseSlider.setDoubleClickReturnValue (true, 150.0);
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
    qSlider.setEnabled (cruz::dsp::shapeUsesQ (shape));
    gainLabel.setEnabled (gainSlider.isEnabled());
    slopeLabel.setEnabled (slopeBox.isEnabled());
    qLabel.setEnabled (qSlider.isEnabled());

    const bool canBeDynamic = cruz::dsp::shapeUsesDynamics (shape);
    const bool dynamic = canBeDynamic && dynamicButton.getToggleState();
    dynamicButton.setEnabled (canBeDynamic);

    for (auto [label, slider] : { std::pair { &rangeLabel, &rangeSlider }, std::pair { &thresholdLabel, &thresholdSlider },
                                  std::pair { &attackLabel, &attackSlider }, std::pair { &releaseLabel, &releaseSlider } })
    {
        slider->setEnabled (dynamic);
        label->setEnabled (dynamic);
    }
}

void BandPanel::paint (juce::Graphics& g)
{
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
}

void BandPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    auto left = area.removeFromLeft (130);

    title.setBounds (left.removeFromTop (24));

    if (band < 0)
    {
        hint.setBounds (area);
        return;
    }

    left.removeFromTop (4);
    bypassButton.setBounds (left.removeFromTop (24));
    dynamicButton.setBounds (left.removeFromTop (24));
    left.removeFromTop (6);
    removeButton.setBounds (left.removeFromTop (24).withWidth (90));

    auto combos = area.removeFromLeft (150);

    for (auto [label, box] : { std::pair { &shapeLabel, &shapeBox }, std::pair { &slopeLabel, &slopeBox }, std::pair { &placeLabel, &placeBox } })
    {
        label->setBounds (combos.removeFromTop (14));
        box->setBounds (combos.removeFromTop (22));
        combos.removeFromTop (4);
    }

    area.removeFromLeft (12);
    constexpr int gap = 18;
    const int knobWidth = juce::jmin (100, (area.getWidth() - gap) / 7);
    int index = 0;

    for (auto [label, slider] : knobs())
    {
        // The four dynamics knobs sit in their own group on the right.
        if (index++ == 3)
        {
            area.removeFromLeft (gap);
            dynamicArea = area.withWidth (knobWidth * 4).expanded (4, 6);
        }

        auto column = area.removeFromLeft (knobWidth);
        label->setBounds (column.removeFromTop (16));
        slider->setBounds (column);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, knobWidth, 18);
    }
}

void BandPanel::paintOverChildren (juce::Graphics& g)
{
    if (band < 0 || dynamicArea.isEmpty())
        return;

    // A thin frame groups the dynamics knobs.
    g.setColour (Theme::gridMajor);
    g.drawRoundedRectangle (dynamicArea.toFloat(), 5.0f, 1.0f);
}
