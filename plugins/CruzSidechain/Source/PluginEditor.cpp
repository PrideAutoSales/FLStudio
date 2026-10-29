#include "PluginEditor.h"

namespace Colours
{
    const juce::Colour background { 0xff0c0f15 };
    const juce::Colour panel { 0xff141922 };
    const juce::Colour plot { 0xff0f131a };
    const juce::Colour panelEdge { 0xff262e3c };
    const juce::Colour grid { 0xff1d2430 };
    const juce::Colour knobBody { 0xff1e2531 };
    const juce::Colour text { 0xffe6edf6 };
    const juce::Colour dimText { 0xff8792a4 };
    const juce::Colour accent { 0xff38c8ff };   // the trigger (kick) everywhere: cyan
    const juce::Colour track { 0xffb06cff };    // the track being ducked: violet
    const juce::Colour reduction { 0xffff5577 }; // gain reduction: pink-red
    const juce::Colour good { 0xff4fe08a };
    const juce::Colour warn { 0xffffb648 };

    // Black through deep red, orange and yellow to white, for low-end clashes.
    inline juce::Colour heat (float c)
    {
        static const juce::Colour stops[] = { juce::Colour (0xff200810), juce::Colour (0xff8c1424), juce::Colour (0xffe8442a),
                                              juce::Colour (0xffffae2e), juce::Colour (0xfffff3c4) };
        c = juce::jlimit (0.0f, 1.0f, c) * 4.0f;
        const int i = juce::jmin (3, (int) c);
        return stops[i].interpolatedWith (stops[i + 1], c - (float) i);
    }
}

namespace cruzui
{

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, Colours::accent.withAlpha (0.4f));
    setColour (juce::Label::textColourId, Colours::dimText);
    setColour (juce::TextButton::textColourOffId, Colours::dimText);
    setColour (juce::TextButton::textColourOnId, Colours::background);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Colours::dimText);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + pos * (endAngle - startAngle);
    const auto trackWidth = juce::jmax (3.0f, radius * 0.1f);
    const auto arcRadius = radius - trackWidth * 0.5f;
    const bool enabled = slider.isEnabled();

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (Colours::panelEdge);
    g.strokePath (track, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    // Bipolar knobs (feel, output) light up from the centre.
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const auto from = bipolar ? startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle)
                              : startAngle;

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
    g.setColour (enabled ? Colours::accent : Colours::dimText);
    g.strokePath (value, { trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded });

    const auto bodyRadius = arcRadius - trackWidth * 1.6f;
    const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
    g.setGradientFill (juce::ColourGradient (Colours::knobBody.brighter (0.3f), centre.x, centre.y - bodyRadius,
                                             Colours::knobBody.darker (0.1f), centre.x, centre.y + bodyRadius, false));
    g.fillEllipse (body);
    g.setColour (Colours::panelEdge.brighter (0.2f));
    g.drawEllipse (body, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -bodyRadius + 3.0f, 3.0f, bodyRadius * 0.45f, 1.5f);
    g.setColour (enabled ? Colours::text : Colours::dimText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                        bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = button.getToggleState();

    g.setColour (on ? Colours::accent : (highlighted || down ? Colours::knobBody.brighter (0.15f) : Colours::knobBody));
    g.fillRoundedRectangle (r, 5.0f);

    if (! on)
    {
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::FontOptions (juce::jmin (14.0f, (float) buttonHeight * 0.5f), juce::Font::bold);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (13.0f));
    return label;
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& titleText)
    : attachment (state, paramID, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 96, 18);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);

    if (auto* param = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

    addAndMakeVisible (slider);

    title.setText (titleText.toUpperCase(), juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (title);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
ChoiceButtons::ChoiceButtons (juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID));
    jassert (param != nullptr);

    const int group = juce::Random::getSystemRandom().nextInt (1 << 20) + 1;

    for (int i = 0; i < param->choices.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (param->choices[i]));
        b->setClickingTogglesState (false);
        b->setRadioGroupId (group);
        b->onClick = [this, i] { attachment->setValueAsCompleteGesture ((float) i); };
        addAndMakeVisible (b);
    }

    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        const int index = juce::roundToInt (v);
        for (int i = 0; i < buttons.size(); ++i)
            buttons[i]->setToggleState (i == index, juce::dontSendNotification);
    });
    attachment->sendInitialUpdate();
}

void ChoiceButtons::resized()
{
    auto r = getLocalBounds();
    const int w = r.getWidth() / juce::jmax (1, buttons.size());

    for (auto* b : buttons)
        b->setBounds (r.removeFromLeft (w));
}

//==============================================================================
void ParamDrag::begin (juce::RangedAudioParameter* p)
{
    param = p;
    if (param != nullptr)
    {
        start = value();
        param->beginChangeGesture();
    }
}

void ParamDrag::set (float v)
{
    if (param != nullptr)
        param->setValueNotifyingHost (param->convertTo0to1 (param->getNormalisableRange().snapToLegalValue (v)));
}

void ParamDrag::end()
{
    if (param != nullptr)
        param->endChangeGesture();
    param = nullptr;
}

float ParamDrag::value() const
{
    return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f;
}

static void resetToDefault (juce::RangedAudioParameter* p)
{
    if (p == nullptr)
        return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->getDefaultValue());
    p->endChangeGesture();
}

static void drawHandle (juce::Graphics& g, juce::Point<float> at, juce::Colour colour, bool active, bool square = false)
{
    const float r = active ? 7.0f : 5.5f;
    auto box = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (at);

    if (active)
    {
        g.setColour (colour.withAlpha (0.25f));
        g.fillEllipse (box.expanded (5.0f));
    }

    g.setColour (Colours::plot);
    square ? g.fillRoundedRectangle (box, 2.0f) : g.fillEllipse (box);
    g.setColour (colour);
    square ? g.drawRoundedRectangle (box, 2.0f, 2.0f) : g.drawEllipse (box, 2.0f);
}

static void drawPlotBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (Colours::plot);
    g.fillRoundedRectangle (r, 4.0f);
}

static juce::String noteName (float hz)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int midi = juce::roundToInt (69.0f + 12.0f * std::log2 (hz / 440.0f));
    return juce::String (names[((midi % 12) + 12) % 12]) + juce::String (midi / 12 - 1);
}

//==============================================================================
ScopeView::ScopeView (CruzSidechainAudioProcessor& p, const History& h) : processor (p), history (h)
{
    setInterceptsMouseClicks (false, false);
}

void ScopeView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawPlotBackground (g, area);

    auto inner = area.reduced (8.0f, 6.0f);
    auto top = inner.removeFromTop (inner.getHeight() * 0.42f);
    inner.removeFromTop (6.0f);
    auto bottom = inner;

    const int n = History::size;
    const float w = top.getWidth();

    auto lane = [&] (juce::Rectangle<float> r, auto getMin, auto getMax, juce::Colour colour, float alpha)
    {
        juce::Path p;
        const float mid = r.getCentreY(), half = r.getHeight() * 0.5f;

        for (int i = 0; i < n; ++i)
        {
            const auto& f = history[i];
            const float x = r.getX() + w * (float) i / (float) (n - 1);
            const float y = mid - juce::jlimit (-1.0f, 1.0f, getMax (f)) * half;
            i == 0 ? p.startNewSubPath (x, y) : p.lineTo (x, y);
        }
        for (int i = n - 1; i >= 0; --i)
        {
            const auto& f = history[i];
            const float x = r.getX() + w * (float) i / (float) (n - 1);
            p.lineTo (x, mid - juce::jlimit (-1.0f, 1.0f, getMin (f)) * half);
        }
        p.closeSubPath();

        g.setColour (colour.withAlpha (alpha));
        g.fillPath (p);
        g.setColour (colour.withAlpha (juce::jmin (1.0f, alpha + 0.25f)));
        g.strokePath (p, juce::PathStrokeType (0.8f));
    };

    // Centre lines.
    g.setColour (Colours::grid);
    g.drawHorizontalLine ((int) top.getCentreY(), top.getX(), top.getRight());
    g.drawHorizontalLine ((int) bottom.getCentreY(), bottom.getX(), bottom.getRight());

    lane (top, [] (const cruzsc::Frame& f) { return f.keyMin; }, [] (const cruzsc::Frame& f) { return f.keyMax; }, Colours::accent, 0.55f);
    lane (bottom, [] (const cruzsc::Frame& f) { return f.inMin; }, [] (const cruzsc::Frame& f) { return f.inMax; }, Colours::dimText, 0.18f);
    lane (bottom, [] (const cruzsc::Frame& f) { return f.outMin; }, [] (const cruzsc::Frame& f) { return f.outMax; }, Colours::track, 0.6f);

    // Gain reduction hanging down from the top of the lower lane.
    const float rangeDb = juce::jmax (6.0f, processor.getState().getRawParameterValue (ParamIDs::range)->load());
    juce::Path gr;
    for (int i = 0; i < n; ++i)
    {
        const float x = bottom.getX() + w * (float) i / (float) (n - 1);
        const float y = bottom.getY() + bottom.getHeight() * juce::jlimit (0.0f, 1.0f, history[i].grDb / rangeDb);
        i == 0 ? gr.startNewSubPath (x, y) : gr.lineTo (x, y);
    }
    g.setColour (Colours::reduction);
    g.strokePath (gr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Lane names and the live reduction.
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.setColour (Colours::accent);
    g.drawText ("TRIGGER", top.reduced (4.0f, 2.0f), juce::Justification::topLeft);
    g.setColour (Colours::track);
    g.drawText ("DUCKED TRACK", bottom.reduced (4.0f, 2.0f), juce::Justification::topLeft);
    g.setColour (Colours::dimText.withAlpha (0.8f));
    g.drawText ("grey = before", bottom.reduced (4.0f, 2.0f).translated (0.0f, 14.0f), juce::Justification::topLeft);

    g.setColour (Colours::reduction);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("-" + juce::String (processor.getGainReductionDb(), 1) + " dB", bottom.reduced (4.0f, 2.0f), juce::Justification::topRight);
}

//==============================================================================
HeatMapView::HeatMapView (const History& h) : history (h)
{
    for (auto* b : { &beforeButton, &afterButton })
    {
        b->setClickingTogglesState (false);
        addAndMakeVisible (b);
    }

    beforeButton.onClick = [this] { showAfter = false; afterButton.setToggleState (false, juce::dontSendNotification); beforeButton.setToggleState (true, juce::dontSendNotification); repaint(); };
    afterButton.onClick = [this] { showAfter = true; afterButton.setToggleState (true, juce::dontSendNotification); beforeButton.setToggleState (false, juce::dontSendNotification); repaint(); };
    afterButton.setToggleState (true, juce::dontSendNotification);
}

void HeatMapView::resized()
{
    auto top = getLocalBounds().removeFromTop (24);
    beforeButton.setBounds (top.removeFromLeft (118));
    afterButton.setBounds (top.removeFromLeft (110));
}

void HeatMapView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    auto header = area.removeFromTop (24.0f);
    area.removeFromTop (6.0f);

    auto legend = area.removeFromBottom (18.0f);
    auto axis = area.removeFromLeft (44.0f);
    auto profile = area.removeFromRight (86.0f);
    area.removeFromRight (6.0f);

    drawPlotBackground (g, area);

    constexpr int bands = cruzsc::numBands;
    const int n = History::size;
    constexpr float floorDb = -48.0f;
    auto norm = [] (float db) { return juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb); };

    std::array<float, bands> clashSum {};

    {
        juce::Image::BitmapData pixels (image, juce::Image::BitmapData::writeOnly);

        for (int i = 0; i < n; ++i)
        {
            const auto& f = history[i];
            const auto& track = showAfter ? f.outBands : f.inBands;

            for (int b = 0; b < bands; ++b)
            {
                const float k = norm (f.keyBands[(size_t) b]);
                const float t = norm (track[(size_t) b]);
                const float clash = std::sqrt (k * t);
                clashSum[(size_t) b] += clash * clash;

                // Each signal alone shows in its own colour; where both are loud the heat takes over.
                auto base = Colours::plot;
                base = base.interpolatedWith (Colours::accent, std::pow (k, 1.6f) * 0.55f);
                base = base.interpolatedWith (Colours::track, std::pow (t, 1.6f) * 0.55f);
                const float hot = juce::jlimit (0.0f, 1.0f, (clash - 0.4f) / 0.4f);
                pixels.setPixelColour (i, bands - 1 - b, base.interpolatedWith (Colours::heat (clash), hot * hot * (3.0f - 2.0f * hot)));
            }
        }
    }

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImage (image, area, juce::RectanglePlacement::stretchToFit);
    g.restoreState();

    // Frequency axis.
    auto yForHz = [&] (float hz)
    {
        const float band = 6.0f * std::log2 (hz / 20.0f);
        return area.getBottom() - (band + 0.5f) / (float) bands * area.getHeight();
    };

    g.setFont (juce::FontOptions (10.5f));
    for (float hz : { 20.0f, 30.0f, 40.0f, 60.0f, 80.0f, 120.0f, 160.0f, 250.0f })
    {
        const float y = yForHz (hz);
        g.setColour (Colours::grid.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
        g.setColour (Colours::dimText);
        g.drawText (juce::String ((int) hz) + " Hz", axis.withY (y - 7.0f).withHeight (14.0f).withTrimmedRight (6.0f),
                    juce::Justification::centredRight);
    }

    // Clash profile: how much each band clashed over the last few seconds.
    drawPlotBackground (g, profile);
    float maxClash = 0.0f;
    int worst = 0;
    for (int b = 0; b < bands; ++b)
    {
        clashSum[(size_t) b] = std::sqrt (clashSum[(size_t) b] / (float) n);
        if (clashSum[(size_t) b] > maxClash) { maxClash = clashSum[(size_t) b]; worst = b; }
    }

    const float rowH = profile.getHeight() / (float) bands;
    for (int b = 0; b < bands; ++b)
    {
        const float c = clashSum[(size_t) b];
        const float len = (profile.getWidth() - 8.0f) * juce::jlimit (0.0f, 1.0f, c / 0.6f);
        auto row = juce::Rectangle<float> (profile.getX() + 4.0f, profile.getBottom() - (float) (b + 1) * rowH, len, rowH).reduced (0.0f, 0.5f);
        g.setColour (Colours::heat (0.25f + c * 1.2f).withAlpha (c < 0.05f ? 0.3f : 1.0f));
        g.fillRect (row);
    }
    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText ("CLASH", profile.reduced (4.0f, 2.0f), juce::Justification::topRight);

    // Header: where it clashes most.
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    if (maxClash < 0.12f)
    {
        g.setColour (Colours::good);
        g.drawText ("No low-end clash right now", header, juce::Justification::centredRight);
    }
    else
    {
        const float hz = cruzsc::bandHz (worst);
        g.setColour (Colours::heat (0.3f + maxClash));
        g.drawText ("Most clash: " + juce::String (juce::roundToInt (hz)) + " Hz (" + noteName (hz) + ")",
                    header, juce::Justification::centredRight);
    }

    // Legend.
    g.setFont (juce::FontOptions (11.0f));
    auto item = [&] (juce::Colour c, const juce::String& text, float width)
    {
        auto r = legend.removeFromLeft (width);
        g.setColour (c);
        g.fillRoundedRectangle (r.removeFromLeft (12.0f).withSizeKeepingCentre (10.0f, 10.0f), 2.0f);
        g.setColour (Colours::dimText);
        g.drawText (text, r.withTrimmedLeft (5.0f), juce::Justification::centredLeft);
    };
    legend.removeFromLeft (axis.getWidth());
    item (Colours::accent, "Trigger only", 110.0f);
    item (Colours::track, "Ducked track only", 140.0f);
    item (Colours::heat (0.75f), "Both at once = clash", 170.0f);
}

//==============================================================================
CurveEditor::CurveEditor (CruzSidechainAudioProcessor& p) : processor (p) {}

juce::Rectangle<float> CurveEditor::plot() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (30.0f).withTrimmedBottom (16.0f).withTrimmedTop (22.0f).reduced (2.0f);
}

static constexpr float curveLowDb = -60.0f;

float CurveEditor::xFor (float db) const
{
    const auto r = plot();
    return r.getX() + (db - curveLowDb) / -curveLowDb * r.getWidth();
}

float CurveEditor::yFor (float db) const
{
    const auto r = plot();
    return r.getBottom() - (db - curveLowDb) / -curveLowDb * r.getHeight();
}

float CurveEditor::dbPerPixel() const { return -curveLowDb / juce::jmax (1.0f, plot().getWidth()); }

juce::RangedAudioParameter* CurveEditor::paramFor (Handle h) const
{
    auto& s = processor.getState();
    switch (h)
    {
        case Handle::threshold: return s.getParameter (ParamIDs::threshold);
        case Handle::ratio: return s.getParameter (ParamIDs::ratio);
        case Handle::knee: return s.getParameter (ParamIDs::knee);
        case Handle::range: return s.getParameter (ParamIDs::range);
        case Handle::none: break;
    }
    return nullptr;
}

juce::Point<float> CurveEditor::handlePos (Handle h) const
{
    const auto p = processor.readParams();
    auto out = [&] (float in) { return in - cruzsc::gainReductionDb (in, p); };

    switch (h)
    {
        case Handle::threshold: return { xFor (p.thresholdDb), yFor (out (p.thresholdDb)) };
        case Handle::knee:
        {
            const float x = juce::jmin (0.0f, p.thresholdDb + juce::jmax (1.5f, p.kneeDb * 0.5f));
            return { xFor (x), yFor (out (x)) };
        }
        case Handle::ratio:
        {
            const float slope = p.ratio >= cruzsc::infiniteRatio ? 0.0f : 1.0f / p.ratio;
            return { xFor (0.0f), yFor (p.thresholdDb - p.thresholdDb * slope) };
        }
        case Handle::range:
        {
            const float x = -8.0f;
            return { xFor (x), yFor (juce::jmax (curveLowDb, x - p.rangeDb)) };
        }
        case Handle::none: break;
    }
    return {};
}

CurveEditor::Handle CurveEditor::hit (juce::Point<float> at) const
{
    Handle best = Handle::none;
    float bestDist = 14.0f;
    for (auto h : { Handle::knee, Handle::threshold, Handle::ratio, Handle::range })
    {
        const float d = handlePos (h).getDistanceFrom (at);
        if (d < bestDist) { bestDist = d; best = h; }
    }
    return best;
}

void CurveEditor::paint (juce::Graphics& g)
{
    const auto r = plot();
    drawPlotBackground (g, r);
    const auto p = processor.readParams();

    // Info line.
    auto& s = processor.getState();
    juce::String info;
    info << "Thr " << s.getParameter (ParamIDs::threshold)->getCurrentValueAsText()
         << "   Ratio " << s.getParameter (ParamIDs::ratio)->getCurrentValueAsText()
         << "   Knee " << s.getParameter (ParamIDs::knee)->getCurrentValueAsText()
         << "   Range " << s.getParameter (ParamIDs::range)->getCurrentValueAsText();
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (info, getLocalBounds().toFloat().removeFromTop (20.0f).withTrimmedLeft (30.0f), juce::Justification::centredLeft);

    // Grid every 12 dB.
    g.setFont (juce::FontOptions (10.0f));
    for (float db = curveLowDb; db <= 0.0f; db += 12.0f)
    {
        g.setColour (Colours::grid);
        g.drawVerticalLine ((int) xFor (db), r.getY(), r.getBottom());
        g.drawHorizontalLine ((int) yFor (db), r.getX(), r.getRight());
        g.setColour (Colours::dimText);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (0.0f, yFor (db) - 6.0f, 26.0f, 12.0f), juce::Justification::centredRight);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (xFor (db) - 14.0f, r.getBottom() + 2.0f, 28.0f, 12.0f), juce::Justification::centred);
    }

    // Knee zone.
    if (p.kneeDb > 0.1f)
    {
        g.setColour (Colours::accent.withAlpha (0.07f));
        const float x0 = xFor (juce::jmax (curveLowDb, p.thresholdDb - p.kneeDb * 0.5f));
        const float x1 = xFor (juce::jmin (0.0f, p.thresholdDb + p.kneeDb * 0.5f));
        g.fillRect (juce::Rectangle<float> (x0, r.getY(), x1 - x0, r.getHeight()));
    }

    // 1:1 line and the range floor.
    g.setColour (Colours::dimText.withAlpha (0.35f));
    g.drawLine (xFor (curveLowDb), yFor (curveLowDb), xFor (0.0f), yFor (0.0f), 1.0f);

    {
        juce::Path floorLine;
        const float startIn = juce::jmin (0.0f, curveLowDb + p.rangeDb);
        floorLine.startNewSubPath (xFor (startIn), yFor (startIn - p.rangeDb));
        floorLine.lineTo (xFor (0.0f), yFor (-p.rangeDb));
        juce::Path dashed;
        const float dashes[] = { 4.0f, 4.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, floorLine, dashes, 2);
        g.setColour (Colours::reduction.withAlpha (0.55f));
        g.fillPath (dashed);
    }

    // The curve.
    juce::Path curve;
    for (int i = 0; i <= 240; ++i)
    {
        const float in = curveLowDb + -curveLowDb * (float) i / 240.0f;
        const float out = in - cruzsc::gainReductionDb (in, p);
        i == 0 ? curve.startNewSubPath (xFor (in), yFor (out)) : curve.lineTo (xFor (in), yFor (out));
    }

    {
        juce::Path fill (curve);
        fill.lineTo (r.getRight(), r.getBottom());
        fill.lineTo (r.getX(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Colours::accent.withAlpha (0.16f), r.getX(), r.getY(),
                                                 Colours::accent.withAlpha (0.0f), r.getX(), r.getBottom(), false));
        g.fillPath (fill);
    }

    g.setColour (Colours::accent);
    g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Live trigger level on the curve.
    const float level = juce::jlimit (curveLowDb, 0.0f, processor.getKeyLevelDb());
    if (processor.getKeyLevelDb() > curveLowDb)
    {
        const juce::Point<float> dot { xFor (level), yFor (level - cruzsc::gainReductionDb (level, p)) };
        g.setColour (Colours::text.withAlpha (0.25f));
        g.drawVerticalLine ((int) dot.x, dot.y, r.getBottom());
        g.setColour (Colours::reduction.withAlpha (0.35f));
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (dot));
        g.setColour (Colours::text);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (dot));
    }

    // Handles.
    for (auto h : { Handle::range, Handle::ratio, Handle::knee, Handle::threshold })
    {
        const bool active = h == hover || h == dragging;
        const auto colour = h == Handle::range ? Colours::reduction : (h == Handle::threshold ? Colours::text : Colours::accent);
        drawHandle (g, handlePos (h), colour, active, h == Handle::knee || h == Handle::range);
    }

    // Name of the handle under the mouse.
    const auto shown = dragging != Handle::none ? dragging : hover;
    if (auto* param = paramFor (shown))
    {
        const auto at = handlePos (shown);
        juce::String text = param->getName (32) + "  " + param->getCurrentValueAsText();
        auto box = juce::Rectangle<float> (150.0f, 20.0f).withCentre (at.translated (0.0f, -20.0f));
        box = box.constrainedWithin (r.reduced (2.0f));
        g.setColour (Colours::panel.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (text, box, juce::Justification::centred);
    }

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("trigger level in (dB)", r.reduced (6.0f, 4.0f), juce::Justification::bottomRight);
    g.drawText ("track level out", r.reduced (6.0f, 4.0f), juce::Justification::topLeft);
}

void CurveEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hit (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h == Handle::none ? juce::MouseCursor::NormalCursor
                    : (h == Handle::ratio || h == Handle::range ? juce::MouseCursor::UpDownResizeCursor
                                                                 : juce::MouseCursor::LeftRightResizeCursor));
}

void CurveEditor::mouseExit (const juce::MouseEvent&)
{
    hover = Handle::none;
    repaint();
}

void CurveEditor::mouseDown (const juce::MouseEvent& e)
{
    dragging = hit (e.position);
    if (dragging == Handle::none)
        return;

    dragStart = e.position;
    drag.begin (paramFor (dragging));

    const auto p = processor.readParams();
    const float slope = p.ratio >= cruzsc::infiniteRatio ? 0.0f : 1.0f / p.ratio;
    startOutAtZero = p.thresholdDb - p.thresholdDb * slope;
}

void CurveEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Handle::none)
        return;

    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    const float dx = (e.position.x - dragStart.x) * dbPerPixel() * fine;
    const float dy = -(e.position.y - dragStart.y) * dbPerPixel() * fine;

    switch (dragging)
    {
        case Handle::threshold: drag.set (drag.start + dx); break;
        case Handle::knee: drag.set (drag.start + 2.0f * dx); break;
        case Handle::range: drag.set (drag.start - dy); break;
        case Handle::ratio:
        {
            const float threshold = processor.readParams().thresholdDb;
            const float outAtZero = juce::jmin (0.0f, startOutAtZero + dy);
            const float rise = outAtZero - threshold;
            drag.set (rise <= 0.05f ? cruzsc::infiniteRatio : juce::jlimit (1.0f, cruzsc::infiniteRatio, -threshold / rise));
            break;
        }
        case Handle::none: break;
    }

    repaint();
}

void CurveEditor::mouseUp (const juce::MouseEvent&)
{
    drag.end();
    dragging = Handle::none;
    repaint();
}

void CurveEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    resetToDefault (paramFor (hit (e.position)));
}

//==============================================================================
ShapeEditor::ShapeEditor (CruzSidechainAudioProcessor& p) : processor (p) {}

juce::Rectangle<float> ShapeEditor::plot() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (30.0f).withTrimmedBottom (16.0f).withTrimmedTop (22.0f).reduced (2.0f);
}

float ShapeEditor::spanMs() const
{
    const auto p = processor.readParams();
    const float total = juce::jmax (40.0f, (p.attackMs + p.holdMs + p.releaseMs) * 1.15f);
    return juce::jmax (total, frozenSpan);
}

// A short run-in before the hit, so the trigger moment is visible.
static constexpr float shapePreFraction = 0.06f;

float ShapeEditor::xFor (float ms) const
{
    const auto r = plot();
    const float span = spanMs();
    const float pre = span * shapePreFraction;
    return r.getX() + (ms + pre) / (span + pre) * r.getWidth();
}

float ShapeEditor::yFor (float depth) const
{
    const auto r = plot();
    return r.getY() + 6.0f + depth * (r.getHeight() - 12.0f);
}

juce::RangedAudioParameter* ShapeEditor::paramFor (Handle h) const
{
    auto& s = processor.getState();
    switch (h)
    {
        case Handle::attack: return s.getParameter (ParamIDs::attack);
        case Handle::hold: return s.getParameter (ParamIDs::hold);
        case Handle::release: return s.getParameter (ParamIDs::release);
        case Handle::attackBend: return s.getParameter (ParamIDs::attackBend);
        case Handle::releaseBend: return s.getParameter (ParamIDs::releaseBend);
        case Handle::none: break;
    }
    return nullptr;
}

juce::Point<float> ShapeEditor::handlePos (Handle h) const
{
    const auto p = processor.readParams();
    cruzsc::Bend a, rel;
    a.set (p.attackBend);
    rel.set (p.releaseBend);

    switch (h)
    {
        case Handle::attack: return { xFor (p.attackMs), yFor (1.0f) };
        case Handle::hold: return { xFor (p.attackMs + p.holdMs), yFor (1.0f) };
        case Handle::release: return { xFor (p.attackMs + p.holdMs + p.releaseMs), yFor (0.0f) };
        case Handle::attackBend: return { xFor (p.attackMs * 0.5f), yFor (a (0.5f)) };
        case Handle::releaseBend: return { xFor (p.attackMs + p.holdMs + p.releaseMs * 0.5f), yFor (1.0f - rel (0.5f)) };
        case Handle::none: break;
    }
    return {};
}

ShapeEditor::Handle ShapeEditor::hit (juce::Point<float> at) const
{
    Handle best = Handle::none;
    float bestDist = 14.0f;

    for (auto h : { Handle::attack, Handle::hold, Handle::release, Handle::releaseBend, Handle::attackBend })
    {
        const auto pos = handlePos (h);
        float d = pos.getDistanceFrom (at);

        // Attack and hold sit on top of each other when hold is 0: left of the
        // point picks attack, right of it picks hold.
        if (h == Handle::hold && at.x < pos.x) d += 4.0f;
        if (h == Handle::attack && at.x > pos.x) d += 4.0f;

        if (d < bestDist) { bestDist = d; best = h; }
    }
    return best;
}

void ShapeEditor::paint (juce::Graphics& g)
{
    const auto r = plot();
    drawPlotBackground (g, r);
    const auto p = processor.readParams();
    auto& s = processor.getState();

    juce::String info;
    info << "Attack " << s.getParameter (ParamIDs::attack)->getCurrentValueAsText()
         << "   Hold " << s.getParameter (ParamIDs::hold)->getCurrentValueAsText()
         << "   Release " << s.getParameter (ParamIDs::release)->getCurrentValueAsText();
    g.setColour (Colours::text);
    g.setFont (juce::FontOptions (11.5f));
    g.drawText (info, getLocalBounds().toFloat().removeFromTop (20.0f).withTrimmedLeft (30.0f), juce::Justification::centredLeft);

    // Beat grid at the host tempo, so the release can be fitted to the groove.
    const float quarterMs = 60000.0f / juce::jmax (20.0f, processor.getHostBpm());
    const float span = spanMs();
    g.setFont (juce::FontOptions (10.0f));
    const std::pair<float, const char*> marks[] = { { 0.25f, "1/16" }, { 0.5f, "1/8" }, { 1.0f, "1/4" }, { 2.0f, "1/2" }, { 4.0f, "1 bar" } };
    for (auto [beats, name] : marks)
    {
        const float ms = beats * quarterMs;
        if (ms > span)
            break;
        const float x = xFor (ms);
        g.setColour (Colours::grid.brighter (0.2f));
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (Colours::dimText);
        g.drawText (name, juce::Rectangle<float> (x - 20.0f, r.getBottom() + 2.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    // The trigger moment.
    g.setColour (Colours::accent.withAlpha (0.8f));
    g.drawVerticalLine ((int) xFor (0.0f), r.getY(), r.getBottom());

    g.setColour (Colours::dimText);
    g.drawText ("0 dB", juce::Rectangle<float> (0.0f, yFor (0.0f) - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight);
    g.drawText ("-" + juce::String (juce::roundToInt (p.rangeDb)), juce::Rectangle<float> (0.0f, yFor (1.0f) - 6.0f, 28.0f, 12.0f),
                juce::Justification::centredRight);

    // The duck shape.
    cruzsc::Bend a, rel;
    a.set (p.attackBend);
    rel.set (p.releaseBend);

    auto depthAt = [&] (float t)
    {
        if (t < 0.0f) return 0.0f;
        if (t < p.attackMs) return a (t / p.attackMs);
        if (t < p.attackMs + p.holdMs) return 1.0f;
        if (t < p.attackMs + p.holdMs + p.releaseMs) return 1.0f - rel ((t - p.attackMs - p.holdMs) / p.releaseMs);
        return 0.0f;
    };

    juce::Path shapePath;
    const float pre = span * shapePreFraction;
    shapePath.startNewSubPath (xFor (-pre), yFor (0.0f));
    shapePath.lineTo (xFor (0.0f), yFor (0.0f));

    for (int i = 0; i <= 60; ++i)
        shapePath.lineTo (xFor (p.attackMs * (float) i / 60.0f), yFor (depthAt (p.attackMs * (float) i / 60.0f - 1.0e-4f * (i == 60))));
    shapePath.lineTo (xFor (p.attackMs + p.holdMs), yFor (1.0f));
    for (int i = 1; i <= 120; ++i)
    {
        const float t = p.attackMs + p.holdMs + p.releaseMs * (float) i / 120.0f;
        shapePath.lineTo (xFor (t), yFor (depthAt (t - 1.0e-4f)));
    }
    shapePath.lineTo (xFor (span), yFor (0.0f));

    {
        juce::Path fill (shapePath);
        fill.lineTo (xFor (span), yFor (0.0f));
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Colours::reduction.withAlpha (0.05f), r.getX(), yFor (0.0f),
                                                 Colours::reduction.withAlpha (0.28f), r.getX(), yFor (1.0f), false));
        g.fillPath (fill);
    }

    g.setColour (Colours::reduction);
    g.strokePath (shapePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Live reduction as a level line.
    const float live = juce::jlimit (0.0f, 1.0f, processor.getGainReductionDb() / juce::jmax (1.0f, p.rangeDb));
    if (live > 0.01f)
    {
        g.setColour (Colours::text.withAlpha (0.35f));
        g.drawHorizontalLine ((int) yFor (live), r.getX(), r.getRight());
    }

    for (auto h : { Handle::attackBend, Handle::releaseBend, Handle::release, Handle::hold, Handle::attack })
    {
        const bool active = h == hover || h == dragging;
        const bool bend = h == Handle::attackBend || h == Handle::releaseBend;
        drawHandle (g, handlePos (h), bend ? Colours::warn : Colours::text, active, bend);
    }

    const auto shown = dragging != Handle::none ? dragging : hover;
    if (auto* param = paramFor (shown))
    {
        const auto at = handlePos (shown);
        juce::String text = param->getName (32) + "  " + param->getCurrentValueAsText();
        auto box = juce::Rectangle<float> (150.0f, 20.0f).withCentre (at.translated (0.0f, at.y > r.getCentreY() ? -22.0f : 22.0f));
        box = box.constrainedWithin (r.reduced (2.0f));
        g.setColour (Colours::panel.withAlpha (0.92f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
        g.drawText (text, box, juce::Justification::centred);
    }
}

void ShapeEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto h = hit (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h == Handle::none ? juce::MouseCursor::NormalCursor
                    : (h == Handle::attackBend || h == Handle::releaseBend ? juce::MouseCursor::UpDownResizeCursor
                                                                            : juce::MouseCursor::LeftRightResizeCursor));
}

void ShapeEditor::mouseExit (const juce::MouseEvent&)
{
    hover = Handle::none;
    repaint();
}

void ShapeEditor::mouseDown (const juce::MouseEvent& e)
{
    dragging = hit (e.position);
    if (dragging == Handle::none)
        return;

    // Keep the time scale still while dragging, so the point stays under the mouse.
    frozenSpan = spanMs();
    dragStart = e.position;
    drag.begin (paramFor (dragging));
}

void ShapeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging == Handle::none)
        return;

    const auto r = plot();
    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    const float msPerPixel = frozenSpan * (1.0f + shapePreFraction) / juce::jmax (1.0f, r.getWidth());
    const float dt = (e.position.x - dragStart.x) * msPerPixel * fine;

    // Where the mouse is as a duck depth, for the bend handles. A bend's
    // half-way point sits at 1 / (1 + e^(-3 * bend)), so solve that for bend.
    const float depth = juce::jlimit (0.02f, 0.98f, (e.position.y - (r.getY() + 6.0f)) / juce::jmax (1.0f, r.getHeight() - 12.0f));
    auto bendFor = [] (float half) { return juce::jlimit (-1.0f, 1.0f, -2.0f * std::log (1.0f / half - 1.0f) / 6.0f); };

    switch (dragging)
    {
        case Handle::attack:
        case Handle::hold:
        case Handle::release: drag.set (drag.start + dt); break;
        case Handle::attackBend: drag.set (bendFor (depth) * 100.0f); break;
        case Handle::releaseBend: drag.set (bendFor (1.0f - depth) * 100.0f); break;
        case Handle::none: break;
    }

    // Let the scale grow if a point is pulled past the edge.
    const auto p = processor.readParams();
    frozenSpan = juce::jmax (frozenSpan, (p.attackMs + p.holdMs + p.releaseMs) * 1.05f);
    repaint();
}

void ShapeEditor::mouseUp (const juce::MouseEvent&)
{
    drag.end();
    dragging = Handle::none;
    frozenSpan = 0.0f;
    repaint();
}

void ShapeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    resetToDefault (paramFor (hit (e.position)));
}

} // namespace cruzui

//==============================================================================
CruzSidechainAudioProcessorEditor::CruzSidechainAudioProcessorEditor (CruzSidechainAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      triggerButtons (p.getState(), ParamIDs::trigger),
      rateButtons (p.getState(), ParamIDs::rate),
      detectButtons (p.getState(), ParamIDs::detect),
      splitAttachment (p.getState(), ParamIDs::splitLows, splitButton),
      listenAttachment (p.getState(), ParamIDs::listen, listenButton),
      scope (p, history),
      heatMap (history),
      curve (p),
      shape (p),
      threshold (p.getState(), ParamIDs::threshold, "Threshold"),
      ratio (p.getState(), ParamIDs::ratio, "Ratio"),
      knee (p.getState(), ParamIDs::knee, "Knee"),
      range (p.getState(), ParamIDs::range, "Range"),
      attack (p.getState(), ParamIDs::attack, "Attack"),
      hold (p.getState(), ParamIDs::hold, "Hold"),
      release (p.getState(), ParamIDs::release, "Release"),
      lookahead (p.getState(), ParamIDs::lookahead, "Lookahead"),
      keyGain (p.getState(), ParamIDs::keyGain, "Trigger Gain"),
      keyHighPass (p.getState(), ParamIDs::keyHighPass, "Trigger HPF"),
      keyLowPass (p.getState(), ParamIDs::keyLowPass, "Trigger LPF"),
      splitFreq (p.getState(), ParamIDs::splitFreq, "Split Freq"),
      attackBend (p.getState(), ParamIDs::attackBend, "Attack Bend"),
      releaseBend (p.getState(), ParamIDs::releaseBend, "Release Bend"),
      mix (p.getState(), ParamIDs::mix, "Mix"),
      output (p.getState(), ParamIDs::output, "Output")
{
    for (auto* b : { &splitButton, &listenButton })
        b->setClickingTogglesState (true);

    for (auto* c : std::initializer_list<juce::Component*> { &triggerButtons, &rateButtons, &detectButtons, &splitButton, &listenButton,
                                                             &scope, &heatMap, &curve, &shape })
        addAndMakeVisible (c);

    for (auto* k : { &threshold, &ratio, &knee, &range, &attack, &hold, &release, &lookahead,
                     &keyGain, &keyHighPass, &keyLowPass, &splitFreq, &attackBend, &releaseBend, &mix, &output })
        addAndMakeVisible (k);

    // After the children are added, so their value boxes pick up these colours.
    setLookAndFeel (&lookAndFeel);
    setSize (1200, 780);
    startTimerHz (50);
}

CruzSidechainAudioProcessorEditor::~CruzSidechainAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

juce::String CruzSidechainAudioProcessorEditor::statusText() const
{
    const auto source = (TriggerSource) juce::roundToInt (processor.getState().getRawParameterValue (ParamIDs::trigger)->load());

    switch (source)
    {
        case TriggerSource::sidechain:
            if (! processor.isSidechainConnected())
                return "No sidechain input here. Use Tempo or Self, or route a track in (see INSTALL)";
            if (! processor.hasSidechainSignal())
                return "Nothing on input 2. In FL: gear icon > Processing > set input 2 to the trigger's track";
            return "Sidechain signal coming in";
        case TriggerSource::self:
            return "Ducking from this track's own sound";
        case TriggerSource::tempo:
            return processor.isHostPlaying() ? "Ghost kick at " + juce::String (processor.getHostBpm(), 1) + " BPM"
                                             : "Ghost kick plays when FL Studio plays";
    }
    return {};
}

void CruzSidechainAudioProcessorEditor::timerCallback()
{
    const int got = processor.popFrames (incoming.data(), (int) incoming.size());
    for (int i = 0; i < got; ++i)
        history.push (incoming[(size_t) i]);

    const auto source = (TriggerSource) juce::roundToInt (processor.getState().getRawParameterValue (ParamIDs::trigger)->load());
    rateButtons.setEnabled (source == TriggerSource::tempo);
    rateButtons.setAlpha (source == TriggerSource::tempo ? 1.0f : 0.4f);
    splitFreq.setEnabled (splitButton.getToggleState());

    const auto text = statusText();
    const bool ok = (source == TriggerSource::sidechain && processor.hasSidechainSignal())
                    || source == TriggerSource::self
                    || (source == TriggerSource::tempo && processor.isHostPlaying());
    const auto colour = ok ? Colours::good : Colours::warn;
    if (text != status || colour != statusColour)
    {
        status = text;
        statusColour = colour;
        repaint (scopePanel.withHeight (30));
    }

    scope.repaint();
    heatMap.repaint();
    curve.repaint();
    shape.repaint();
}

void CruzSidechainAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto header = getLocalBounds().reduced (16, 0).removeFromTop (56);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.setColour (Colours::text);
    g.drawText ("CRUZ", header.removeFromLeft (70), juce::Justification::centredLeft);
    g.setColour (Colours::accent);
    g.drawText ("SIDECHAIN", header.removeFromLeft (150), juce::Justification::centredLeft);

    g.setColour (Colours::dimText);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("TRIGGER", triggerLabel, juce::Justification::centredRight);
    g.drawText ("RATE", rateLabel, juce::Justification::centredRight);
    g.drawText ("DETECT", detectLabel, juce::Justification::centredRight);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("v" JucePlugin_VersionString, knobPanel.translated (0, knobPanel.getHeight()).withHeight (14).withTrimmedRight (4),
                juce::Justification::centredRight);

    auto panel = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (Colours::panelEdge);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours::dimText);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (title, r.reduced (12, 6).removeFromTop (16), juce::Justification::centredLeft);
    };

    panel (scopePanel, "WAVEFORMS");
    panel (heatPanel, "LOW-END CLASH MAP");
    panel (curvePanel, "COMPRESSION CURVE");
    panel (shapePanel, "RESPONSE SHAPE");
    panel (knobPanel, "");

    // Trigger status lamp.
    auto lamp = scopePanel.reduced (12, 6).removeFromTop (16);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const int textW = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), status) + 4;
    g.setColour (statusColour);
    g.drawText (status, lamp.removeFromRight (textW), juce::Justification::centredRight);
    g.fillEllipse (lamp.removeFromRight (16).toFloat().withSizeKeepingCentre (8.0f, 8.0f));
}

void CruzSidechainAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (16, 0);
    auto header = area.removeFromTop (56);
    header.removeFromLeft (220);
    triggerLabel = header.removeFromLeft (56);
    triggerButtons.setBounds (header.removeFromLeft (228).reduced (4, 12));
    rateLabel = header.removeFromLeft (44);
    rateButtons.setBounds (header.removeFromLeft (205).reduced (4, 12));
    detectLabel = header.removeFromLeft (58);
    detectButtons.setBounds (header.removeFromLeft (118).reduced (4, 12));
    header.removeFromLeft (6);
    splitButton.setBounds (header.removeFromLeft (124).reduced (3, 12));
    listenButton.setBounds (header.removeFromLeft (112).reduced (3, 12));

    area.removeFromBottom (14);
    knobPanel = area.removeFromBottom (200);
    area.removeFromBottom (10);

    auto right = area.removeFromRight (400);
    area.removeFromRight (10);

    const int topH = area.getHeight() * 46 / 100;
    scopePanel = area.removeFromTop (topH);
    area.removeFromTop (10);
    heatPanel = area;

    curvePanel = right.removeFromTop (right.getHeight() / 2 + 20);
    right.removeFromTop (10);
    shapePanel = right;

    scope.setBounds (scopePanel.reduced (10).withTrimmedTop (20));
    heatMap.setBounds (heatPanel.reduced (10).withTrimmedTop (20));
    curve.setBounds (curvePanel.reduced (10).withTrimmedTop (14));
    shape.setBounds (shapePanel.reduced (10).withTrimmedTop (14));

    auto layoutRow = [] (juce::Rectangle<int> row, std::initializer_list<juce::Component*> knobs)
    {
        row.reduce (8, 4);
        const int w = row.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->setBounds (row.removeFromLeft (w).reduced (4, 0));
    };

    auto knobs = knobPanel.reduced (0, 6);
    layoutRow (knobs.removeFromTop (knobs.getHeight() / 2), { &threshold, &ratio, &knee, &range, &attack, &hold, &release, &lookahead });
    layoutRow (knobs, { &keyGain, &keyHighPass, &keyLowPass, &splitFreq, &attackBend, &releaseBend, &mix, &output });
}
