#include "Displays.h"
#include "../Piano.h"
#include <complex>

namespace cs::ui
{
namespace
{
    float param (CruzSynthProcessor& p, const juce::String& pid)
    {
        return p.apvts.getRawParameterValue (pid)->load();
    }

    void drawDisplayBackground (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (colours::display);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (colours::outline.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }
}

//==============================================================================
WavetableView::WavetableView (CruzSynthProcessor& p, int o) : proc (p), osc (o)
{
    setTooltip ("Click to switch 3D / flat view. Drop a WAV file here to load it as a wavetable.");
    startTimerHz (30);
}

void WavetableView::timerCallback()
{
    if (! isShowing())
        return;
    const float pos = proc.live.wtPos[(size_t) osc].load();
    const float warp = param (proc, id::osc (osc, "warpAmt"));
    const int mode = juce::roundToInt (param (proc, id::osc (osc, "warp")));
    if (proc.getTable (osc).get() != cachedFor || pos != lastPos || warp != lastWarp || mode != lastWarpMode)
        repaint();
}

float WavetableView::warpedSample (const Wavetable& t, float fp, float ph) const
{
    const int mode = juce::roundToInt (param (proc, id::osc (osc, "warp")));
    const float a = param (proc, id::osc (osc, "warpAmt"));
    double p = ph;
    switch (mode)
    {
        case WarpSync: p = std::fmod (ph * (1.0 + a * 15.0), 1.0); break;
        case WarpBendPlus:
        case WarpBendMinus:
        {
            const double e = mode == WarpBendPlus ? 1.0 + a * 5.0 : 1.0 / (1.0 + a * 5.0);
            p = ph < 0.5 ? 0.5 * std::pow (2.0 * ph, e) : 1.0 - 0.5 * std::pow (2.0 - 2.0 * ph, e);
            break;
        }
        case WarpAsym: { const double b = 0.5 - 0.45 * a; p = ph < b ? 0.5 * ph / b : 0.5 + 0.5 * (ph - b) / (1.0 - b); break; }
        case WarpPwm: { const double w = 1.0 - 0.95 * a; if (ph >= w) return 0.0f; p = ph / w; break; }
        case WarpMirror:
        {
            const float s0 = t.read (fp, 0, ph);
            const float s1 = t.read (fp, 0, juce::jlimit (0.0, 0.999999, ph < 0.5 ? 2.0 * ph : 2.0 - 2.0 * ph));
            return s0 + a * (s1 - s0);
        }
        case WarpFlip: { const float s = t.read (fp, 0, ph); return ph > 1.0 - 0.5 * a ? -s : s; }
        case WarpQuantize:
        {
            const float s = t.read (fp, 0, ph);
            if (a <= 0.0f) return s;
            const float steps = std::exp2 (6.0f - 5.0f * a);
            return std::round (s * steps) / steps;
        }
        default: break;
    }
    return t.read (fp, 0, juce::jlimit (0.0, 0.999999, p));
}

void WavetableView::rebuildCache()
{
    auto table = proc.getTable (osc);
    cachedFor = table.get();
    cache = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth()), juce::jmax (1, getHeight()), true);
    if (table == nullptr || flat)
        return;

    juce::Graphics g (cache);
    const auto r = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    const int frames = table->getNumFrames();
    const int shown = juce::jmin (frames, 48);
    const float dx = r.getWidth() * 0.22f, dy = r.getHeight() * 0.42f;
    const float w = r.getWidth() - dx, amp = r.getHeight() * 0.2f;

    for (int k = shown - 1; k >= 0; --k)
    {
        const float fi = shown > 1 ? (float) k / (float) (shown - 1) : 0.0f;
        const int f = juce::roundToInt (fi * (float) (frames - 1));
        const float* data = table->getRawFrame (f);
        juce::Path path;
        for (int i = 0; i <= 96; ++i)
        {
            const float t = (float) i / 96.0f;
            const float x = r.getX() + t * w + fi * dx;
            const float y = r.getBottom() - amp - fi * dy - data[(int) (t * (Wavetable::kSize - 1))] * amp;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (colours::accent2.withAlpha (0.10f + 0.12f * (1.0f - fi)));
        g.strokePath (path, juce::PathStrokeType (1.0f));
    }
}

void WavetableView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    auto table = proc.getTable (osc);
    if (table == nullptr)
        return;

    if (cachedFor != table.get() || cache.getWidth() != getWidth() || cache.getHeight() != getHeight())
        rebuildCache();
    g.drawImageAt (cache, 0, 0);

    lastPos = proc.live.wtPos[(size_t) osc].load();
    lastWarp = param (proc, id::osc (osc, "warpAmt"));
    lastWarpMode = juce::roundToInt (param (proc, id::osc (osc, "warp")));
    const int frames = table->getNumFrames();
    const float fp = lastPos * (float) (frames - 1);
    const auto r = bounds.reduced (10.0f, 12.0f);

    juce::Path path;
    if (flat)
    {
        const float mid = r.getCentreY(), amp = r.getHeight() * 0.45f;
        for (int i = 0; i <= 256; ++i)
        {
            const float t = (float) i / 256.0f;
            const float y = mid - warpedSample (*table, fp, juce::jmin (t, 0.99999f)) * amp;
            if (i == 0) path.startNewSubPath (r.getX() + t * r.getWidth(), y); else path.lineTo (r.getX() + t * r.getWidth(), y);
        }
        g.setColour (colours::outline);
        g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    }
    else
    {
        const float fi = frames > 1 ? fp / (float) (frames - 1) : 0.0f;
        const float dx = r.getWidth() * 0.22f, dy = r.getHeight() * 0.42f;
        const float w = r.getWidth() - dx, amp = r.getHeight() * 0.2f;
        for (int i = 0; i <= 192; ++i)
        {
            const float t = (float) i / 192.0f;
            const float x = r.getX() + t * w + fi * dx;
            const float y = r.getBottom() - amp - fi * dy - warpedSample (*table, fp, juce::jmin (t, 0.99999f)) * amp;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
    }
    g.setColour (colours::accent.withAlpha (0.25f));
    g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent);
    g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (font (11.0f));
    g.setColour (colours::textDim);
    g.drawText ("Frame " + juce::String (juce::roundToInt (fp) + 1) + " / " + juce::String (frames),
                bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);

    if (fileHover)
    {
        g.setColour (colours::accent2.withAlpha (0.2f));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (colours::text);
        g.setFont (font (14.0f, true));
        g.drawText ("Drop to load wavetable", bounds, juce::Justification::centred);
    }
}

void WavetableView::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
    {
        flat = ! flat;
        cache = {};
        repaint();
    }
}

bool WavetableView::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (f.endsWithIgnoreCase (".wav") || f.endsWithIgnoreCase (".aif") || f.endsWithIgnoreCase (".aiff"))
            return true;
    return false;
}

void WavetableView::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;
    const juce::File file (files[0]);
    juce::String error;
    auto frames = tables::loadFromFile (file, error);
    if (frames.empty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't load wavetable", error);
        return;
    }
    proc.setTable (osc, std::make_shared<const Wavetable> (file.getFileNameWithoutExtension(), std::move (frames)));
    repaint();
}

//==============================================================================
FilterView::FilterView (CruzSynthProcessor& p) : proc (p) { startTimerHz (30); }

void FilterView::timerCallback()
{
    if (! isShowing())
        return;
    const float h = proc.live.dstNorm[DstFltCutoff].load() * 3.1f + proc.live.dstNorm[DstFltRes].load() * 7.7f
                  + param (proc, "flt_type") * 13.0f + param (proc, "flt_on") * 17.0f;
    if (h != lastHash)
    {
        lastHash = h;
        repaint();
    }
}

float FilterView::magnitudeDb (int type, float freq, float cutoff, float res) const
{
    using C = std::complex<float>;
    const C s (0.0f, freq / cutoff);
    const float k = 1.414f - 1.384f * res;
    const float k2 = juce::jmax (0.4f, 1.414f - res);
    auto den = [&] (float kk) { return s * s + kk * s + 1.0f; };
    C h (1.0f);
    switch (type)
    {
        case FltLp12: h = 1.0f / den (k); break;
        case FltLp24: h = 1.0f / (den (k) * den (k2)); break;
        case FltLadder: { const C g = 1.0f / (1.0f + s); const float kk = res * 3.9f; h = (g * g * g * g) * (1.0f + kk * 0.3f) / (1.0f + kk * g * g * g * g); break; }
        case FltHp12: h = s * s / den (k); break;
        case FltHp24: h = (s * s / den (k)) * (s * s / den (k2)); break;
        case FltBp12: h = k * s / den (k); break;
        case FltBp24: h = (k * s / den (k)) * (k2 * s / den (k2)); break;
        case FltNotch: h = (s * s + 1.0f) / den (k); break;
        case FltPeak: h = 1.0f + 3.0f * k * s / den (k); break;
        case FltCombPlus:
        case FltCombMinus:
        {
            const float fb = (0.2f + res * 0.75f) * (type == FltCombPlus ? 1.0f : -1.0f);
            const float w = juce::MathConstants<float>::twoPi * freq / cutoff;
            h = 0.5f / (1.0f - fb * std::exp (C (0.0f, -w)));
            break;
        }
        case FltVowel:
        {
            const float pos = juce::jlimit (0.0f, 3.999f, std::log2 (cutoff / 20.0f) / std::log2 (1000.0f) * 4.0f);
            static const float formants[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 },
                                                  { 450, 800, 2830 }, { 325, 700, 2530 } };
            static const float gains[3] = { 1.0f, 0.7f, 0.45f };
            const int v0 = (int) pos;
            const float t = pos - (float) v0;
            const float vk = 0.25f - 0.2f * res;
            h = 0.0f;
            for (int f = 0; f < 3; ++f)
            {
                const float fc = formants[v0][f] * (1.0f - t) + formants[v0 + 1][f] * t;
                const C sv (0.0f, freq / fc);
                h += vk * sv / (sv * sv + vk * sv + 1.0f) * gains[f] * 2.5f;
            }
            break;
        }
        default: break;
    }
    return juce::Decibels::gainToDecibels (std::abs (h), -60.0f);
}

void FilterView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    const auto r = bounds.reduced (6.0f, 8.0f);

    g.setColour (colours::outline.withAlpha (0.4f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = r.getX() + std::log10 (f / 20.0f) / 3.0f * r.getWidth();
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    }

    const bool on = param (proc, "flt_on") > 0.5f;
    const int type = juce::roundToInt (param (proc, "flt_type"));
    auto* cutParam = proc.apvts.getParameter ("flt_cutoff");
    const float cutoff = cutParam->convertFrom0to1 (proc.live.dstNorm[DstFltCutoff].load());
    const float res = proc.live.dstNorm[DstFltRes].load();
    const float mix = param (proc, "flt_mix");

    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (-36.0f, 24.0f, db), -36.0f, 24.0f, r.getBottom(), r.getY()); };
    g.setColour (colours::outline.withAlpha (0.4f));
    g.drawHorizontalLine ((int) yFor (0.0f), r.getX(), r.getRight());

    juce::Path p;
    for (int i = 0; i <= 180; ++i)
    {
        const float t = (float) i / 180.0f;
        const float f = 20.0f * std::pow (1000.0f, t);
        float db = magnitudeDb (type, f, cutoff, res);
        db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, 1.0f - mix + mix * juce::Decibels::decibelsToGain (db)));
        const float x = r.getX() + t * r.getWidth();
        if (i == 0) p.startNewSubPath (x, yFor (db)); else p.lineTo (x, yFor (db));
    }

    juce::Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    const auto col = on ? colours::accent : colours::textDim;
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.8f));

    g.setFont (font (11.0f));
    g.setColour (colours::textDim);
    g.drawText (on ? cutParam->getText (cutParam->convertTo0to1 (cutoff), 16) : "Off", bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
}

//==============================================================================
EnvelopeView::EnvelopeView (CruzSynthProcessor& p, int e) : proc (p), env (e) { startTimerHz (30); }

void EnvelopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    const auto r = bounds.reduced (8.0f, 10.0f);

    const float a = param (proc, id::env (env, "attack")), h = param (proc, id::env (env, "hold"));
    const float d = param (proc, id::env (env, "decay")), s = param (proc, id::env (env, "sustain"));
    const float rel = param (proc, id::env (env, "release")), curve = param (proc, id::env (env, "curve"));
    const float k = (curve - 0.5f) * 14.0f + 3.5f;

    // Times are drawn on a soft scale so short and long settings both read well.
    auto span = [] (float secs) { return std::sqrt (juce::jmax (0.0f, secs)) + 0.02f; };
    const float sa = span (a), sh = h > 0.0f ? span (h) : 0.0f, sd = span (d), ss = 0.6f, sr = span (rel);
    const float total = sa + sh + sd + ss + sr;
    auto X = [&] (float units) { return r.getX() + units / total * r.getWidth(); };
    auto Y = [&] (float v) { return r.getBottom() - v * r.getHeight(); };

    juce::Path p;
    p.startNewSubPath (X (0), Y (0));
    const int n = 40;
    for (int i = 1; i <= n; ++i) { const float t = (float) i / n; p.lineTo (X (t * sa), Y (dsp::bend (t, k * 0.4f))); }
    if (sh > 0.0f) p.lineTo (X (sa + sh), Y (1));
    for (int i = 1; i <= n; ++i) { const float t = (float) i / n; p.lineTo (X (sa + sh + t * sd), Y (1.0f - (1.0f - s) * dsp::bend (t, k))); }
    p.lineTo (X (sa + sh + sd + ss), Y (s));
    for (int i = 1; i <= n; ++i) { const float t = (float) i / n; p.lineTo (X (sa + sh + sd + ss + t * sr), Y (s * (1.0f - dsp::bend (t, k)))); }

    juce::Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.closeSubPath();
    const auto col = modSourceColour ((int) ModSrc::Env1 + env);
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.8f));

    // Live position: find the point on the current stage with the live level.
    const int stage = proc.live.envStage[(size_t) env].load();
    const float v = proc.live.envValue[(size_t) env].load();
    if (stage != dsp::Envelope::Idle && proc.live.anyVoice.load())
    {
        float x = 0.0f;
        switch (stage)
        {
            case dsp::Envelope::Attack:  x = X (sa * juce::jlimit (0.0f, 1.0f, v)); break;
            case dsp::Envelope::Hold:    x = X (sa + sh * 0.5f); break;
            case dsp::Envelope::Decay:   x = X (sa + sh + sd * (s < 1.0f ? juce::jlimit (0.0f, 1.0f, (1.0f - v) / (1.0f - s)) : 0.0f)); break;
            case dsp::Envelope::Sustain: x = X (sa + sh + sd + ss * 0.5f); break;
            default:                     x = X (sa + sh + sd + ss + sr * (s > 0.0f ? juce::jlimit (0.0f, 1.0f, 1.0f - v / s) : 1.0f)); break;
        }
        g.setColour (colours::text);
        g.fillEllipse (x - 4.0f, Y (v) - 4.0f, 8.0f, 8.0f);
    }

    g.setFont (font (11.0f));
    g.setColour (colours::textDim);
    g.drawText (env == 0 ? "ENV 1 (amp)" : "ENV " + juce::String (env + 1), bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
}

//==============================================================================
LfoView::LfoView (CruzSynthProcessor& p, int l) : proc (p), lfo (l)
{
    startTimerHz (30);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Point<float> LfoView::toScreen (float x, float y) const
{
    const auto r = plotArea();
    return { r.getX() + x * r.getWidth(), r.getBottom() - y * r.getHeight() };
}

std::vector<LfoView::Point> LfoView::readPoints() const
{
    std::vector<Point> pts;
    for (auto& tok : juce::StringArray::fromTokens (proc.getLfoPoints (lfo), ";", ""))
    {
        auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() >= 2)
            pts.push_back ({ v[0].getFloatValue(), v[1].getFloatValue(), v.size() > 2 ? v[2].getFloatValue() : 0.0f });
    }
    std::sort (pts.begin(), pts.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
    if (pts.size() < 2)
        pts = { { 0, 0, 0 }, { 1, 0, 0 } };
    return pts;
}

void LfoView::writePoints (const std::vector<Point>& pts)
{
    juce::StringArray parts;
    for (auto& p : pts)
        parts.add (juce::String (p.x, 4) + "," + juce::String (p.y, 4) + "," + juce::String (p.c, 3));
    proc.setLfoPoints (lfo, parts.joinIntoString (";"));
}

float LfoView::shapeValue (float p) const
{
    const int shape = juce::roundToInt (param (proc, id::lfo (lfo, "shape")));
    switch (shape)
    {
        case LfoSine:     return 0.5f + 0.5f * std::sin (dsp::twoPi * p);
        case LfoTriangle: return 1.0f - 2.0f * std::abs (p - 0.5f);
        case LfoSawUp:    return p;
        case LfoSawDown:  return 1.0f - p;
        case LfoSquare:   return p < 0.5f ? 1.0f : 0.0f;
        case LfoCustom:
        {
            const auto& t = proc.lfoTables[(size_t) lfo];
            return t[(size_t) juce::jlimit (0, kLfoTableSize - 1, (int) (p * (kLfoTableSize - 1)))].load();
        }
        case LfoSampleHold:
        {
            const int step = (int) (p * 8.0f);
            return std::fmod (std::sin ((float) step * 12.9898f) * 43758.5453f, 1.0f) * 0.5f + 0.5f;
        }
        default:
            return 0.5f + 0.3f * std::sin (dsp::twoPi * p) + 0.15f * std::sin (dsp::twoPi * p * 3.0f + 1.0f);
    }
}

void LfoView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    const auto r = plotArea();

    g.setColour (colours::outline.withAlpha (0.35f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 8.0f), r.getY(), r.getBottom());
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200.0f;
        const auto pt = toScreen (t, shapeValue (juce::jmin (t, 0.9999f)));
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    const auto col = modSourceColour ((int) ModSrc::Lfo1);
    juce::Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setColour (col.withAlpha (0.10f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.8f));

    if (juce::roundToInt (param (proc, id::lfo (lfo, "shape"))) == LfoCustom)
        for (auto& pt : readPoints())
        {
            const auto s = toScreen (pt.x, pt.y);
            g.setColour (colours::text);
            g.drawEllipse (s.x - 4.0f, s.y - 4.0f, 8.0f, 8.0f, 1.5f);
        }

    const float ph = proc.live.lfoPhase[(size_t) lfo].load();
    const float x = r.getX() + ph * r.getWidth();
    g.setColour (colours::text.withAlpha (0.5f));
    g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
    const auto dot = toScreen (ph, shapeValue (ph));
    g.setColour (colours::text);
    g.fillEllipse (dot.x - 4.0f, dot.y - 4.0f, 8.0f, 8.0f);

    g.setFont (font (11.0f));
    g.setColour (colours::textDim);
    g.drawText ("LFO " + juce::String (lfo + 1) + "  -  click to draw", bounds.reduced (8.0f, 4.0f), juce::Justification::topRight);
}

void LfoView::ensureCustom()
{
    auto* shape = proc.apvts.getParameter (id::lfo (lfo, "shape"));
    if (juce::roundToInt (shape->convertFrom0to1 (shape->getValue())) == LfoCustom)
        return;

    // Start the custom curve from whatever shape was showing.
    std::vector<Point> pts;
    const int current = juce::roundToInt (shape->convertFrom0to1 (shape->getValue()));
    if (current == LfoSquare)
        pts = { { 0, 1, 0 }, { 0.499f, 1, 0 }, { 0.5f, 0, 0 }, { 1, 0, 0 } };
    else if (current == LfoSawUp)
        pts = { { 0, 0, 0 }, { 1, 1, 0 } };
    else if (current == LfoSawDown)
        pts = { { 0, 1, 0 }, { 1, 0, 0 } };
    else if (current == LfoSine)
        for (int i = 0; i <= 16; ++i)
            pts.push_back ({ (float) i / 16.0f, 0.5f + 0.5f * std::sin (dsp::twoPi * (float) i / 16.0f), 0.0f });
    else
        pts = { { 0, 0, 0 }, { 0.5f, 1, 0 }, { 1, 0, 0 } };
    writePoints (pts);
    shape->setValueNotifyingHost (shape->convertTo0to1 ((float) LfoCustom));
}

int LfoView::hitPoint (juce::Point<float> p, const std::vector<Point>& pts) const
{
    for (int i = 0; i < (int) pts.size(); ++i)
        if (toScreen (pts[(size_t) i].x, pts[(size_t) i].y).getDistanceFrom (p) < 8.0f)
            return i;
    return -1;
}

void LfoView::mouseDown (const juce::MouseEvent& e)
{
    ensureCustom();
    auto pts = readPoints();
    const auto r = plotArea();
    const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());

    if (e.mods.isAltDown())
    {
        for (int i = 0; i + 1 < (int) pts.size(); ++i)
            if (x >= pts[(size_t) i].x && x <= pts[(size_t) i + 1].x)
            {
                curveIndex = i;
                curveStart = pts[(size_t) i].c;
            }
        return;
    }

    dragIndex = hitPoint (e.position, pts);
    if (dragIndex < 0)
    {
        const float y = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
        pts.push_back ({ x, y, 0.0f });
        std::sort (pts.begin(), pts.end(), [] (const Point& a, const Point& b) { return a.x < b.x; });
        writePoints (pts);
        dragIndex = hitPoint (e.position, pts);
    }
}

void LfoView::mouseDrag (const juce::MouseEvent& e)
{
    auto pts = readPoints();
    const auto r = plotArea();

    if (curveIndex >= 0 && curveIndex < (int) pts.size())
    {
        pts[(size_t) curveIndex].c = juce::jlimit (-1.0f, 1.0f, curveStart - (float) e.getDistanceFromDragStartY() / 80.0f);
        writePoints (pts);
        return;
    }
    if (dragIndex < 0 || dragIndex >= (int) pts.size())
        return;

    float x = juce::jlimit (0.0f, 1.0f, (e.position.x - r.getX()) / r.getWidth());
    float y = juce::jlimit (0.0f, 1.0f, (r.getBottom() - e.position.y) / r.getHeight());
    if (e.mods.isShiftDown())
    {
        x = std::round (x * 16.0f) / 16.0f;
        y = std::round (y * 8.0f) / 8.0f;
    }
    // End points stay at the edges; others stay between their neighbours.
    if (dragIndex == 0) x = 0.0f;
    else if (dragIndex == (int) pts.size() - 1) x = 1.0f;
    else x = juce::jlimit (pts[(size_t) dragIndex - 1].x, pts[(size_t) dragIndex + 1].x, x);
    pts[(size_t) dragIndex].x = x;
    pts[(size_t) dragIndex].y = y;
    writePoints (pts);
}

void LfoView::mouseUp (const juce::MouseEvent&)
{
    dragIndex = curveIndex = -1;
}

void LfoView::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto pts = readPoints();
    const int i = hitPoint (e.position, pts);
    if (i > 0 && i < (int) pts.size() - 1)
    {
        pts.erase (pts.begin() + i);
        writePoints (pts);
    }
}

//==============================================================================
FxCurveView::FxCurveView (CruzSynthProcessor& p, Kind k) : proc (p), kind (k) { startTimerHz (20); }

float FxCurveView::value (const char* pid) const
{
    const int dst = modDestinationForParam (pid);
    if (dst > 0)
        return proc.apvts.getParameter (pid)->convertFrom0to1 (proc.live.dstNorm[(size_t) dst].load());
    return param (proc, pid);
}

void FxCurveView::timerCallback()
{
    static const char* ids[3][9] = {
        { "fx_dist_mode", "fx_dist_drive", "fx_dist_mix", "fx_dist_on", nullptr },
        { "fx_eq_lowtype", "fx_eq_lowfreq", "fx_eq_lowq", "fx_eq_lowgain", "fx_eq_hightype", "fx_eq_highfreq", "fx_eq_highq", "fx_eq_highgain", "fx_eq_on" },
        { "fx_fil_type", "fx_fil_cutoff", "fx_fil_res", "fx_fil_mix", "fx_fil_on", nullptr } };
    float h = 0.0f;
    for (auto* id : ids[kind])
        if (id != nullptr)
            h = h * 1.37f + value (id);
    if (h != lastHash)
    {
        lastHash = h;
        repaint();
    }
}

void FxCurveView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    if (kind == Distortion)
        paintDistortion (g, bounds.reduced (8.0f));
    else
        paintResponse (g, bounds.reduced (6.0f, 8.0f));
}

void FxCurveView::paintDistortion (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int mode = juce::roundToInt (value ("fx_dist_mode"));
    const float drive = value ("fx_dist_drive"), mix = value ("fx_dist_mix");
    const bool on = value ("fx_dist_on") > 0.5f;

    g.setColour (colours::outline.withAlpha (0.4f));
    g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
    g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());

    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / 200.0f;
        float y = FxRack::transfer (mode, drive, x);
        y = juce::jlimit (-1.2f, 1.2f, x + (y - x) * mix);
        const auto pt = juce::Point<float> (r.getX() + (x + 1.0f) * 0.5f * r.getWidth(),
                                            r.getCentreY() - y / 1.2f * r.getHeight() * 0.5f);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    g.setColour (on ? colours::accent : colours::textDim);
    g.strokePath (p, juce::PathStrokeType (1.8f));
}

void FxCurveView::paintResponse (juce::Graphics& g, juce::Rectangle<float> r)
{
    const double sr = 48000.0;
    const bool on = value (kind == Eq ? "fx_eq_on" : "fx_fil_on") > 0.5f;
    const float lo = kind == Eq ? -20.0f : -36.0f, hi = kind == Eq ? 20.0f : 24.0f;
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (lo, hi, db), lo, hi, r.getBottom(), r.getY()); };

    g.setColour (colours::outline.withAlpha (0.4f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
        g.drawVerticalLine ((int) (r.getX() + std::log10 (f / 20.0f) / 3.0f * r.getWidth()), r.getY(), r.getBottom());
    g.drawHorizontalLine ((int) yFor (0.0f), r.getX(), r.getRight());

    fx::Biquad low, high;
    if (kind == Eq)
    {
        static constexpr fx::Biquad::Kind lowKinds[] = { fx::Biquad::LowShelf, fx::Biquad::Peak, fx::Biquad::LowCut };
        static constexpr fx::Biquad::Kind highKinds[] = { fx::Biquad::HighShelf, fx::Biquad::Peak, fx::Biquad::HighCut };
        low.design (lowKinds[juce::jlimit (0, 2, juce::roundToInt (value ("fx_eq_lowtype")))], sr,
                    value ("fx_eq_lowfreq"), value ("fx_eq_lowq"), value ("fx_eq_lowgain"));
        high.design (highKinds[juce::jlimit (0, 2, juce::roundToInt (value ("fx_eq_hightype")))], sr,
                     value ("fx_eq_highfreq"), value ("fx_eq_highq"), value ("fx_eq_highgain"));
    }
    const int type = juce::roundToInt (value ("fx_fil_type"));
    const float cutoff = value ("fx_fil_cutoff"), res = value ("fx_fil_res"), mix = value ("fx_fil_mix");
    const double k = juce::jmax (0.1, 2.0 - 1.9 * res);

    // The FX filter is drawn from its analogue prototype, which the SVF follows closely.
    auto filterDb = [&] (double f)
    {
        using C = std::complex<double>;
        const C s (0.0, f / cutoff);
        auto stage = [&] (int t, double kk)
        {
            const C den = s * s + kk * s + 1.0;
            switch (t)
            {
                case 0: return C (1.0) / den;
                case 1: return s * s / den;
                case 2: return kk * s / den;
                case 3: return (s * s + 1.0) / den;
                default: return (1.0 - s * s) / den;
            }
        };
        static constexpr int base[] = { 0, 0, 1, 1, 2, 2, 3, 4 };
        C h = stage (base[juce::jlimit (0, 7, type)], k) * (1.0 - 0.5 * res);
        if (type == 1 || type == 3) h *= stage (base[type], 1.414);
        if (type == 5) h *= stage (2, k);
        h = 1.0 + (double) mix * (h - 1.0);
        return (float) (20.0 * std::log10 (std::abs (h) + 1.0e-9));
    };

    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const float t = (float) i / 200.0f;
        const double f = 20.0 * std::pow (1000.0, (double) t);
        const float db = kind == Eq ? low.magnitudeDb (f, sr) + high.magnitudeDb (f, sr) : filterDb (f);
        const float x = r.getX() + t * r.getWidth();
        if (i == 0) p.startNewSubPath (x, yFor (db)); else p.lineTo (x, yFor (db));
    }
    juce::Path fill (p);
    fill.lineTo (r.getRight(), yFor (0.0f));
    fill.lineTo (r.getX(), yFor (0.0f));
    fill.closeSubPath();
    const auto col = on ? colours::accent : colours::textDim;
    g.setColour (col.withAlpha (0.12f));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (1.8f));
}

//==============================================================================
ScopeView::ScopeView (CruzSynthProcessor& p) : proc (p), buf (2048) { startTimerHz (30); }

void ScopeView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    drawDisplayBackground (g, bounds);
    proc.copyScope (buf.data(), (int) buf.size());

    // Trigger on a rising zero crossing so the wave stands still.
    int start = 0;
    for (int i = 1; i < 1024; ++i)
        if (buf[(size_t) i - 1] <= 0.0f && buf[(size_t) i] > 0.0f) { start = i; break; }

    const auto r = bounds.reduced (4.0f, 4.0f).withTrimmedRight (8.0f);
    juce::Path p;
    const int n = 768;
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + (float) i / (float) (n - 1) * r.getWidth();
        const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, buf[(size_t) (start + i)]) * r.getHeight() * 0.48f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    g.setColour (colours::accent2);
    g.strokePath (p, juce::PathStrokeType (1.2f));

    // Peak meter
    const float peak = juce::jlimit (0.0f, 1.2f, proc.getOutputPeak());
    auto meter = bounds.reduced (3.0f).removeFromRight (5.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (meter, 2.0f);
    const float h = meter.getHeight() * juce::jmin (1.0f, peak);
    g.setColour (peak > 1.0f ? juce::Colours::red : colours::accent);
    g.fillRoundedRectangle (meter.removeFromBottom (h), 2.0f);
}
} // namespace cs::ui

//==============================================================================
namespace cs::ui
{
PianoView::PianoView (CruzSynthProcessor& p) : proc (p) { startTimerHz (10); }

void PianoView::timerCallback()
{
    static const char* ids[] = { "pno_on", "pno_model", "pno_tone", "pno_dynamics", "pno_sustain",
                                 "pno_strings", "pno_body", "pno_hammer", "pno_width" };
    float hash = 0.0f, k = 1.0f;
    for (auto* pid : ids)
    {
        hash += param (proc, pid) * k;
        k *= 1.618f;
    }
    if (hash == lastHash)
        return;
    lastHash = hash;

    piano::Settings s;
    s.model = juce::roundToInt (param (proc, "pno_model"));
    s.tone = param (proc, "pno_tone");
    s.dynamics = param (proc, "pno_dynamics");
    s.sustain = param (proc, "pno_sustain");
    s.strings = param (proc, "pno_strings");
    s.body = param (proc, "pno_body");
    s.hammer = param (proc, "pno_hammer");
    s.width = param (proc, "pno_width");

    auto take = [&] (float vel, Strike& out)
    {
        auto voice = std::make_unique<piano::Voice>();
        voice->prepare (48000.0);
        voice->strike (60.0f, vel, s);
        out.f.resize (300);
        out.a.resize (300);
        const int n = voice->snapshot (out.f.data(), out.a.data(), 300);
        out.f.resize ((size_t) n);
        out.a.resize ((size_t) n);
    };
    take (0.35f, soft);
    take (1.0f, hard);
    repaint();
}

void PianoView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawDisplayBackground (g, r);
    const bool on = param (proc, "pno_on") > 0.5f;
    const int model = juce::roundToInt (param (proc, "pno_model"));

    auto text = r.reduced (10, 6);
    g.setFont (font (13.0f, true));
    g.setColour (on ? colours::text : colours::textDim);
    g.drawText (piano::modelNames()[model], text.removeFromTop (18), juce::Justification::centredLeft);
    g.setFont (font (11.0f));
    g.setColour (colours::textDim);
    g.drawFittedText (piano::modelDescription (model), text.removeFromTop (16).toNearestInt(), juce::Justification::centredLeft, 1);

    auto plot = r.reduced (10, 0).withTrimmedTop (48).withTrimmedBottom (18);
    const float lo = -72.0f;
    auto xFor = [&] (float f) { return plot.getX() + std::log10 (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / 3.0f * plot.getWidth(); };

    g.setColour (colours::outline.withAlpha (0.4f));
    g.setFont (font (10.0f));
    for (float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const float x = xFor (f);
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x + 3, plot.getBottom() + 2, 40, 14), juce::Justification::centredLeft);
    }

    float ref = 1.0e-9f;
    for (float a : hard.a) ref = juce::jmax (ref, a);
    auto yFor = [&] (float a)
    {
        const float db = 20.0f * std::log10 (a / ref + 1.0e-9f);
        return juce::jmap (juce::jlimit (lo, 0.0f, db), lo, 0.0f, plot.getBottom(), plot.getY());
    };

    auto bars = [&] (const Strike& st, juce::Colour c, float w)
    {
        g.setColour (c.withAlpha (on ? 0.9f : 0.35f));
        for (size_t i = 0; i < st.f.size(); ++i)
        {
            const float x = xFor (st.f[i]), y = yFor (st.a[i]);
            if (y < plot.getBottom() - 1.0f)
                g.fillRect (x - w * 0.5f, y, w, plot.getBottom() - y);
        }
    };
    bars (hard, colours::accent, 2.0f);
    bars (soft, colours::accent2, 2.0f);

    g.setFont (font (10.0f));
    auto legend = r.reduced (10, 6).removeFromTop (18).removeFromRight (150);
    g.setColour (colours::accent2);
    g.drawText ("soft", legend.removeFromLeft (50), juce::Justification::centredRight);
    g.setColour (colours::accent);
    g.drawText ("hard (middle C)", legend, juce::Justification::centredRight);
}
} // namespace cs::ui
