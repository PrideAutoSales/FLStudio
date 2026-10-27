#pragma once

#include "../PluginProcessor.h"
#include "SpectrumAnalyzer.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The main EQ view: frequency grid, live analyzer, the EQ curve and one
// draggable dot per band.
//
// Mouse: double-click empty space to add a band, drag a dot to move it
// (vertical drag sets Q on cuts, notches and band passes), scroll over a dot
// to change Q, hold Shift for fine moves, Alt-click a dot to bypass it,
// double-click a dot to remove it, right-click a dot for more options.
class EqDisplay final : public juce::Component,
                        private juce::Timer
{
public:
    explicit EqDisplay (CruzEQAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    void setSelectedBand (int band);
    int getSelectedBand() const noexcept { return selected; }
    std::function<void (int)> onSelectionChanged;

    void setRangeDb (float newRange);

private:
    void timerCallback() override;

    juce::Rectangle<float> plotArea() const noexcept;
    float xForFreq (double hz) const noexcept;
    double freqForX (float x) const noexcept;
    float yForDb (double db) const noexcept;
    double dbForY (float y) const noexcept;
    float yForAnalyzerDb (float db) const noexcept;

    bool refreshBandCache();
    void recomputeCurves();
    void updateAnalyzers();
    juce::Point<float> nodePosition (int band) const noexcept;
    int nodeAt (juce::Point<float>) const noexcept;

    juce::RangedAudioParameter& param (int band, Params::BandField) const;
    float paramValue (int band, Params::BandField) const;
    void setParam (int band, Params::BandField, float value);
    void setParamGesture (int band, Params::BandField, float value);
    void beginDrag (int band, juce::Point<float> position, bool fine);
    void createBandAt (juce::Point<float>);
    void removeBand (int band);
    void showBandMenu (int band);
    void select (int band);

    void drawGrid (juce::Graphics&, juce::Rectangle<float> area);
    void drawAnalyzer (juce::Graphics&, juce::Rectangle<float> area);
    void drawCurves (juce::Graphics&, juce::Rectangle<float> area);
    void drawNodes (juce::Graphics&);
    void drawBandInfo (juce::Graphics&, juce::Rectangle<float> area);

    CruzEQAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    SpectrumAnalyzer preAnalyzer, postAnalyzer;
    std::vector<double> pointFreqs;
    std::vector<float> preLevels, postLevels, totalCurve;
    std::array<std::vector<float>, Params::numBands> bandCurves;

    struct CachedBand
    {
        bool used = false, active = false;
        cruz::dsp::BandParams params;
    };

    std::array<CachedBand, Params::numBands> cache;
    double cachedSampleRate = 0.0;
    bool curvesDirty = true;
    int preIdleFrames = 0, postIdleFrames = 0;

    int selected = -1, hovered = -1, dragging = -1;

    struct DragAnchor
    {
        juce::Point<float> mouse;
        double logFreq = 0.0, gainDb = 0.0, logQ = 0.0;
        bool fine = false;
    } anchor;

    float rangeDb = 12.0f;

    static constexpr double minDisplayHz = 10.0;
    static constexpr double maxDisplayHz = 30000.0;
    static constexpr float analyzerFloorDb = -90.0f;
    static constexpr float analyzerTiltDbPerOct = 4.5f;
    static constexpr int pointSpacing = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqDisplay)
};
