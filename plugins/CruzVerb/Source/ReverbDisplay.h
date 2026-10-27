#pragma once

#include <juce_dsp/juce_dsp.h>

#include "PluginProcessor.h"

// The big graph: a live spectrum of the reverb, the decay time across frequency
// (Decay EQ) and the tone curve applied to the reverb (Post EQ), with draggable nodes.
class ReverbDisplay final : public juce::Component, private juce::Timer
{
public:
    enum class Mode { decay, postEq };

    explicit ReverbDisplay (CruzVerbAudioProcessor&);
    ~ReverbDisplay() override = default;

    void setMode (Mode);
    Mode getMode() const noexcept { return mode; }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Node
    {
        const char* label;
        const char* xParam;   // may be null
        const char* yParam;   // may be null
        juce::Point<float> position;
    };

    void timerCallback() override;
    void updateSpectrum();
    std::vector<Node> currentNodes() const;
    int nodeAt (juce::Point<float>) const;

    float value (const char* id) const;
    void setValue (const char* id, float v);

    juce::Rectangle<float> plotArea() const;
    float xForFreq (double freq) const;
    double freqForX (float x) const;
    float yForDecay (double seconds) const;
    double decayForY (float y) const;
    float yForDb (double db) const;
    double dbForY (float y) const;

    void drawGrid (juce::Graphics&, juce::Rectangle<float>) const;
    void drawSpectrum (juce::Graphics&, juce::Rectangle<float>) const;
    void drawDecayCurve (juce::Graphics&, juce::Rectangle<float>) const;
    void drawPostEqCurve (juce::Graphics&, juce::Rectangle<float>) const;
    void drawNodes (juce::Graphics&) const;

    CruzVerbAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    Mode mode = Mode::decay;

    static constexpr int fftOrder = 12;
    static constexpr int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> ring {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> spectrumDb {};
    std::vector<float> pullBuffer = std::vector<float> ((size_t) AnalyzerFifo::capacity);
    int ringPos = 0;

    int hoverNode = -1, dragNode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbDisplay)
};
