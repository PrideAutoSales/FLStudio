#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

namespace cs::ui
{
// Stacked 3D (or flat) view of an oscillator's wavetable, with the current
// position highlighted. Drop a WAV file on it to load it as a wavetable.
class WavetableView : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public juce::SettableTooltipClient,
                      private juce::Timer
{
public:
    WavetableView (CruzSynthProcessor& p, int osc);

    void paint (juce::Graphics&) override;
    void resized() override { cache = {}; }
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { fileHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { fileHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void timerCallback() override;
    void rebuildCache();
    float warpedSample (const Wavetable& t, float framePos, float phase) const;

    CruzSynthProcessor& proc;
    int osc;
    bool flat = false, fileHover = false;
    juce::Image cache;
    const Wavetable* cachedFor = nullptr;
    float lastPos = -1.0f, lastWarp = -1.0f;
    int lastWarpMode = -1;
};

//==============================================================================
class FilterView : public juce::Component, private juce::Timer
{
public:
    explicit FilterView (CruzSynthProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float magnitudeDb (int type, float freq, float cutoff, float res) const;
    CruzSynthProcessor& proc;
    float lastHash = 0.0f;
};

//==============================================================================
class EnvelopeView : public juce::Component, private juce::Timer
{
public:
    EnvelopeView (CruzSynthProcessor& p, int env);
    void paint (juce::Graphics&) override;
    void setEnvelope (int e) { env = e; repaint(); }

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    CruzSynthProcessor& proc;
    int env;
};

//==============================================================================
// Shows an LFO and lets you draw your own shape: click to add a point, drag to
// move it, double-click a point to delete it, Option-drag a line to bend it.
class LfoView : public juce::Component, private juce::Timer
{
public:
    LfoView (CruzSynthProcessor& p, int lfo);
    void paint (juce::Graphics&) override;
    void setLfo (int l) { lfo = l; repaint(); }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Point { float x, y, c; };
    void timerCallback() override { if (isShowing()) repaint(); }
    float shapeValue (float phase) const;
    std::vector<Point> readPoints() const;
    void writePoints (const std::vector<Point>&);
    void ensureCustom();
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (6.0f, 8.0f); }
    juce::Point<float> toScreen (float x, float y) const;
    int hitPoint (juce::Point<float> p, const std::vector<Point>& pts) const;

    CruzSynthProcessor& proc;
    int lfo;
    int dragIndex = -1, curveIndex = -1;
    float curveStart = 0.0f;
};

//==============================================================================
// Live curves for the effects rack: the distortion's shaper, the EQ and the FX filter.
class FxCurveView : public juce::Component, private juce::Timer
{
public:
    enum Kind { Distortion, Eq, Filter };
    FxCurveView (CruzSynthProcessor& p, Kind k);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float value (const char* paramID) const;   // includes modulation
    void paintDistortion (juce::Graphics&, juce::Rectangle<float>);
    void paintResponse (juce::Graphics&, juce::Rectangle<float>);
    CruzSynthProcessor& proc;
    Kind kind;
    float lastHash = 0.0f;
};

//==============================================================================
class ScopeView : public juce::Component, private juce::Timer
{
public:
    explicit ScopeView (CruzSynthProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    CruzSynthProcessor& proc;
    std::vector<float> buf;
};
} // namespace cs::ui
