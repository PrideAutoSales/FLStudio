#pragma once

#include "EditorRedrawGuard.h"
#include "PluginProcessor.h"

namespace cruzui
{
    class LookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        LookAndFeel();
        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                               float startAngle, float endAngle, juce::Slider&) override;
        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
        juce::Label* createSliderTextBox (juce::Slider&) override;
    };

    // A knob with its name above it and its value below.
    class Knob final : public juce::Component
    {
    public:
        Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& title);
        void resized() override;

        juce::Slider slider;

    private:
        juce::Label title;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    // A row of buttons that picks one value of a choice parameter.
    class ChoiceButtons final : public juce::Component
    {
    public:
        ChoiceButtons (juce::AudioProcessorValueTreeState&, const juce::String& paramID);
        void resized() override;

    private:
        juce::OwnedArray<juce::TextButton> buttons;
        std::unique_ptr<juce::ParameterAttachment> attachment;
    };

    // The last few seconds of display slices, oldest first.
    struct History
    {
        static constexpr int size = 600; // 3.2 seconds

        void push (const cruzsc::Frame& f)
        {
            frames[(size_t) next] = f;
            next = (next + 1) % size;
        }

        const cruzsc::Frame& operator[] (int i) const { return frames[(size_t) ((next + i) % size)]; }

        std::vector<cruzsc::Frame> frames = std::vector<cruzsc::Frame> ((size_t) size);
        int next = 0;
    };

    // Drags one parameter as a single undoable gesture.
    struct ParamDrag
    {
        juce::RangedAudioParameter* param = nullptr;
        float start = 0.0f;

        void begin (juce::RangedAudioParameter* p);
        void set (float value);
        void end();
        float value() const;
    };

    // Scrolling waveforms: the trigger on top, the track being ducked below,
    // with the gain reduction drawn over it.
    class ScopeView final : public juce::Component
    {
    public:
        ScopeView (CruzSidechainAudioProcessor&, const History&);
        void paint (juce::Graphics&) override;

    private:
        CruzSidechainAudioProcessor& processor;
        const History& history;
    };

    // Low-frequency heat map: where the trigger and the ducked track share
    // the same low band at the same moment.
    class HeatMapView final : public juce::Component
    {
    public:
        HeatMapView (const History&);
        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        const History& history;
        juce::TextButton beforeButton { "Before ducking" }, afterButton { "After ducking" };
        bool showAfter = true;
        juce::Image image { juce::Image::ARGB, History::size, cruzsc::numBands, true };
    };

    // The compression curve: drag the threshold, ratio, knee and range handles.
    class CurveEditor final : public juce::Component
    {
    public:
        CurveEditor (CruzSidechainAudioProcessor&);
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        enum class Handle { none, threshold, ratio, knee, range };

        juce::Rectangle<float> plot() const;
        float xFor (float db) const;
        float yFor (float db) const;
        float dbPerPixel() const;
        juce::Point<float> handlePos (Handle) const;
        Handle hit (juce::Point<float>) const;
        juce::RangedAudioParameter* paramFor (Handle) const;

        CruzSidechainAudioProcessor& processor;
        Handle hover = Handle::none, dragging = Handle::none;
        ParamDrag drag;
        juce::Point<float> dragStart;
        float startOutAtZero = 0.0f;
    };

    // The shape of one duck over time: drag the attack, hold and release
    // points and bend the attack and release curves.
    class ShapeEditor final : public juce::Component
    {
    public:
        ShapeEditor (CruzSidechainAudioProcessor&);
        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        enum class Handle { none, attack, hold, release, attackBend, releaseBend };

        juce::Rectangle<float> plot() const;
        float spanMs() const;
        float xFor (float ms) const;
        float yFor (float depth) const;
        juce::Point<float> handlePos (Handle) const;
        Handle hit (juce::Point<float>) const;
        juce::RangedAudioParameter* paramFor (Handle) const;

        CruzSidechainAudioProcessor& processor;
        Handle hover = Handle::none, dragging = Handle::none;
        ParamDrag drag;
        juce::Point<float> dragStart;
        float frozenSpan = 0.0f;
    };
}

class CruzSidechainAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit CruzSidechainAudioProcessorEditor (CruzSidechainAudioProcessor&);
    ~CruzSidechainAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void timerCallback() override;
    juce::String statusText() const;

    CruzSidechainAudioProcessor& processor;
    cruzui::LookAndFeel lookAndFeel;

    cruzui::History history;
    std::vector<cruzsc::Frame> incoming = std::vector<cruzsc::Frame> (1024);

    cruzui::ChoiceButtons triggerButtons, rateButtons, detectButtons;
    juce::TextButton splitButton { "Duck lows only" }, listenButton { "Hear trigger" };
    juce::AudioProcessorValueTreeState::ButtonAttachment splitAttachment, listenAttachment;

    cruzui::ScopeView scope;
    cruzui::HeatMapView heatMap;
    cruzui::CurveEditor curve;
    cruzui::ShapeEditor shape;

    cruzui::Knob threshold, ratio, knee, range, attack, hold, release, lookahead;
    cruzui::Knob keyGain, keyHighPass, keyLowPass, splitFreq, attackBend, releaseBend, mix, output;

    juce::Rectangle<int> scopePanel, heatPanel, curvePanel, shapePanel, knobPanel;
    juce::Rectangle<int> triggerLabel, rateLabel, detectLabel;
    juce::String status;
    juce::Colour statusColour;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzSidechainAudioProcessorEditor)
};
