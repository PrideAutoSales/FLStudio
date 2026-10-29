#pragma once

#include "../PluginProcessor.h"
#include "SpectrumAnalyzer.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The main EQ view: frequency grid, live analyzer, the EQ curve and one
// draggable dot per band.
//
// Mouse: click empty space to add a band (its shape depends on where: cuts at
// the far ends, shelves just inside them, bells in between; Alt-click adds a
// dynamic band), drag a dot to move it (vertical drag sets Q on cuts, notches
// and band passes), scroll over a dot to change Q (Cmd-scroll for gain,
// Alt-scroll for dynamic range), hold Shift for fine moves, Alt-click a dot to
// bypass it, double-click a dot to remove it, right-click a dot for more
// options. The selected bell, shelf or tilt has a small handle on its stem:
// drag it to make the band dynamic and set how far it moves.
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
    bool hasRangeHandle (int band) const noexcept;
    juce::Point<float> rangeHandlePosition (int band) const noexcept;
    bool rangeHandleAt (juce::Point<float>) const noexcept;
    double dbPerPixel() const noexcept;

    juce::RangedAudioParameter& param (int band, Params::BandField) const;
    float paramValue (int band, Params::BandField) const;
    void setParam (int band, Params::BandField, float value);
    void setParamGesture (int band, Params::BandField, float value);
    void resetParam (int band, Params::BandField);
    void beginDrag (int band, juce::Point<float> position, bool fine);
    void startBandDrag (int band, juce::Point<float> position, bool fine);
    void makeDynamic (int band);
    int createBandAt (juce::Point<float>, bool dynamic);
    void updatePreview (juce::Point<float>);
    void removeBand (int band);
    void showBandMenu (int band);
    void select (int band);

    void drawGrid (juce::Graphics&, juce::Rectangle<float> area);
    void drawAnalyzer (juce::Graphics&, juce::Rectangle<float> area);
    void drawCurves (juce::Graphics&, juce::Rectangle<float> area);
    void drawLegend (juce::Graphics&, juce::Rectangle<float> area);
    void drawNodes (juce::Graphics&);
    void drawPreview (juce::Graphics&, juce::Rectangle<float> area);
    void drawBandInfo (juce::Graphics&, juce::Rectangle<float> area);

    CruzEQAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    SpectrumAnalyzer preAnalyzer, postAnalyzer;
    std::vector<double> pointFreqs;
    std::vector<float> preLevels, postLevels;
    // Total curve for each channel group: left (or mid) and right (or side).
    // They differ only when some band is not on both channels.
    std::array<std::vector<float>, 2> totalCurves;
    bool splitCurves = false;
    juce::String groupNames[2];
    std::array<std::vector<float>, Params::numBands> bandCurves;
    // For the selected band when it is dynamic: its curve with no gain move
    // and with the full range, drawn as a shaded area.
    std::vector<float> staticCurve, rangeCurve;

    struct CachedBand
    {
        bool used = false, active = false;
        cruz::dsp::BandParams params; // as set on the band, before gain scale
        cruz::dsp::Placement placement = cruz::dsp::Placement::stereo;
        cruz::dsp::DynamicParams dynamics;
        float dynamicDb = 0.0f; // the gain move happening right now

        bool isDynamic() const noexcept { return dynamics.enabled && cruz::dsp::shapeUsesDynamics (params.shape); }
    };

    std::array<CachedBand, Params::numBands> cache;
    double gainScale = 1.0;
    double vScale() const noexcept { return gainScale > 0.05 ? gainScale : 1.0; }
    double cachedSampleRate = 0.0;
    bool curvesDirty = true;
    int preIdleFrames = 0, postIdleFrames = 0;

    int selected = -1, hovered = -1, dragging = -1;

    // The band the last click added, so the second click of a double-click
    // does not add another band or remove the new one.
    int lastCreated = -1;

    // Dragging the selected band's dynamic range handle.
    int draggingRange = -1;
    bool rangeHandleHovered = false, rangeDragTurnedOn = false;
    double rangeAnchorDb = 0.0;
    juce::Point<float> rangeAnchorMouse;

    // What a click on empty space would add, drawn faintly under the mouse.
    bool showPreview = false;
    juce::Point<float> previewPoint;
    cruz::dsp::BandParams previewParams;
    std::vector<float> previewCurve;

    struct DragAnchor
    {
        juce::Point<float> mouse;
        double logFreq = 0.0, gainDb = 0.0, logQ = 0.0;
        bool fine = false;
    } anchor;

    float rangeDb = 12.0f;

public:
    // The shape a click adds at this frequency.
    static cruz::dsp::Shape shapeForClick (double hz) noexcept;
    static constexpr double lowCutBelowHz = 30.0, lowShelfBelowHz = 150.0;
    static constexpr double highShelfAboveHz = 5000.0, highCutAboveHz = 14000.0;

private:
    static constexpr double minDisplayHz = 10.0;
    static constexpr double maxDisplayHz = 30000.0;
    static constexpr float analyzerFloorDb = -90.0f;
    static constexpr float analyzerTiltDbPerOct = 4.5f;
    static constexpr int pointSpacing = 2;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqDisplay)
};
