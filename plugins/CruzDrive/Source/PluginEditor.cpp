#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff15110e };
    const juce::Colour panel { 0xff211a15 };
    const juce::Colour panelEdge { 0xff3a2d23 };
    const juce::Colour well { 0xff0f0c0a };
    const juce::Colour text { 0xfff3e7da };
    const juce::Colour dimText { 0xff9d8b7b };
    const juce::Colour heat { 0xffff7a2f };
    const juce::Colour glow { 0xffffb347 };
    const juce::Colour crush { 0xffe5483f };
    const juce::Colour cool { 0xff8fb8c9 };
}

namespace
{
    class DriveLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        DriveLookAndFeel()
        {
            setColour (juce::Label::textColourId, Colours::dimText);
            setColour (juce::Slider::textBoxTextColourId, Colours::text);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
            setColour (juce::ComboBox::backgroundColourId, Colours::well);
            setColour (juce::ComboBox::outlineColourId, Colours::panelEdge);
            setColour (juce::ComboBox::textColourId, Colours::text);
            setColour (juce::ComboBox::arrowColourId, Colours::heat);
            setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
            setColour (juce::PopupMenu::textColourId, Colours::text);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::heat.withAlpha (0.3f));
            setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
            setColour (juce::TextButton::textColourOffId, Colours::dimText);
            setColour (juce::TextButton::textColourOnId, Colours::background);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
            const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
            const auto centre = bounds.getCentre();
            const auto arcRadius = radius - 3.0f;
            const auto trackWidth = juce::jmax (3.0f, radius * 0.09f);
            const auto angle = startAngle + pos * (endAngle - startAngle);

            juce::Path track;
            track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
            g.setColour (Colours::well);
            g.strokePath (track, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

            // Bipolar knobs (like Tone) fill from the centre.
            const auto bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
            const auto fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
            if (std::abs (angle - fromAngle) > 0.01f)
            {
                juce::Path value;
                value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                     juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
                g.setColour (accent.withAlpha (0.25f));
                g.strokePath (value, { trackWidth * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
                g.setColour (accent);
                g.strokePath (value, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });
            }

            const auto bodyRadius = arcRadius - trackWidth * 1.8f;
            const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b2f26), body.getX(), body.getY(),
                                                     juce::Colour (0xff1c1612), body.getRight(), body.getBottom(), false));
            g.fillEllipse (body);
            g.setColour (Colours::panelEdge.brighter (0.2f));
            g.drawEllipse (body, 1.0f);

            const auto tip = centre.getPointOnCircumference (bodyRadius * 0.82f, angle);
            const auto base = centre.getPointOnCircumference (bodyRadius * 0.35f, angle);
            g.setColour (Colours::text);
            g.drawLine ({ base, tip }, juce::jmax (2.0f, radius * 0.06f));
        }

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                   bool isHighlighted, bool isDown) override
        {
            const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
            const auto corner = bounds.getHeight() * 0.5f;
            const auto accent = button.findColour (juce::TextButton::buttonOnColourId);

            if (button.getToggleState())
            {
                g.setColour (accent.withAlpha (0.25f));
                g.fillRoundedRectangle (bounds.expanded (1.0f), corner + 1.0f);
                g.setColour (accent);
                g.fillRoundedRectangle (bounds, corner);
            }
            else
            {
                g.setColour (Colours::well.brighter (isHighlighted ? 0.12f : 0.0f).darker (isDown ? 0.2f : 0.0f));
                g.fillRoundedRectangle (bounds, corner);
                g.setColour (Colours::panelEdge);
                g.drawRoundedRectangle (bounds, corner, 1.0f);
            }
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::FontOptions (juce::jmin (14.0f, buttonHeight * 0.48f), juce::Font::bold);
        }

        juce::Label* createSliderTextBox (juce::Slider& slider) override
        {
            auto* l = LookAndFeel_V4::createSliderTextBox (slider);
            l->setFont (juce::FontOptions (13.0f));
            l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
            l->setColour (juce::Label::outlineWhenEditingColourId, Colours::heat);
            l->setColour (juce::Label::backgroundWhenEditingColourId, Colours::well);
            return l;
        }
    };

    void drawPanel (juce::Graphics& g, juce::Rectangle<int> area, const juce::String& title, juce::Colour accent)
    {
        if (area.isEmpty())
            return;

        const auto r = area.toFloat();
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        auto titleArea = area.reduced (14, 8).removeFromTop (18);
        g.setColour (accent);
        g.fillRoundedRectangle (titleArea.removeFromLeft (4).toFloat().reduced (0.0f, 3.0f), 2.0f);
        titleArea.removeFromLeft (8);
        g.setColour (Colours::dimText);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (title, titleArea, juce::Justification::centredLeft);
    }
}

namespace DriveUI
{
    //==============================================================================
    Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                const juce::String& caption, juce::Colour accent, bool large)
        : attachment (state, paramID, slider), isLarge (large)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f, true);
        if (auto* p = state.getParameter (paramID))
            slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        addAndMakeVisible (slider);

        label.setText (caption.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (large ? 14.0f : 11.5f, juce::Font::bold));
        addAndMakeVisible (label);
    }

    void Knob::resized()
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (isLarge ? 20 : 16));
        slider.setBounds (r);
    }

    //==============================================================================
    Switch::Switch (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                    const juce::String& caption, juce::Colour accent)
        : juce::TextButton (caption), attachment (state, paramID, *this)
    {
        setClickingTogglesState (true);
        setColour (juce::TextButton::buttonOnColourId, accent);
    }

    //==============================================================================
    Meter::Meter (juce::String c, bool h, bool isReduction)
        : caption (std::move (c)), horizontal (h), reduction (isReduction) {}

    void Meter::setLevel (float newValue)
    {
        value = newValue;
        // Fast rise, slower fall, so peaks stay readable.
        shown = value > shown ? value : shown + (value - shown) * 0.25f;
        repaint();
    }

    void Meter::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        auto captionArea = horizontal ? r.removeFromLeft (24.0f) : r.removeFromBottom (16.0f);

        g.setColour (Colours::dimText);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText (caption, captionArea, juce::Justification::centred);

        g.setColour (Colours::well);
        g.fillRoundedRectangle (r, 3.0f);

        // Level meters show -48..+6 dBFS; the reduction meter shows 0..30 dB.
        const auto fraction = reduction
            ? juce::jlimit (0.0f, 1.0f, shown / 30.0f)
            : juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (shown, -60.0f) + 48.0f) / 54.0f);

        if (fraction <= 0.0f)
            return;

        auto fill = r.reduced (2.0f);
        if (horizontal)
            fill = reduction ? fill.removeFromRight (fill.getWidth() * fraction)
                             : fill.removeFromLeft (fill.getWidth() * fraction);
        else
            fill = reduction ? fill.removeFromTop (fill.getHeight() * fraction)
                             : fill.removeFromBottom (fill.getHeight() * fraction);

        if (reduction)
            g.setColour (Colours::crush);
        else
            g.setGradientFill (juce::ColourGradient::vertical (Colours::crush, r.getY(), Colours::glow.darker (0.3f), r.getBottom()));
        g.fillRoundedRectangle (fill, 2.0f);
    }

    //==============================================================================
    CurveDisplay::CurveDisplay (CruzDriveProcessor& p) : processor (p) {}

    void CurveDisplay::refresh()
    {
        level = level * 0.7f + processor.getEngine().meterIn.load() * 0.3f;
        repaint();
    }

    void CurveDisplay::paint (juce::Graphics& g)
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (Colours::well);
        g.fillRoundedRectangle (r, 8.0f);

        const auto plot = r.reduced (16.0f);
        const auto toScreen = [plot] (float x, float y)
        {
            return juce::Point<float> (plot.getCentreX() + x * plot.getWidth() * 0.5f,
                                       plot.getCentreY() - y * plot.getHeight() * 0.5f);
        };

        // Grid and the unity line.
        g.setColour (Colours::panelEdge.withAlpha (0.6f));
        for (int i = 1; i < 4; ++i)
        {
            const auto t = (float) i / 4.0f;
            g.drawHorizontalLine (juce::roundToInt (plot.getY() + t * plot.getHeight()), plot.getX(), plot.getRight());
            g.drawVerticalLine (juce::roundToInt (plot.getX() + t * plot.getWidth()), plot.getY(), plot.getBottom());
        }
        g.setColour (Colours::dimText.withAlpha (0.35f));
        g.drawLine ({ toScreen (-1.0f, -1.0f), toScreen (1.0f, 1.0f) }, 1.0f);

        auto& state = processor.getState();
        const auto style = juce::roundToInt (state.getRawParameterValue (DriveParams::style)->load());
        const auto punish = state.getRawParameterValue (DriveParams::punish)->load() > 0.5f;
        const auto driveDb = state.getRawParameterValue (DriveParams::drive)->load() * 3.6f
                             + (punish ? 20.0f : 0.0f)
                             + cruzdrive::voicings[(size_t) juce::jlimit (0, cruzdrive::numStyles - 1, style)].extraDriveDb;
        const auto gain = juce::Decibels::decibelsToGain (driveDb) * cruzdrive::shaperReference;

        // Normalise so the curve's loudest point touches the frame, which
        // matches what Auto Gain does to the output.
        constexpr int points = 200;
        std::array<float, points + 1> ys {};
        float peak = 1.0e-6f;
        for (int i = 0; i <= points; ++i)
        {
            const auto x = -1.0f + 2.0f * (float) i / (float) points;
            ys[(size_t) i] = cruzdrive::shape (style, x * gain);
            peak = juce::jmax (peak, std::abs (ys[(size_t) i]));
        }

        juce::Path curve, fill;
        for (int i = 0; i <= points; ++i)
        {
            const auto x = -1.0f + 2.0f * (float) i / (float) points;
            const auto pt = toScreen (x, ys[(size_t) i] / peak);
            if (i == 0)
            {
                curve.startNewSubPath (pt);
                fill.startNewSubPath (toScreen (x, 0.0f));
            }
            else
                curve.lineTo (pt);
            fill.lineTo (pt);
        }
        fill.lineTo (toScreen (1.0f, 0.0f));
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Colours::heat.withAlpha (0.28f), plot.getCentreX(), plot.getY(),
                                                 Colours::heat.withAlpha (0.0f), plot.getCentreX(), plot.getCentreY(), true));
        g.fillPath (fill);
        g.setColour (Colours::heat.withAlpha (0.3f));
        g.strokePath (curve, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Colours::glow);
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Where the incoming peak level sits on the curve.
        if (level > 0.0005f)
        {
            const auto x = juce::jmin (1.0f, level);
            const auto pt = toScreen (x, cruzdrive::shape (style, x * gain) / peak);
            g.setColour (Colours::text.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (pt));
            g.setColour (Colours::text);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (pt));
        }

        g.setColour (Colours::dimText);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (cruzdrive::styleNames[style].toUpperCase(), r.reduced (12.0f, 10.0f),
                    juce::Justification::topLeft);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (juce::String (driveDb, 1) + " dB drive", r.reduced (12.0f, 10.0f),
                    juce::Justification::bottomRight);
    }
}

//==============================================================================
CruzDriveEditor::CruzDriveEditor (CruzDriveProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      lookAndFeel (std::make_unique<DriveLookAndFeel>()),
      curve (p),
      drive (p.getState(), DriveParams::drive, "Drive", Colours::heat, true),
      lowCut (p.getState(), DriveParams::lowCut, "Low Cut", Colours::cool),
      tone (p.getState(), DriveParams::tone, "Tone", Colours::glow),
      highCut (p.getState(), DriveParams::highCut, "High Cut", Colours::cool),
      crush (p.getState(), DriveParams::crush, "Crush", Colours::crush),
      darkness (p.getState(), DriveParams::darkness, "Darkness", Colours::crush),
      mix (p.getState(), DriveParams::mix, "Mix", Colours::glow),
      output (p.getState(), DriveParams::output, "Output", Colours::glow),
      punish (p.getState(), DriveParams::punish, "PUNISH", Colours::crush),
      thump (p.getState(), DriveParams::thump, "THUMP", Colours::cool),
      steep (p.getState(), DriveParams::steep, "STEEP", Colours::cool),
      autoGain (p.getState(), DriveParams::autoGain, "AUTO GAIN", Colours::glow),
      inMeter ("IN", false, false),
      outMeter ("OUT", false, false),
      crushMeter ("GR", true, true)
{
    setLookAndFeel (lookAndFeel.get());

    addAndMakeVisible (curve);

    for (int i = 0; i < cruzdrive::numStyles; ++i)
    {
        auto* b = styleButtons.add (new juce::TextButton (cruzdrive::styleNames[i].toUpperCase()));
        b->setColour (juce::TextButton::buttonOnColourId, Colours::heat);
        b->setRadioGroupId (1);
        b->onClick = [this, i]
        {
            if (styleAttachment != nullptr)
                styleAttachment->setValueAsCompleteGesture ((float) i);
        };
        addAndMakeVisible (b);
    }
    styleAttachment = std::make_unique<juce::ParameterAttachment> (*p.getState().getParameter (DriveParams::style), [this] (float v)
    {
        const auto index = juce::roundToInt (v);
        for (int i = 0; i < styleButtons.size(); ++i)
            styleButtons[i]->setToggleState (i == index, juce::dontSendNotification);
        curve.repaint();
    });
    styleAttachment->sendInitialUpdate();

    for (auto* c : std::initializer_list<juce::Component*> { &drive, &lowCut, &tone, &highCut, &crush, &darkness,
                                                             &mix, &output, &punish, &thump, &steep, &autoGain,
                                                             &inMeter, &outMeter, &crushMeter })
        addAndMakeVisible (c);

    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        const auto index = presetBox.getSelectedItemIndex();
        if (index >= 0)
            processor.setCurrentProgram (index);
    };
    refreshPresetBox();
    addAndMakeVisible (presetBox);

    setSize (940, 580);
    startTimerHz (30);
}

CruzDriveEditor::~CruzDriveEditor()
{
    setLookAndFeel (nullptr);
}

void CruzDriveEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (processor.getCurrentProgram(), juce::dontSendNotification);
}

void CruzDriveEditor::timerCallback()
{
    const auto& engine = processor.getEngine();
    inMeter.setLevel (engine.meterIn.load());
    outMeter.setLevel (engine.meterOut.load());
    crushMeter.setLevel (engine.meterReductionDb.load());
    curve.refresh();

    // The host can switch presets too; keep the menu in step.
    if (presetBox.getSelectedItemIndex() != processor.getCurrentProgram())
        presetBox.setSelectedItemIndex (processor.getCurrentProgram(), juce::dontSendNotification);
}

void CruzDriveEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto header = getLocalBounds().reduced (20, 0).removeFromTop (56);
    auto titleArea = header.removeFromLeft (360);

    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    const auto titleWidth = juce::GlyphArrangement::getStringWidthInt (juce::Font (juce::FontOptions (24.0f, juce::Font::bold)), "CRUZ DRIVE");
    g.drawText ("CRUZ DRIVE", titleArea, juce::Justification::centredLeft);

    g.setColour (Colours::heat);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("SATURATION  /  CRUSH", titleArea.withTrimmedLeft (titleWidth + 14),
                juce::Justification::centredLeft);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("v" JucePlugin_VersionString, header, juce::Justification::centredRight);

    drawPanel (g, characterPanel, "CHARACTER", Colours::heat);
    drawPanel (g, drivePanel, "DRIVE & TONE", Colours::glow);
    drawPanel (g, crushPanel, "CRUSH", Colours::crush);
    drawPanel (g, outputPanel, "OUTPUT", Colours::glow);
}

void CruzDriveEditor::resized()
{
    auto area = getLocalBounds().reduced (20, 0);
    auto header = area.removeFromTop (56);
    presetBox.setBounds (header.removeFromRight (260).withTrimmedRight (50).withSizeKeepingCentre (210, 28));
    area.removeFromBottom (20);

    constexpr int gap = 14;
    characterPanel = area.removeFromLeft (340);
    area.removeFromLeft (gap);
    auto right = area.removeFromRight (250);
    area.removeFromRight (gap);
    drivePanel = area;

    // Character: curve display and style picker.
    {
        auto r = characterPanel.reduced (14).withTrimmedTop (24);
        auto buttons = r.removeFromBottom (34);
        r.removeFromBottom (12);
        curve.setBounds (r);

        const auto w = buttons.getWidth() / cruzdrive::numStyles;
        for (auto* b : styleButtons)
            b->setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    }

    // Drive & tone.
    {
        auto r = drivePanel.reduced (14).withTrimmedTop (24);
        auto top = r.removeFromTop (r.getHeight() * 55 / 100);
        punish.setBounds (top.removeFromBottom (30).withSizeKeepingCentre (110, 28));
        top.removeFromBottom (6);
        drive.setBounds (top.withSizeKeepingCentre (juce::jmin (top.getWidth(), 210), top.getHeight()));

        r.removeFromTop (10);
        auto switches = r.removeFromBottom (30);
        const auto w = r.getWidth() / 3;
        lowCut.setBounds (r.removeFromLeft (w).reduced (4, 0));
        tone.setBounds (r.removeFromLeft (w).reduced (4, 0));
        highCut.setBounds (r.reduced (4, 0));

        thump.setBounds (switches.removeFromLeft (w).withSizeKeepingCentre (90, 26));
        switches.removeFromLeft (w);
        steep.setBounds (switches.withSizeKeepingCentre (90, 26));
    }

    // Crush and output on the right.
    {
        crushPanel = right.removeFromTop (right.getHeight() * 45 / 100);
        right.removeFromTop (gap);
        outputPanel = right;

        auto c = crushPanel.reduced (14).withTrimmedTop (24);
        crushMeter.setBounds (c.removeFromBottom (18));
        c.removeFromBottom (8);
        crush.setBounds (c.removeFromLeft (c.getWidth() / 2).reduced (2, 0));
        darkness.setBounds (c.reduced (2, 0));

        auto o = outputPanel.reduced (14).withTrimmedTop (24);
        auto meters = o.removeFromRight (44);
        inMeter.setBounds (meters.removeFromLeft (20));
        meters.removeFromLeft (4);
        outMeter.setBounds (meters.removeFromLeft (20));
        o.removeFromRight (8);

        autoGain.setBounds (o.removeFromBottom (28).reduced (6, 0));
        o.removeFromBottom (8);
        mix.setBounds (o.removeFromLeft (o.getWidth() / 2));
        output.setBounds (o);
    }
}
