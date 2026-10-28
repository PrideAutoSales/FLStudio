#pragma once

#include "DSP.h"
#include "Parameters.h"
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace cs
{
struct LiveState;

namespace fx
{
// A transposed direct form II biquad with RBJ cookbook designs. The editor uses
// the same maths to draw the EQ curve.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    enum Kind { LowShelf, HighShelf, Peak, LowCut, HighCut };
    void design (Kind kind, double sampleRate, float freq, float q, float gainDb) noexcept;
    float magnitudeDb (double freq, double sampleRate) const noexcept;

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
};

// A circular delay line read with 4-point Hermite interpolation.
struct DelayLine
{
    std::vector<float> buf;
    int mask = 0, w = 0;

    void prepare (int minSize)
    {
        int size = 16;
        while (size < minSize + 4)
            size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float maxDelay() const noexcept { return (float) (mask - 4); }

    // The sample written d samples ago (d >= 2).
    float read (float d) const noexcept
    {
        const float rp = (float) (w + mask + 1) - d;
        const int i = (int) rp;
        const float t = rp - (float) i;
        const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)];
        const float y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    float readInt (int d) const noexcept { return buf[(size_t) ((w - d) & mask)]; }
};
} // namespace fx

//==============================================================================
// The effects rack: eleven modules that run in a user-chosen order.
class FxRack
{
public:
    FxRack (juce::AudioProcessorValueTreeState& state, const LiveState& live);

    void prepare (double sampleRate, int maxBlock);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, double bpm);

    // Order is a list of FxModule values. Safe to call from any thread.
    using Order = std::array<int, kNumFx>;
    void setOrder (const Order& order);
    Order getOrder() const;
    static Order defaultOrder();
    static juce::String orderToString (const Order& order);
    // Accepts strings and lists from any version; 0.1 and 0.2 chains had seven modules.
    static Order orderFromString (const juce::String& s);
    static Order orderFromList (const juce::Array<int>& list);

    // The distortion's transfer curve, for drawing it (x in -1..1).
    static float transfer (int mode, float drive, float x);

private:
    // A parameter, read through the modulation matrix when it is a destination.
    struct Knob
    {
        std::atomic<float>* raw = nullptr;
        int dst = -1;
        juce::NormalisableRange<float> range;
    };
    Knob knob (const char* pid);
    float get (const Knob& k) const noexcept;
    bool isOn (const Knob& k) const noexcept { return k.raw->load (std::memory_order_relaxed) > 0.5f; }
    int choice (const Knob& k) const noexcept { return juce::roundToInt (k.raw->load (std::memory_order_relaxed)); }

    void processChunk (float* L, float* R, int n, double bpm);
    void hyper (float* L, float* R, int n);
    void distortion (float* L, float* R, int n);
    void flanger (float* L, float* R, int n, double bpm);
    void phaser (float* L, float* R, int n);
    void chorus (float* L, float* R, int n);
    void delay (float* L, float* R, int n, double bpm);
    void reverb (float* L, float* R, int n);
    void compressor (float* L, float* R, int n);
    void multiband (float* L, float* R, int n);
    void eq (float* L, float* R, int n);
    void filter (float* L, float* R, int n);

    juce::AudioProcessorValueTreeState& apvts;
    const LiveState& live;
    std::atomic<juce::uint64> packedOrder { 0 };
    double sampleRate = 48000.0;
    int maxBlock = 512;
    float smoothC = 0.01f;   // one-pole coefficient for ~20 ms parameter smoothing

    std::array<Knob, kNumFx> onKnobs;

    // Hyper and Widen
    Knob hypRate, hypDetune, hypVoices, hypMix, hypSize, hypWide;
    fx::DelayLine hypLine[2], wideLine;
    double hypPhase[7] {};
    float hypMixS = 0.0f, hypWideS = 0.0f, wideHp = 0.0f;

    // Distortion
    Knob distMode, distDrive, distTone, distMix, distFMode, distFType, distFFreq, distFRes;
    // 4x oversampling with linear-phase filters, so the dry signal can be lined up exactly.
    juce::dsp::Oversampling<float> oversampler { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, false, true };
    fx::DelayLine distDry[2];
    dsp::Svf distSvf[2];
    float distLp[2] {}, distHeld[2] {}, distDcX[2] {}, distDcY[2] {}, distMixS = 0.0f;
    int distCounter = 0;

    // Flanger
    Knob flaSync, flaDiv, flaRate, flaDepth, flaFeedback, flaSpread, flaMix;
    fx::DelayLine flaLine[2];
    double flaPhase = 0.0;
    float flaLast[2] {}, flaMixS = 0.0f, flaDepthS = 0.0f;

    // Phaser
    Knob phaRate, phaDepth, phaFreq, phaFeedback, phaMix, phaStages, phaSpread;
    float phaState[2][12] {}, phaLast[2] {}, phaCoef[2] {}, phaMixS = 0.0f;
    double phaPhase = 0.0;
    int phaCounter = 0;

    // Chorus
    Knob choRate, choDepth, choDelay, choFeedback, choMix, choHicut, choSpread;
    fx::DelayLine choLine[2];
    double choPhase = 0.0;
    float choLast[2] {}, choLp[2][2] {}, choMixS = 0.0f, choDelayS = -1.0f;

    // Delay
    Knob dlySync, dlyDiv, dlyTime, dlyFeedback, dlyPing, dlyHicut, dlyMix, dlyLowcut, dlyOffset;
    fx::DelayLine dlyLine[2];
    float dlyTimeS[2] { -1.0f, -1.0f }, dlyLp[2] {}, dlyHp[2] {}, dlyMixS = 0.0f, dlyFbS = 0.0f;

    // Reverb (an eight-line feedback delay network)
    static constexpr int kRevLines = 8;
    Knob revMode, revSize, revDecay, revDamp, revWidth, revMix, revPre, revLowcut, revHicut;
    fx::DelayLine revPreLine[2], revLine[kRevLines], revDiff[2][4];
    float revDamping[kRevLines] {}, revSizeS = -1.0f, revMixS = 0.0f;
    double revModPhase = 0.0;
    dsp::Svf revHp[2], revLp[2];

    // Compressor
    struct Compressor
    {
        float env = 0.0f;
        float process (float x, float thr, float ratio, float att, float rel, float upward) noexcept;
    };
    Knob cmpMulti, cmpThresh, cmpRatio, cmpAttack, cmpRelease, cmpUpward, cmpGain, cmpMix;
    Compressor comp[3][2];
    juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit;

    // Multiband
    Knob mbDepth, mbTime, mbUp, mbDown, mbLow, mbMid, mbHigh, mbGain;
    juce::dsp::LinkwitzRileyFilter<float> mbLowSplit, mbHighSplit, mbAllpass;
    float mbEnv[3] {};

    // EQ
    Knob eqLowType, eqLowFreq, eqLowQ, eqLowGain, eqHighType, eqHighFreq, eqHighQ, eqHighGain;
    fx::Biquad eqLow[2], eqHigh[2];
    float eqCache[8] {};

    // Filter
    Knob filType, filCutoff, filRes, filDrive, filStereo, filMix;
    dsp::Svf filSvf[2][2];
    float filCutS = -1.0f, filMixS = 0.0f;
};
} // namespace cs
