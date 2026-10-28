#include "EqDisplay.h"
#include "Theme.h"

using Params::BandField;
using namespace cruz::dsp;

namespace
{
juce::String formatGain (double db)
{
    return (db > 0.0 ? "+" : "") + juce::String (db, 1) + " dB";
}

bool operator!= (const BandParams& a, const BandParams& b) noexcept
{
    return a.shape != b.shape || a.slopeDbPerOct != b.slopeDbPerOct || ! exactlyEqual (a.freqHz, b.freqHz)
        || ! exactlyEqual (a.gainDb, b.gainDb) || ! exactlyEqual (a.q, b.q);
}

bool operator!= (const DynamicParams& a, const DynamicParams& b) noexcept
{
    return a.enabled != b.enabled || a.autoThreshold != b.autoThreshold || ! exactlyEqual (a.rangeDb, b.rangeDb);
}

const char* placementLetter (Placement p) noexcept
{
    switch (p)
    {
        case Placement::left: return "L";
        case Placement::right: return "R";
        case Placement::mid: return "M";
        case Placement::side: return "S";
        case Placement::stereo: break;
    }
    return "";
}

// Which of the two group curves a band belongs to: 0 = left/mid, 1 = right/side.
bool inGroup (Placement p, int group) noexcept
{
    if (p == Placement::stereo)
        return true;

    const bool first = p == Placement::left || p == Placement::mid;
    return first == (group == 0);
}
} // namespace

EqDisplay::EqDisplay (CruzEQAudioProcessor& p)
    : processor (p), state (p.getState())
{
    setOpaque (true);
    refreshBandCache();
    startTimerHz (60);
}

//==============================================================================
juce::Rectangle<float> EqDisplay::plotArea() const noexcept
{
    return getLocalBounds().toFloat().reduced (1.0f);
}

float EqDisplay::xForFreq (double hz) const noexcept
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * (float) (std::log (hz / minDisplayHz) / std::log (maxDisplayHz / minDisplayHz));
}

double EqDisplay::freqForX (float x) const noexcept
{
    const auto a = plotArea();
    return minDisplayHz * std::pow (maxDisplayHz / minDisplayHz, (double) ((x - a.getX()) / a.getWidth()));
}

float EqDisplay::yForDb (double db) const noexcept
{
    const auto a = plotArea();
    return a.getCentreY() - (float) (db / rangeDb) * (a.getHeight() * 0.5f - 14.0f);
}

double EqDisplay::dbForY (float y) const noexcept
{
    const auto a = plotArea();
    return (a.getCentreY() - y) / (a.getHeight() * 0.5f - 14.0f) * rangeDb;
}

float EqDisplay::yForAnalyzerDb (float db) const noexcept
{
    const auto a = plotArea();
    return a.getY() + a.getHeight() * (db / analyzerFloorDb);
}

//==============================================================================
void EqDisplay::resized()
{
    const auto area = plotArea();
    const int numPoints = juce::jmax (2, (int) area.getWidth() / pointSpacing + 1);

    pointFreqs.resize ((size_t) numPoints);
    for (int i = 0; i < numPoints; ++i)
        pointFreqs[(size_t) i] = freqForX (area.getX() + area.getWidth() * (float) i / (float) (numPoints - 1));

    preLevels.assign ((size_t) numPoints, analyzerFloorDb - 30.0f);
    postLevels.assign ((size_t) numPoints, analyzerFloorDb - 30.0f);
    recomputeCurves();
}

void EqDisplay::setRangeDb (float newRange)
{
    rangeDb = newRange;
    repaint();
}

void EqDisplay::setSelectedBand (int band)
{
    selected = band;
    curvesDirty = true;
    repaint();
}

void EqDisplay::select (int band)
{
    if (selected == band)
        return;

    selected = band;
    curvesDirty = true;

    if (onSelectionChanged != nullptr)
        onSelectionChanged (band);

    repaint();
}

//==============================================================================
void EqDisplay::timerCallback()
{
    updateAnalyzers();

    if (refreshBandCache() || curvesDirty)
        recomputeCurves();

    if (selected >= 0 && ! cache[(size_t) selected].used)
        select (-1);

    repaint();
}

bool EqDisplay::refreshBandCache()
{
    bool changed = false;
    const double sampleRate = processor.getCurrentSampleRate();

    if (! exactlyEqual (sampleRate, cachedSampleRate))
    {
        cachedSampleRate = sampleRate;
        changed = true;
    }

    const double scale = state.getRawParameterValue (Params::gainScale)->load() / 100.0;

    if (! exactlyEqual (scale, gainScale))
    {
        gainScale = scale;
        changed = true;
    }

    for (int b = 0; b < Params::numBands; ++b)
    {
        const auto& refs = processor.getBand (b);
        const CachedBand now { refs.isUsed(), refs.isActive(), refs.read(), refs.placement(), refs.dynamics(), processor.getDynamicGainDb (b) };
        auto& cached = cache[(size_t) b];

        // The live gain move only redraws when it has moved visibly.
        const bool dynamicMoved = std::abs (now.dynamicDb - cached.dynamicDb) > 0.05f
                               || (exactlyEqual (now.dynamicDb, 0.0f) != exactlyEqual (cached.dynamicDb, 0.0f));

        if (now.used != cached.used || now.active != cached.active || now.params != cached.params || now.placement != cached.placement
            || now.dynamics != cached.dynamics || dynamicMoved)
        {
            cached = now;
            changed = true;
        }
    }

    return changed;
}

void EqDisplay::recomputeCurves()
{
    const double sampleRate = cachedSampleRate > 0.0 ? cachedSampleRate : 48000.0;
    const size_t n = pointFreqs.size();

    for (auto& total : totalCurves)
        total.assign (n, 0.0f);

    staticCurve.clear();
    rangeCurve.clear();

    bool usesLeftRight = false, usesMidSide = false;

    for (int b = 0; b < Params::numBands; ++b)
    {
        auto& curve = bandCurves[(size_t) b];
        const auto& band = cache[(size_t) b];

        if (! band.used)
        {
            curve.clear();
            continue;
        }

        auto params = band.params;
        params.gainDb = params.gainDb * gainScale + (band.isDynamic() && band.active ? band.dynamicDb : 0.0);
        const auto design = designBand (params, sampleRate);
        curve.resize (n);

        if (b == selected && band.isDynamic())
        {
            auto quiet = band.params, loud = band.params;
            quiet.gainDb *= gainScale;
            loud.gainDb = (loud.gainDb + band.dynamics.rangeDb) * gainScale;
            const auto quietDesign = designBand (quiet, sampleRate), loudDesign = designBand (loud, sampleRate);
            staticCurve.resize (n);
            rangeCurve.resize (n);

            for (size_t i = 0; i < n; ++i)
            {
                const double w = std::min (2.0 * pi * pointFreqs[i] / sampleRate, 0.9999 * pi);
                staticCurve[i] = (float) quietDesign.magnitudeDb (w);
                rangeCurve[i] = (float) loudDesign.magnitudeDb (w);
            }
        }

        if (band.active)
        {
            usesLeftRight |= band.placement == Placement::left || band.placement == Placement::right;
            usesMidSide |= band.placement == Placement::mid || band.placement == Placement::side;
        }

        for (size_t i = 0; i < n; ++i)
        {
            // Above Nyquist the curve holds its value at Nyquist.
            const double w = std::min (2.0 * pi * pointFreqs[i] / sampleRate, 0.9999 * pi);
            curve[i] = (float) design.magnitudeDb (w);

            if (band.active)
                for (int group = 0; group < 2; ++group)
                    if (inGroup (band.placement, group))
                        totalCurves[(size_t) group][i] += curve[i];
        }
    }

    splitCurves = usesLeftRight || usesMidSide;
    groupNames[0] = usesLeftRight && usesMidSide ? "Left + Mid" : usesLeftRight ? "Left" : "Mid";
    groupNames[1] = usesLeftRight && usesMidSide ? "Right + Side" : usesLeftRight ? "Right" : "Side";
    curvesDirty = false;
}

void EqDisplay::updateAnalyzers()
{
    const double sampleRate = processor.getCurrentSampleRate();

    const auto update = [this, sampleRate] (SpectrumAnalyzer& analyzer, AnalyzerFifo& fifo, std::vector<float>& levels, int& idleFrames)
    {
        const bool fresh = analyzer.update (fifo);
        idleFrames = fresh ? 0 : idleFrames + 1;

        // Hold the last spectrum between audio blocks, but fall away if the
        // host stops sending audio altogether.
        if (! fresh && idleFrames < 30)
            return;

        for (size_t i = 0; i < levels.size(); ++i)
        {
            float target = analyzerFloorDb - 30.0f;

            if (fresh)
            {
                const double f = pointFreqs[i];
                const double lo = i > 0 ? std::sqrt (pointFreqs[i - 1] * f) : f;
                const double hi = i + 1 < levels.size() ? std::sqrt (pointFreqs[i + 1] * f) : f;
                target = analyzer.levelDb (lo, hi, sampleRate) + analyzerTiltDbPerOct * (float) std::log2 (f / 1000.0);
            }

            auto& level = levels[i];
            level = target > level ? target : level + (target - level) * 0.12f;
        }
    };

    update (preAnalyzer, processor.preEqFifo, preLevels, preIdleFrames);
    update (postAnalyzer, processor.postEqFifo, postLevels, postIdleFrames);
}

//==============================================================================
juce::Point<float> EqDisplay::nodePosition (int band) const noexcept
{
    const auto area = plotArea();
    const auto& c = cache[(size_t) band];
    const float x = xForFreq (std::clamp (c.params.freqHz, minDisplayHz, maxDisplayHz));
    double db = 0.0;

    if (shapeUsesGain (c.params.shape))
    {
        db = c.params.gainDb * gainScale;
    }
    else if ((c.params.shape == Shape::lowCut || c.params.shape == Shape::highCut) && ! bandCurves[(size_t) band].empty())
    {
        // Cuts sit on their own curve at the cutoff frequency.
        const auto& curve = bandCurves[(size_t) band];
        const int i = juce::jlimit (0, (int) curve.size() - 1,
                                    juce::roundToInt ((x - area.getX()) / area.getWidth() * (float) (curve.size() - 1)));
        db = curve[(size_t) i];
    }

    return { x, juce::jlimit (area.getY() + 8.0f, area.getBottom() - 8.0f, yForDb (db)) };
}

int EqDisplay::nodeAt (juce::Point<float> position) const noexcept
{
    constexpr float radius = 11.0f;

    if (selected >= 0 && cache[(size_t) selected].used && nodePosition (selected).getDistanceFrom (position) <= radius)
        return selected;

    int best = -1;
    float bestDistance = radius;

    for (int b = Params::numBands - 1; b >= 0; --b)
    {
        if (! cache[(size_t) b].used)
            continue;

        const float d = nodePosition (b).getDistanceFrom (position);
        if (d <= bestDistance)
        {
            best = b;
            bestDistance = d;
        }
    }

    return best;
}

bool EqDisplay::hasRangeHandle (int band) const noexcept
{
    return band >= 0 && cache[(size_t) band].used && shapeUsesDynamics (cache[(size_t) band].params.shape);
}

juce::Point<float> EqDisplay::rangeHandlePosition (int band) const noexcept
{
    const auto area = plotArea();
    const auto pos = nodePosition (band);
    const auto& c = cache[(size_t) band];
    constexpr float minGap = 14.0f, restingGap = 26.0f;

    // Before the band is dynamic the handle rests just below the dot (above
    // it near the bottom), ready to pull a range out of.
    float y = pos.y + (pos.y + restingGap < area.getBottom() - 6.0f ? restingGap : -restingGap);

    if (c.isDynamic())
    {
        y = yForDb ((c.params.gainDb + c.dynamics.rangeDb) * gainScale);

        // Keep it clear of the dot so it can still be grabbed at small ranges.
        if (std::abs (y - pos.y) < minGap)
            y = pos.y + (c.dynamics.rangeDb > 0.0 ? -minGap : minGap);
    }

    return { pos.x, juce::jlimit (area.getY() + 6.0f, area.getBottom() - 6.0f, y) };
}

bool EqDisplay::rangeHandleAt (juce::Point<float> position) const noexcept
{
    return hasRangeHandle (selected) && rangeHandlePosition (selected).getDistanceFrom (position) <= 7.0f
        && nodePosition (selected).getDistanceFrom (position) > 8.0f;
}

double EqDisplay::dbPerPixel() const noexcept
{
    // Divided by gain scale so dots and handles follow the mouse.
    return rangeDb / (plotArea().getHeight() * 0.5 - 14.0) / vScale();
}

Shape EqDisplay::shapeForClick (double hz) noexcept
{
    if (hz < lowCutBelowHz)
        return Shape::lowCut;
    if (hz < lowShelfBelowHz)
        return Shape::lowShelf;
    if (hz > highCutAboveHz)
        return Shape::highCut;
    if (hz > highShelfAboveHz)
        return Shape::highShelf;
    return Shape::bell;
}

//==============================================================================
juce::RangedAudioParameter& EqDisplay::param (int band, BandField field) const
{
    return *state.getParameter (Params::bandId (band, field));
}

float EqDisplay::paramValue (int band, BandField field) const
{
    return state.getRawParameterValue (Params::bandId (band, field))->load();
}

void EqDisplay::setParam (int band, BandField field, float value)
{
    auto& p = param (band, field);
    p.setValueNotifyingHost (p.convertTo0to1 (value));
}

void EqDisplay::setParamGesture (int band, BandField field, float value)
{
    auto& p = param (band, field);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.convertTo0to1 (value));
    p.endChangeGesture();
}

void EqDisplay::resetParam (int band, BandField field)
{
    auto& p = param (band, field);
    p.beginChangeGesture();
    p.setValueNotifyingHost (p.getDefaultValue());
    p.endChangeGesture();
}

void EqDisplay::beginDrag (int band, juce::Point<float> position, bool fine)
{
    anchor.mouse = position;
    anchor.logFreq = std::log (paramValue (band, BandField::freq));
    anchor.gainDb = paramValue (band, BandField::gain);
    anchor.logQ = std::log (paramValue (band, BandField::q));
    anchor.fine = fine;
}

void EqDisplay::makeDynamic (int band)
{
    if (paramValue (band, BandField::dynamic) >= 0.5f)
        return;

    // A band made dynamic from the display starts on the automatic threshold.
    setParamGesture (band, BandField::threshold, (float) maxThresholdDb);
    setParamGesture (band, BandField::dynamic, 1.0f);
}

void EqDisplay::startBandDrag (int band, juce::Point<float> position, bool fine)
{
    dragging = band;
    for (auto field : { BandField::freq, BandField::gain, BandField::q })
        param (band, field).beginChangeGesture();

    beginDrag (band, position, fine);
}

int EqDisplay::createBandAt (juce::Point<float> position, bool dynamic)
{
    int band = -1;
    for (int b = 0; b < Params::numBands && band < 0; ++b)
        if (! cache[(size_t) b].used)
            band = b;

    if (band < 0)
        return -1;

    const double freq = std::clamp (freqForX (position.x), minFreqHz, maxFreqHz);
    const auto shape = shapeForClick (freq);
    const double gain = shapeUsesGain (shape) ? std::clamp (std::round (dbForY (position.y) / vScale() * 10.0) / 10.0, minGainDb, maxGainDb)
                                              : 0.0;

    // Start from the defaults, so nothing is left over from an earlier band
    // in this slot.
    for (auto field : { BandField::q, BandField::slope, BandField::place, BandField::bypass, BandField::dynamic,
                        BandField::range, BandField::threshold, BandField::attack, BandField::release })
        resetParam (band, field);

    setParamGesture (band, BandField::shape, (float) shape);
    setParamGesture (band, BandField::freq, (float) freq);
    setParamGesture (band, BandField::gain, (float) gain);

    if (dynamic && shapeUsesDynamics (shape))
        makeDynamic (band);

    setParamGesture (band, BandField::used, 1.0f);

    refreshBandCache();
    recomputeCurves();
    select (band);
    return band;
}

void EqDisplay::updatePreview (juce::Point<float> position)
{
    const bool full = std::all_of (cache.begin(), cache.end(), [] (const CachedBand& b) { return b.used; });
    showPreview = ! full && plotArea().contains (position);

    if (! showPreview)
        return;

    previewPoint = position;
    const double freq = std::clamp (freqForX (position.x), minFreqHz, maxFreqHz);
    const auto shape = shapeForClick (freq);
    const double gain = shapeUsesGain (shape) ? std::clamp (dbForY (position.y) / vScale(), minGainDb, maxGainDb) : 0.0;
    previewParams = { shape, freq, gain, 1.0, slopesDbPerOct[1] };

    const double sampleRate = cachedSampleRate > 0.0 ? cachedSampleRate : 48000.0;
    auto shown = previewParams;
    shown.gainDb *= gainScale;
    const auto design = designBand (shown, sampleRate);
    previewCurve.resize (pointFreqs.size());

    for (size_t i = 0; i < pointFreqs.size(); ++i)
        previewCurve[i] = (float) design.magnitudeDb (std::min (2.0 * pi * pointFreqs[i] / sampleRate, 0.9999 * pi));
}

void EqDisplay::removeBand (int band)
{
    setParamGesture (band, BandField::used, 0.0f);

    if (selected == band)
        select (-1);

    hovered = -1;
    refreshBandCache();
    recomputeCurves();
}

void EqDisplay::showBandMenu (int band)
{
    const int currentShape = juce::roundToInt (paramValue (band, BandField::shape));
    const int currentSlope = juce::roundToInt (paramValue (band, BandField::slope));
    const int currentPlace = juce::roundToInt (paramValue (band, BandField::place));
    const bool bypassed = paramValue (band, BandField::bypass) >= 0.5f;
    const bool dynamic = paramValue (band, BandField::dynamic) >= 0.5f;

    juce::PopupMenu shapes, slopes, places, menu;

    for (int i = 0; i < Params::shapeNames().size(); ++i)
        shapes.addItem (100 + i, Params::shapeNames()[i], true, i == currentShape);

    for (int i = 0; i < Params::slopeNames().size(); ++i)
        slopes.addItem (200 + i, Params::slopeNames()[i], true, i == currentSlope);

    for (int i = 0; i < Params::placementNames().size(); ++i)
        places.addItem (300 + i, Params::placementNames()[i], true, i == currentPlace);

    menu.addSectionHeader ("Band " + juce::String (band + 1));
    menu.addSubMenu ("Shape", shapes);
    menu.addSubMenu ("Slope", slopes, shapeUsesSlope ((Shape) currentShape));
    menu.addSubMenu ("Stereo placement", places);
    menu.addSeparator();
    menu.addItem (1, "Bypass", true, bypassed);
    menu.addItem (3, "Dynamic", shapeUsesDynamics ((Shape) currentShape), dynamic);
    menu.addItem (2, "Remove band");

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                        [safe = juce::Component::SafePointer<EqDisplay> (this), band, bypassed, dynamic] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            if (result >= 300)
                                safe->setParamGesture (band, BandField::place, (float) (result - 300));
                            else if (result >= 200)
                                safe->setParamGesture (band, BandField::slope, (float) (result - 200));
                            else if (result >= 100)
                                safe->setParamGesture (band, BandField::shape, (float) (result - 100));
                            else if (result == 1)
                                safe->setParamGesture (band, BandField::bypass, bypassed ? 0.0f : 1.0f);
                            else if (result == 2)
                                safe->removeBand (band);
                            else if (result == 3)
                                safe->setParamGesture (band, BandField::dynamic, dynamic ? 0.0f : 1.0f);
                        });
}

//==============================================================================
void EqDisplay::mouseMove (const juce::MouseEvent& e)
{
    const bool onHandle = rangeHandleAt (e.position);
    const int band = onHandle ? -1 : nodeAt (e.position);

    hovered = band;
    rangeHandleHovered = onHandle;
    setMouseCursor (onHandle ? juce::MouseCursor::UpDownResizeCursor
                             : band >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);

    if (band < 0 && ! onHandle)
        updatePreview (e.position);
    else
        showPreview = false;

    repaint();
}

void EqDisplay::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    rangeHandleHovered = false;
    showPreview = false;
    repaint();
}

void EqDisplay::mouseDown (const juce::MouseEvent& e)
{
    showPreview = false;

    if (! e.mods.isPopupMenu() && rangeHandleAt (e.position))
    {
        const auto& c = cache[(size_t) selected];
        draggingRange = selected;
        rangeDragTurnedOn = false;
        rangeAnchorMouse = e.position;

        // Before the band is dynamic, the range starts from where the resting
        // handle is drawn, so nothing jumps.
        rangeAnchorDb = c.isDynamic() ? c.dynamics.rangeDb
                                      : (nodePosition (selected).y - rangeHandlePosition (selected).y) * dbPerPixel();

        param (selected, BandField::range).beginChangeGesture();
        return;
    }

    const int band = nodeAt (e.position);

    // The second click of a double-click that added a band is not a new click.
    if (e.getNumberOfClicks() > 1 && lastCreated >= 0)
        return;

    lastCreated = -1;

    if (band < 0)
    {
        if (e.mods.isPopupMenu())
            return;

        const int created = createBandAt (e.position, e.mods.isAltDown());

        if (created < 0)
        {
            select (-1);
            return;
        }

        // Keep holding the button to place the new band.
        lastCreated = created;
        startBandDrag (created, e.position, e.mods.isShiftDown());
        return;
    }

    select (band);

    if (e.mods.isPopupMenu())
    {
        showBandMenu (band);
        return;
    }

    if (e.mods.isAltDown())
    {
        setParamGesture (band, BandField::bypass, paramValue (band, BandField::bypass) >= 0.5f ? 0.0f : 1.0f);
        return;
    }

    startBandDrag (band, e.position, e.mods.isShiftDown());
}

void EqDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingRange >= 0)
    {
        const float dy = (e.position.y - rangeAnchorMouse.y) * (e.mods.isShiftDown() ? 0.1f : 1.0f);

        // Dragging the handle is what makes the band dynamic.
        if (! rangeDragTurnedOn && std::abs (e.position.y - rangeAnchorMouse.y) > 2.0f)
        {
            rangeDragTurnedOn = true;
            makeDynamic (draggingRange);
        }

        if (rangeDragTurnedOn)
            setParam (draggingRange, BandField::range,
                      (float) std::clamp (rangeAnchorDb - dy * dbPerPixel(), minDynamicRangeDb, maxDynamicRangeDb));
        return;
    }

    if (dragging < 0)
        return;

    // Shift gives fine control; re-anchor when it is pressed or released.
    const bool fine = e.mods.isShiftDown();
    if (fine != anchor.fine)
        beginDrag (dragging, e.position, fine);

    const auto area = plotArea();
    const auto delta = (e.position - anchor.mouse) * (fine ? 0.1f : 1.0f);

    const double logFreq = anchor.logFreq + delta.x / area.getWidth() * std::log (maxDisplayHz / minDisplayHz);
    setParam (dragging, BandField::freq, (float) std::clamp (std::exp (logFreq), minFreqHz, maxFreqHz));

    if (shapeUsesGain (cache[(size_t) dragging].params.shape))
    {
        setParam (dragging, BandField::gain, (float) std::clamp (anchor.gainDb - delta.y * dbPerPixel(), minGainDb, maxGainDb));
    }
    else
    {
        // Shapes without gain use vertical movement for Q: up is narrower.
        setParam (dragging, BandField::q, (float) std::clamp (std::exp (anchor.logQ - delta.y * 0.02), minQ, maxQ));
    }
}

void EqDisplay::mouseUp (const juce::MouseEvent&)
{
    if (draggingRange >= 0)
    {
        param (draggingRange, BandField::range).endChangeGesture();
        draggingRange = -1;
        return;
    }

    if (dragging < 0)
        return;

    for (auto field : { BandField::freq, BandField::gain, BandField::q })
        param (dragging, field).endChangeGesture();

    dragging = -1;
}

void EqDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    const int band = nodeAt (e.position);

    // A double-click on empty space already added its band on the first click.
    if (band >= 0 && band != lastCreated)
        removeBand (band);

    lastCreated = -1;
}

void EqDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int band = hovered >= 0 ? hovered : nodeAt (e.position);

    if (band < 0)
        return;

    const float amount = std::abs (wheel.deltaY) > 0.0f ? wheel.deltaY : -wheel.deltaX;
    const double dbStep = amount * (e.mods.isShiftDown() ? 1.0 : 6.0);
    const auto shape = cache[(size_t) band].params.shape;

    if (e.mods.isAltDown() && shapeUsesDynamics (shape))
    {
        // Alt-scroll sets the dynamic range, making the band dynamic.
        makeDynamic (band);

        const double range = paramValue (band, BandField::range) + dbStep;
        setParamGesture (band, BandField::range, (float) std::clamp (range, minDynamicRangeDb, maxDynamicRangeDb));
    }
    else if (e.mods.isCommandDown() && shapeUsesGain (shape))
    {
        const double gain = paramValue (band, BandField::gain) + dbStep;
        setParamGesture (band, BandField::gain, (float) std::clamp (gain, minGainDb, maxGainDb));
    }
    else
    {
        const double q = paramValue (band, BandField::q) * std::exp (amount * (e.mods.isShiftDown() ? 0.3 : 1.5));
        setParamGesture (band, BandField::q, (float) std::clamp (q, minQ, maxQ));
    }
}

//==============================================================================
void EqDisplay::paint (juce::Graphics& g)
{
    const auto area = plotArea();
    g.fillAll (Theme::background);

    drawGrid (g, area);

    {
        juce::Graphics::ScopedSaveState saved (g);
        g.reduceClipRegion (area.toNearestInt());
        drawAnalyzer (g, area);
        drawCurves (g, area);
    }

    drawLegend (g, area);
    drawPreview (g, area);
    drawNodes (g);
    drawBandInfo (g, area);

    if (std::none_of (cache.begin(), cache.end(), [] (const CachedBand& b) { return b.used; }))
    {
        g.setColour (Theme::dimText);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Click anywhere to add a band", area.withTrimmedTop (area.getHeight() * 0.35f).withHeight (24.0f),
                    juce::Justification::centred);
    }
}

void EqDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (Theme::display);
    g.fillRoundedRectangle (area, 6.0f);

    for (double decade = 10.0; decade <= 10000.0; decade *= 10.0)
    {
        for (int m = 1; m <= 9; ++m)
        {
            const double f = decade * m;
            if (f < minDisplayHz || f > maxDisplayHz)
                continue;

            const bool major = m == 1 || m == 2 || m == 5;
            g.setColour (major ? Theme::gridMajor : Theme::gridMinor);
            g.drawVerticalLine (juce::roundToInt (xForFreq (f)), area.getY(), area.getBottom());
        }
    }

    const float step = rangeDb <= 12.0f ? rangeDb / 4.0f : 6.0f;

    for (float db = -rangeDb; db <= rangeDb + 0.001f; db += step)
    {
        g.setColour (std::abs (db) < 0.001f ? Theme::zeroLine : Theme::gridMajor);
        g.drawHorizontalLine (juce::roundToInt (yForDb (db)), area.getX(), area.getRight());
    }

    g.setFont (juce::FontOptions (11.0f));
    g.setColour (Theme::dimText);

    for (double f : { 20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0 })
    {
        const auto label = f < 1000.0 ? juce::String ((int) f) : juce::String ((int) (f / 1000.0)) + "k";
        g.drawText (label, juce::Rectangle<float> (xForFreq (f) + 3.0f, area.getBottom() - 16.0f, 40.0f, 14.0f),
                    juce::Justification::centredLeft, false);
    }

    for (float db = -rangeDb + step; db < rangeDb; db += step)
    {
        const auto label = (db > 0.0f ? "+" : "") + juce::String (db, step < 1.0f ? 1 : 0);
        g.drawText (label, juce::Rectangle<float> (area.getRight() - 38.0f, yForDb (db) - 7.0f, 34.0f, 14.0f),
                    juce::Justification::centredRight, false);
    }
}

void EqDisplay::drawAnalyzer (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (pointFreqs.empty())
        return;

    // The spectrum ends at Nyquist.
    const double nyquist = 0.5 * processor.getCurrentSampleRate();
    size_t numPoints = 0;
    while (numPoints < pointFreqs.size() && pointFreqs[numPoints] < nyquist)
        ++numPoints;

    if (numPoints < 2)
        return;

    const float lastX = xForFreq (pointFreqs[numPoints - 1]);

    const auto buildLine = [&] (const std::vector<float>& levels)
    {
        juce::Path line;
        for (size_t i = 0; i < numPoints; ++i)
        {
            const float x = xForFreq (pointFreqs[i]);
            const float y = juce::jlimit (area.getY(), area.getBottom() + 2.0f, yForAnalyzerDb (levels[i]));

            if (i == 0)
                line.startNewSubPath (x, y);
            else
                line.lineTo (x, y);
        }
        return line;
    };

    const auto preLine = buildLine (preLevels);
    g.setColour (Theme::analyzerPre.withAlpha (0.8f));
    g.strokePath (preLine, juce::PathStrokeType (1.0f));

    const auto postLine = buildLine (postLevels);
    auto postFill = postLine;
    postFill.lineTo (lastX, area.getBottom() + 2.0f);
    postFill.lineTo (area.getX(), area.getBottom() + 2.0f);
    postFill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (Theme::analyzerPost.withAlpha (0.38f), 0.0f, area.getY(),
                                             Theme::analyzerPost.withAlpha (0.04f), 0.0f, area.getBottom(), false));
    g.fillPath (postFill);
    g.setColour (Theme::analyzerPost.withAlpha (0.75f));
    g.strokePath (postLine, juce::PathStrokeType (1.0f));
}

void EqDisplay::drawCurves (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (pointFreqs.empty())
        return;

    const float zeroY = yForDb (0.0);
    const auto curvePath = [&] (const std::vector<float>& curve, bool closeToZero)
    {
        juce::Path path;

        if (closeToZero)
            path.startNewSubPath (xForFreq (pointFreqs.front()), zeroY);

        for (size_t i = 0; i < curve.size(); ++i)
        {
            const float x = xForFreq (pointFreqs[i]);
            const float y = juce::jlimit (area.getY() - 4.0f, area.getBottom() + 4.0f, yForDb (curve[i]));

            if (i == 0 && ! closeToZero)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        if (closeToZero)
        {
            path.lineTo (xForFreq (pointFreqs.back()), zeroY);
            path.closeSubPath();
        }

        return path;
    };

    if (selected >= 0 && ! staticCurve.empty() && staticCurve.size() == rangeCurve.size())
    {
        // Shaded area the selected dynamic band can move through.
        juce::Path shaded;
        shaded.startNewSubPath (xForFreq (pointFreqs.front()), yForDb (staticCurve.front()));
        for (size_t i = 1; i < staticCurve.size(); ++i)
            shaded.lineTo (xForFreq (pointFreqs[i]), yForDb (staticCurve[i]));
        for (size_t i = rangeCurve.size(); i-- > 0;)
            shaded.lineTo (xForFreq (pointFreqs[i]), yForDb (rangeCurve[i]));
        shaded.closeSubPath();

        const auto colour = Theme::band (selected);
        g.setColour (colour.withAlpha (0.13f));
        g.fillPath (shaded);

        juce::Path edge = curvePath (rangeCurve, false), dashed;
        const float dashes[] { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (dashed, edge, dashes, 2);
        g.setColour (colour.withAlpha (0.6f));
        g.fillPath (dashed);
    }

    for (int b : { hovered != selected ? hovered : -1, selected })
    {
        if (b < 0 || bandCurves[(size_t) b].empty())
            continue;

        const auto colour = Theme::band (b);
        const bool active = cache[(size_t) b].active;
        g.setColour (colour.withAlpha (active ? 0.16f : 0.07f));
        g.fillPath (curvePath (bandCurves[(size_t) b], true));
        g.setColour (colour.withAlpha (active ? 0.7f : 0.35f));
        g.strokePath (curvePath (bandCurves[(size_t) b], false), juce::PathStrokeType (1.2f));
    }

    const juce::PathStrokeType stroke { 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded };

    if (splitCurves)
    {
        g.setColour (Theme::curveAlt.withAlpha (0.04f));
        g.fillPath (curvePath (totalCurves[1], true));
        g.setColour (Theme::curveAlt);
        g.strokePath (curvePath (totalCurves[1], false), stroke);
    }

    g.setColour (Theme::curve.withAlpha (0.05f));
    g.fillPath (curvePath (totalCurves[0], true));
    g.setColour (Theme::curve);
    g.strokePath (curvePath (totalCurves[0], false), stroke);
}

void EqDisplay::drawLegend (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (! splitCurves)
        return;

    auto row = juce::Rectangle<float> (area.getX() + 10.0f, area.getY() + 8.0f, 150.0f, 16.0f);
    g.setFont (juce::FontOptions (12.0f));

    for (int group = 0; group < 2; ++group)
    {
        g.setColour (group == 0 ? Theme::curve : Theme::curveAlt);
        g.fillRoundedRectangle (row.getX(), row.getCentreY() - 1.5f, 16.0f, 3.0f, 1.5f);
        g.drawText (groupNames[group], row.withTrimmedLeft (22.0f), juce::Justification::centredLeft, false);
        row.translate (0.0f, 17.0f);
    }
}

void EqDisplay::drawNodes (juce::Graphics& g)
{
    const auto drawNode = [&] (int b)
    {
        const auto pos = nodePosition (b);
        const auto colour = Theme::band (b);
        const bool isSelected = b == selected;
        const float radius = (isSelected ? 7.5f : 6.0f) + (b == hovered ? 1.0f : 0.0f);
        const auto circle = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (pos);
        const auto& c = cache[(size_t) b];

        if (c.isDynamic())
        {
            // A line from the dot to where the full range would take it.
            const auto area = plotArea();
            const float endY = juce::jlimit (area.getY() + 4.0f, area.getBottom() - 4.0f,
                                             yForDb ((c.params.gainDb + c.dynamics.rangeDb) * gainScale));
            const float liveY = juce::jlimit (area.getY() + 4.0f, area.getBottom() - 4.0f,
                                              yForDb (c.params.gainDb * gainScale + c.dynamicDb));
            g.setColour (colour.withAlpha (0.55f));
            g.drawLine (pos.x, pos.y, pos.x, endY, 1.5f);

            if (! (isSelected && hasRangeHandle (b)))
                g.drawLine (pos.x - 5.0f, endY, pos.x + 5.0f, endY, 1.5f);

            // Where the gain is right now.
            if (std::abs (liveY - pos.y) > 1.0f)
            {
                g.setColour (colour);
                g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre ({ pos.x, liveY }));
            }
        }

        if (cache[(size_t) b].active)
        {
            g.setColour (colour);
            g.fillEllipse (circle);
        }
        else
        {
            g.setColour (Theme::display);
            g.fillEllipse (circle);
            g.setColour (colour.withAlpha (0.6f));
            g.drawEllipse (circle.reduced (1.0f), 1.5f);
        }

        if (isSelected)
        {
            g.setColour (Theme::curve);
            g.drawEllipse (circle.expanded (2.0f), 1.5f);
        }

        if (isSelected && hasRangeHandle (b))
        {
            // The dynamic range handle: a small diamond on the dot's stem.
            const auto handle = rangeHandlePosition (b);
            const bool hot = rangeHandleHovered || draggingRange == b;
            const float size = hot ? 6.5f : 5.0f;
            juce::Path diamond;
            diamond.addQuadrilateral (handle.x, handle.y - size, handle.x + size, handle.y,
                                      handle.x, handle.y + size, handle.x - size, handle.y);

            if (c.isDynamic())
            {
                g.setColour (colour);
                g.fillPath (diamond);
            }
            else
            {
                const float dashes[] { 2.0f, 2.0f };
                juce::Path stem, dashed;
                stem.startNewSubPath (pos.x, pos.y + (handle.y > pos.y ? radius : -radius));
                stem.lineTo (handle.x, handle.y + (handle.y > pos.y ? -size : size));
                juce::PathStrokeType (1.0f).createDashedStroke (dashed, stem, dashes, 2);
                g.setColour (colour.withAlpha (hot ? 0.8f : 0.45f));
                g.fillPath (dashed);
                g.setColour (Theme::display);
                g.fillPath (diamond);
                g.setColour (colour.withAlpha (hot ? 1.0f : 0.6f));
                g.strokePath (diamond, juce::PathStrokeType (1.3f));
            }
        }

        if (const auto* letter = placementLetter (cache[(size_t) b].placement); *letter != 0)
        {
            g.setColour (cache[(size_t) b].active ? Theme::display : colour);
            g.setFont (juce::FontOptions (radius * 1.45f, juce::Font::bold));
            g.drawText (letter, circle.translated (0.0f, 0.5f), juce::Justification::centred, false);
        }
    };

    for (int b = 0; b < Params::numBands; ++b)
        if (cache[(size_t) b].used && b != selected)
            drawNode (b);

    if (selected >= 0 && cache[(size_t) selected].used)
        drawNode (selected);
}

void EqDisplay::drawPreview (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (! showPreview || previewCurve.size() != pointFreqs.size() || dragging >= 0)
        return;

    juce::Path line;
    for (size_t i = 0; i < previewCurve.size(); ++i)
    {
        const juce::Point<float> p { xForFreq (pointFreqs[i]), juce::jlimit (area.getY(), area.getBottom(), yForDb (previewCurve[i])) };
        if (i == 0)
            line.startNewSubPath (p);
        else
            line.lineTo (p);
    }

    juce::Path dashed;
    const float dashes[] { 3.0f, 3.0f };
    juce::PathStrokeType (1.0f).createDashedStroke (dashed, line, dashes, 2);
    g.setColour (Theme::curve.withAlpha (0.35f));
    g.fillPath (dashed);

    // Which shape a click here adds, next to the mouse.
    const juce::String text = Params::shapeNames()[(int) previewParams.shape] + "  " + Params::formatFrequency (previewParams.freqHz);
    const juce::Font font { juce::FontOptions (11.5f) };
    const float width = juce::GlyphArrangement::getStringWidth (font, text) + 12.0f;
    auto box = juce::Rectangle<float> (width, 18.0f).withPosition (previewPoint.x + 12.0f, previewPoint.y + 10.0f);

    if (box.getRight() > area.getRight() - 2.0f)
        box.setX (previewPoint.x - 12.0f - width);
    box = box.constrainedWithin (area.reduced (2.0f));

    g.setColour (Theme::panel.withAlpha (0.85f));
    g.fillRoundedRectangle (box, 3.0f);
    g.setColour (Theme::dimText);
    g.setFont (font);
    g.drawText (text, box, juce::Justification::centred, false);
}

void EqDisplay::drawBandInfo (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int b = draggingRange >= 0 ? draggingRange : dragging >= 0 ? dragging : hovered;
    if (b < 0 || ! cache[(size_t) b].used)
        return;

    const auto& p = cache[(size_t) b].params;
    auto text = Params::formatFrequency (p.freqHz);
    if (shapeUsesGain (p.shape))
        text << "   " << formatGain (p.gainDb);
    if (shapeUsesQ (p.shape))
        text << "   Q " << juce::String (p.q, 2);
    if (cache[(size_t) b].placement != Placement::stereo)
        text << "   " << Params::placementNames()[(int) cache[(size_t) b].placement];
    if (cache[(size_t) b].isDynamic())
        text << "   Dyn " << formatGain (cache[(size_t) b].dynamics.rangeDb)
             << (cache[(size_t) b].dynamics.autoThreshold ? " (auto)" : "");

    const juce::Font font { juce::FontOptions (12.0f) };
    const float width = juce::GlyphArrangement::getStringWidth (font, text) + 16.0f;
    const auto pos = nodePosition (b);
    auto box = juce::Rectangle<float> (width, 22.0f).withCentre ({ pos.x, pos.y - 24.0f });

    if (box.getY() < area.getY() + 2.0f)
        box.setCentre (pos.x, pos.y + 24.0f);

    box = box.constrainedWithin (area.reduced (2.0f));

    g.setColour (Theme::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (Theme::band (b).withAlpha (0.8f));
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    g.setColour (Theme::text);
    g.setFont (font);
    g.drawText (text, box, juce::Justification::centred, false);
}
