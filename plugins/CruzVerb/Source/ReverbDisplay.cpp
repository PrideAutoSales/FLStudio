#include "ReverbDisplay.h"
#include "Theme.h"

namespace
{
    constexpr double minFreq = 10.0, maxFreq = 22000.0;
    constexpr double minDecay = 0.1, maxDecay = 60.0;
    constexpr double dbRange = 24.0;
    constexpr float nodeRadius = 7.0f;
    constexpr float spectrumFloorDb = -96.0f;
}

ReverbDisplay::ReverbDisplay (CruzVerbAudioProcessor& p)
    : processor (p), state (p.getState())
{
    spectrumDb.fill (spectrumFloorDb);
    startTimerHz (30);
}

void ReverbDisplay::setMode (Mode m)
{
    mode = m;
    hoverNode = dragNode = -1;
    repaint();
}

//==============================================================================
float ReverbDisplay::value (const char* id) const
{
    return state.getRawParameterValue (id)->load();
}

void ReverbDisplay::setValue (const char* id, float v)
{
    if (auto* param = state.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (v));
}

juce::Rectangle<float> ReverbDisplay::plotArea() const
{
    return getLocalBounds().toFloat().reduced (36.0f, 14.0f).withTrimmedBottom (8.0f);
}

float ReverbDisplay::xForFreq (double freq) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * (float) (std::log (freq / minFreq) / std::log (maxFreq / minFreq));
}

double ReverbDisplay::freqForX (float x) const
{
    const auto a = plotArea();
    return minFreq * std::pow (maxFreq / minFreq, (double) ((x - a.getX()) / a.getWidth()));
}

float ReverbDisplay::yForDecay (double seconds) const
{
    const auto a = plotArea();
    const auto t = std::log (juce::jlimit (minDecay, maxDecay, seconds) / minDecay) / std::log (maxDecay / minDecay);
    return a.getBottom() - a.getHeight() * (float) t;
}

double ReverbDisplay::decayForY (float y) const
{
    const auto a = plotArea();
    return minDecay * std::pow (maxDecay / minDecay, (double) ((a.getBottom() - y) / a.getHeight()));
}

float ReverbDisplay::yForDb (double db) const
{
    const auto a = plotArea();
    return a.getCentreY() - (float) (db / dbRange) * a.getHeight() * 0.5f;
}

double ReverbDisplay::dbForY (float y) const
{
    const auto a = plotArea();
    return (a.getCentreY() - y) / (a.getHeight() * 0.5f) * dbRange;
}

//==============================================================================
std::vector<ReverbDisplay::Node> ReverbDisplay::currentNodes() const
{
    using namespace ParamIDs;
    std::vector<Node> nodes;

    if (mode == Mode::decay)
    {
        const auto t = (double) value (decay);
        const auto lf = (double) value (decayLowFreq), hf = (double) value (decayHighFreq);

        nodes.push_back ({ "Low", decayLowFreq, decayLowMult, { xForFreq (lf), yForDecay (t * value (decayLowMult)) } });
        nodes.push_back ({ "Decay", nullptr, ParamIDs::decay, { xForFreq (std::sqrt (lf * hf)), yForDecay (t) } });
        nodes.push_back ({ "High", decayHighFreq, decayHighMult, { xForFreq (hf), yForDecay (t * value (decayHighMult)) } });
    }
    else
    {
        nodes.push_back ({ "Low Cut", lowCut, nullptr, { xForFreq (value (lowCut)), yForDb (0.0) } });
        nodes.push_back ({ "Low Shelf", lowShelfFreq, lowShelfGain, { xForFreq (value (lowShelfFreq)), yForDb (value (lowShelfGain)) } });
        nodes.push_back ({ "High Shelf", highShelfFreq, highShelfGain, { xForFreq (value (highShelfFreq)), yForDb (value (highShelfGain)) } });
        nodes.push_back ({ "High Cut", highCut, nullptr, { xForFreq (value (highCut)), yForDb (0.0) } });
    }

    const auto a = plotArea();
    for (auto& n : nodes)
        n.position = { juce::jlimit (a.getX(), a.getRight(), n.position.x),
                       juce::jlimit (a.getY(), a.getBottom(), n.position.y) };

    return nodes;
}

int ReverbDisplay::nodeAt (juce::Point<float> p) const
{
    const auto nodes = currentNodes();
    int best = -1;
    auto bestDistance = nodeRadius * 2.2f;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto d = nodes[i].position.getDistanceFrom (p);
        if (d < bestDistance)
        {
            bestDistance = d;
            best = (int) i;
        }
    }

    return best;
}

//==============================================================================
void ReverbDisplay::mouseMove (const juce::MouseEvent& e)
{
    const auto n = nodeAt (e.position);
    if (n != hoverNode)
    {
        hoverNode = n;
        setMouseCursor (n >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ReverbDisplay::mouseExit (const juce::MouseEvent&)
{
    hoverNode = -1;
    repaint();
}

void ReverbDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragNode = nodeAt (e.position);
    if (dragNode < 0)
        return;

    const auto node = currentNodes()[(size_t) dragNode];
    for (auto* id : { node.xParam, node.yParam })
        if (id != nullptr)
            state.getParameter (id)->beginChangeGesture();
}

void ReverbDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0)
        return;

    using namespace ParamIDs;
    const auto node = currentNodes()[(size_t) dragNode];
    const auto a = plotArea();
    const auto pos = juce::Point<float> (juce::jlimit (a.getX(), a.getRight(), e.position.x),
                                         juce::jlimit (a.getY(), a.getBottom(), e.position.y));

    if (node.xParam != nullptr)
        setValue (node.xParam, (float) freqForX (pos.x));

    if (node.yParam != nullptr)
    {
        if (mode == Mode::postEq)
            setValue (node.yParam, (float) dbForY (pos.y));
        else if (node.yParam == decay)
            setValue (decay, (float) decayForY (pos.y));
        else
            setValue (node.yParam, (float) (decayForY (pos.y) / value (decay)));
    }

    repaint();
}

void ReverbDisplay::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0)
    {
        const auto node = currentNodes()[(size_t) dragNode];
        for (auto* id : { node.xParam, node.yParam })
            if (id != nullptr)
                state.getParameter (id)->endChangeGesture();
    }

    dragNode = -1;
}

void ReverbDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto index = nodeAt (e.position);
    if (index < 0)
        return;

    // Double-click puts a node back to where it started.
    const auto node = currentNodes()[(size_t) index];
    for (auto* id : { node.xParam, node.yParam })
        if (id != nullptr)
            if (auto* param = state.getParameter (id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->getDefaultValue());
                param->endChangeGesture();
            }
}

//==============================================================================
void ReverbDisplay::timerCallback()
{
    updateSpectrum();
    repaint();
}

void ReverbDisplay::updateSpectrum()
{
    const auto got = processor.getAnalyzerFifo().pull (pullBuffer.data(), (int) pullBuffer.size());

    for (int i = 0; i < got; ++i)
    {
        ring[(size_t) ringPos] = pullBuffer[(size_t) i];
        ringPos = (ringPos + 1) % fftSize;
    }

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];

    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    // Hann window loses half the amplitude; scale so a full-scale sine reads about 0 dB.
    const auto scale = 4.0f / (float) fftSize;

    for (size_t i = 0; i < spectrumDb.size(); ++i)
    {
        const auto db = juce::Decibels::gainToDecibels (fftData[i] * scale, spectrumFloorDb);
        auto& s = spectrumDb[i];
        s = db > s ? db : s + 0.25f * (db - s); // fast rise, slow fall
    }
}

//==============================================================================
void ReverbDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (Theme::panel);
    g.fillRoundedRectangle (bounds, 8.0f);
    g.setColour (Theme::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    const auto a = plotArea();
    drawGrid (g, a);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (a.toNearestInt());
        drawSpectrum (g, a);
        // Each mode has its own vertical scale, so only the active curve is drawn.
        if (mode == Mode::decay)
            drawDecayCurve (g, a);
        else
            drawPostEqCurve (g, a);
    }

    drawNodes (g);
}

void ReverbDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> a) const
{
    g.setFont (juce::FontOptions (11.0f));

    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const auto x = xForFreq (f);
        g.setColour (Theme::grid);
        g.drawVerticalLine (juce::roundToInt (x), a.getY(), a.getBottom());
        g.setColour (Theme::dimText);
        const auto label = f >= 1000.0 ? juce::String (f / 1000.0) + "k" : juce::String ((int) f);
        g.drawText (label, juce::Rectangle<float> (40.0f, 12.0f).withCentre ({ x, a.getBottom() + 9.0f }),
                    juce::Justification::centred);
    }

    if (mode == Mode::decay)
    {
        for (double s : { 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0 })
        {
            const auto y = yForDecay (s);
            g.setColour (Theme::grid);
            g.drawHorizontalLine (juce::roundToInt (y), a.getX(), a.getRight());
            g.setColour (Theme::decay.withAlpha (0.7f));
            g.drawText (juce::String (s) + "s", juce::Rectangle<float> (a.getX() - 34.0f, y - 6.0f, 30.0f, 12.0f),
                        juce::Justification::centredRight);
        }
    }
    else
    {
        for (double db : { -18.0, -12.0, -6.0, 0.0, 6.0, 12.0, 18.0 })
        {
            const auto y = yForDb (db);
            g.setColour (std::abs (db) < 0.1 ? Theme::panelEdge.brighter (0.2f) : Theme::grid);
            g.drawHorizontalLine (juce::roundToInt (y), a.getX(), a.getRight());
            g.setColour (Theme::postEq.withAlpha (0.7f));
            g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (a.getX() - 34.0f, y - 6.0f, 30.0f, 12.0f),
                        juce::Justification::centredRight);
        }
    }
}

void ReverbDisplay::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> a) const
{
    const auto fs = processor.getCurrentSampleRate();
    juce::Path p;
    p.startNewSubPath (a.getX(), a.getBottom());

    for (auto x = a.getX(); x <= a.getRight(); x += 2.0f)
    {
        const auto bin = freqForX (x) / fs * fftSize;
        const auto i = juce::jlimit (0, (int) spectrumDb.size() - 2, (int) bin);
        const auto frac = (float) juce::jlimit (0.0, 1.0, bin - i);
        const auto db = spectrumDb[(size_t) i] + frac * (spectrumDb[(size_t) i + 1] - spectrumDb[(size_t) i]);

        // Tilt by +4.5 dB per octave so a reverb tail looks roughly flat, like pink noise.
        const auto tilted = db + 4.5f * (float) std::log2 (juce::jmax (20.0, freqForX (x)) / 1000.0);
        const auto t = juce::jlimit (0.0f, 1.0f, (tilted - spectrumFloorDb) / -spectrumFloorDb);
        p.lineTo (x, a.getBottom() - t * a.getHeight());
    }

    p.lineTo (a.getRight(), a.getBottom());
    p.closeSubPath();

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.13f), 0.0f, a.getY(),
                                             juce::Colours::white.withAlpha (0.02f), 0.0f, a.getBottom(), false));
    g.fillPath (p);
}

void ReverbDisplay::drawDecayCurve (juce::Graphics& g, juce::Rectangle<float> a) const
{
    using namespace ParamIDs;
    const auto fs = processor.getCurrentSampleRate();
    const auto frozen = value (freeze) > 0.5f;
    const auto colour = frozen ? Theme::freeze : Theme::decay;

    juce::Path curve;

    if (frozen)
    {
        curve.startNewSubPath (a.getX(), a.getY() + 2.0f);
        curve.lineTo (a.getRight(), a.getY() + 2.0f);
    }
    else
    {
        // Use the same filter design as the engine so the line shows the real decay.
        const auto delaySamples = cruzverb::ReverbEngine::referenceDelaySeconds (value (space)) * fs;
        const auto filter = cruzverb::ReverbEngine::designLoopFilter (delaySamples, fs, value (decay),
                                                                      value (decayLowFreq), value (decayLowMult),
                                                                      value (decayHighFreq), value (decayHighMult), false);

        auto firstPoint = true;

        for (auto x = a.getX(); x <= a.getRight(); x += 2.0f)
        {
            const auto freq = juce::jmin (freqForX (x), fs * 0.49);
            const auto db = 20.0 * std::log10 (juce::jmax (1.0e-9, filter.magnitude (freq, fs)));
            const auto seconds = db < -1.0e-9 ? -60.0 * delaySamples / (fs * db) : maxDecay;
            const auto y = yForDecay (seconds);

            if (std::exchange (firstPoint, false))
                curve.startNewSubPath (x, y);
            else
                curve.lineTo (x, y);
        }
    }

    {
        juce::Path fill (curve);
        fill.lineTo (a.getRight(), a.getBottom());
        fill.lineTo (a.getX(), a.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.22f), 0.0f, a.getY(),
                                                 colour.withAlpha (0.0f), 0.0f, a.getBottom(), false));
        g.fillPath (fill);
    }

    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (2.2f));

    if (frozen)
    {
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("FROZEN: tail holds forever", a.reduced (8.0f, 6.0f).removeFromTop (16.0f),
                    juce::Justification::centredLeft);
    }
}

void ReverbDisplay::drawPostEqCurve (juce::Graphics& g, juce::Rectangle<float> a) const
{
    using namespace ParamIDs;
    const auto fs = processor.getCurrentSampleRate();
    const auto eq = cruzverb::ReverbEngine::designPostEq (fs, value (lowCut), value (highCut),
                                                          value (lowShelfFreq), value (lowShelfGain),
                                                          value (highShelfFreq), value (highShelfGain));
    juce::Path curve;
    auto firstPoint = true;

    for (auto x = a.getX(); x <= a.getRight(); x += 2.0f)
    {
        const auto freq = juce::jmin (freqForX (x), fs * 0.499);
        const auto mag = eq.lowCut.magnitude (freq, fs) * eq.lowShelf.magnitude (freq, fs)
                       * eq.highShelf.magnitude (freq, fs) * eq.highCut.magnitude (freq, fs);
        const auto y = yForDb (20.0 * std::log10 (juce::jmax (1.0e-6, mag)));

        if (std::exchange (firstPoint, false))
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    {
        juce::Path fill (curve);
        fill.lineTo (a.getRight(), a.getCentreY());
        fill.lineTo (a.getX(), a.getCentreY());
        fill.closeSubPath();
        g.setColour (Theme::postEq.withAlpha (0.12f));
        g.fillPath (fill);
    }

    g.setColour (Theme::postEq);
    g.strokePath (curve, juce::PathStrokeType (2.2f));
}

void ReverbDisplay::drawNodes (juce::Graphics& g) const
{
    const auto nodes = currentNodes();
    const auto colour = mode == Mode::decay ? Theme::decay : Theme::postEq;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        const auto& n = nodes[i];
        const auto highlighted = (int) i == hoverNode || (int) i == dragNode;
        const auto r = highlighted ? nodeRadius + 2.0f : nodeRadius;
        const auto circle = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (n.position);

        g.setColour (Theme::panel);
        g.fillEllipse (circle);
        g.setColour (colour);
        g.drawEllipse (circle, 2.0f);

        if (highlighted)
            g.fillEllipse (circle.reduced (r * 0.45f));
    }

    const auto shown = dragNode >= 0 ? dragNode : hoverNode;
    if (shown < 0 || shown >= (int) nodes.size())
        return;

    // Read-out next to the active node.
    const auto& n = nodes[(size_t) shown];
    juce::String text = n.label;

    for (auto* id : { n.xParam, n.yParam })
        if (id != nullptr)
            if (auto* param = state.getParameter (id))
                text << "   " << param->getCurrentValueAsText() << " " << param->getLabel();

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    const auto width = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 16.0f;
    auto box = juce::Rectangle<float> (width, 20.0f).withCentre (n.position.translated (0.0f, -22.0f));
    box = box.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));

    g.setColour (Theme::background.withAlpha (0.9f));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (Theme::text);
    g.drawText (text, box, juce::Justification::centred);
}
