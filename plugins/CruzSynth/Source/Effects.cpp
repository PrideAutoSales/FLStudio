#include "Effects.h"

namespace cs
{
namespace
{
    float load (std::atomic<float>* v) { return v->load (std::memory_order_relaxed); }
    bool on (std::atomic<float>* v)    { return load (v) > 0.5f; }
}

FxRack::FxRack (juce::AudioProcessorValueTreeState& s) : apvts (s)
{
    setOrder ({ FxDistortion, FxChorus, FxPhaser, FxDelay, FxReverb, FxCompressor, FxEq });
}

void FxRack::setOrder (const std::array<int, kNumFx>& order)
{
    juce::uint32 packed = 0;
    for (int i = 0; i < kNumFx; ++i)
        packed |= (juce::uint32) (order[(size_t) i] & 0xf) << (i * 4);
    packedOrder.store (packed);
}

std::array<int, kNumFx> FxRack::getOrder() const
{
    std::array<int, kNumFx> order {};
    const auto packed = packedOrder.load();
    for (int i = 0; i < kNumFx; ++i)
        order[(size_t) i] = (int) ((packed >> (i * 4)) & 0xf);
    return order;
}

juce::String FxRack::orderToString (const std::array<int, kNumFx>& order)
{
    juce::String s;
    for (auto v : order)
        s << v;
    return s;
}

std::array<int, kNumFx> FxRack::orderFromString (const juce::String& s)
{
    std::array<int, kNumFx> order { 0, 1, 2, 3, 4, 5, 6 };
    if (s.length() != kNumFx)
        return order;
    bool seen[kNumFx] {};
    for (int i = 0; i < kNumFx; ++i)
    {
        const int v = s[i] - '0';
        if (v < 0 || v >= kNumFx || seen[v])
            return { 0, 1, 2, 3, 4, 5, 6 };
        seen[v] = true;
        order[(size_t) i] = v;
    }
    return order;
}

void FxRack::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    chorusFx.prepare (spec);
    phaserFx.prepare (spec);
    reverbFx.prepare (spec);
    for (auto* f : { &lowSplit, &highSplit })
        f->prepare (spec);
    lowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowSplit.setCutoffFrequency (120.0f);
    highSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highSplit.setCutoffFrequency (2500.0f);

    for (auto& d : delayBuf)
        d.assign ((size_t) (sr * 2.5) + 8, 0.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        lowShelf[ch].prepare ({ sr, (juce::uint32) maxBlock, 1 });
        highShelf[ch].prepare ({ sr, (juce::uint32) maxBlock, 1 });
    }
    std::fill (std::begin (eqCache), std::end (eqCache), -1.0f);
    reset();
}

void FxRack::reset()
{
    chorusFx.reset();
    phaserFx.reset();
    reverbFx.reset();
    lowSplit.reset();
    highSplit.reset();
    for (auto& d : delayBuf)
        std::fill (d.begin(), d.end(), 0.0f);
    delayWrite = 0;
    delaySmoothed = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
    {
        distLp[ch] = distHeld[ch] = delayLp[ch] = 0.0f;
        lowShelf[ch].reset();
        highShelf[ch].reset();
    }
    for (auto& band : comp)
        for (auto& c : band)
            c.env = 0.0f;
}

void FxRack::process (juce::AudioBuffer<float>& b, double bpm)
{
    for (auto m : getOrder())
    {
        switch (m)
        {
            case FxDistortion: if (on (p ("fx_dist_on"))) distortion (b); break;
            case FxChorus:     if (on (p ("fx_cho_on")))  chorus (b); break;
            case FxPhaser:     if (on (p ("fx_pha_on")))  phaser (b); break;
            case FxDelay:      if (on (p ("fx_dly_on")))  delay (b, bpm); break;
            case FxReverb:     if (on (p ("fx_rev_on")))  reverb (b); break;
            case FxCompressor: if (on (p ("fx_cmp_on")))  compressor (b); break;
            case FxEq:         if (on (p ("fx_eq_on")))   eq (b); break;
            default: break;
        }
    }
}

//==============================================================================
void FxRack::distortion (juce::AudioBuffer<float>& b)
{
    const int mode = juce::roundToInt (load (p ("fx_dist_mode")));
    const float drive = load (p ("fx_dist_drive"));
    const float mix = load (p ("fx_dist_mix"));
    const float tone = load (p ("fx_dist_tone"));
    const float g = std::pow (10.0f, drive * 36.0f / 20.0f);
    const float lpHz = 800.0f * std::pow (25.0f, tone);
    const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * lpHz / (float) sampleRate);
    const float bits = std::exp2 (12.0f - drive * 10.0f);
    const int hold = 1 + (int) (drive * drive * 40.0f);

    for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
    {
        auto* x = b.getWritePointer (ch);
        int counter = distCounter;
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float in = x[i];
            float y;
            switch (mode)
            {
                case 0:  y = std::tanh (in * g + 0.25f) - std::tanh (0.25f); break;
                case 1:  y = std::tanh (in * g); break;
                case 2:  y = juce::jlimit (-1.0f, 1.0f, in * g); break;
                case 3:  y = std::sin (in * g * 1.5707963f); break;
                case 4:  y = std::round (in * bits) / bits; break;
                default:
                    if (counter % hold == 0) distHeld[ch] = in;
                    y = distHeld[ch];
                    ++counter;
                    break;
            }
            distLp[ch] += (y - distLp[ch]) * lpC;
            x[i] = in + (distLp[ch] - in) * mix;
        }
        if (ch == juce::jmin (2, b.getNumChannels()) - 1)
            distCounter = counter % 100000;
    }
}

void FxRack::chorus (juce::AudioBuffer<float>& b)
{
    chorusFx.setRate (load (p ("fx_cho_rate")));
    chorusFx.setDepth (load (p ("fx_cho_depth")));
    chorusFx.setCentreDelay (load (p ("fx_cho_delay")));
    chorusFx.setFeedback (load (p ("fx_cho_feedback")));
    chorusFx.setMix (load (p ("fx_cho_mix")));
    juce::dsp::AudioBlock<float> block (b);
    chorusFx.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void FxRack::phaser (juce::AudioBuffer<float>& b)
{
    phaserFx.setRate (load (p ("fx_pha_rate")));
    phaserFx.setDepth (load (p ("fx_pha_depth")));
    phaserFx.setCentreFrequency (load (p ("fx_pha_freq")));
    phaserFx.setFeedback (load (p ("fx_pha_feedback")));
    phaserFx.setMix (load (p ("fx_pha_mix")));
    juce::dsp::AudioBlock<float> block (b);
    phaserFx.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void FxRack::delay (juce::AudioBuffer<float>& b, double bpm)
{
    const bool sync = on (p ("fx_dly_sync"));
    const double seconds = sync ? syncDivisionBeats (juce::roundToInt (load (p ("fx_dly_div")))) * 60.0 / juce::jmax (20.0, bpm)
                                : load (p ("fx_dly_time")) / 1000.0;
    const int size = (int) delayBuf[0].size();
    const float target = juce::jlimit (1.0f, (float) size - 4.0f, (float) (seconds * sampleRate));
    if (delaySmoothed <= 0.0f)
        delaySmoothed = target;

    const float fb = load (p ("fx_dly_feedback"));
    const bool ping = on (p ("fx_dly_pingpong"));
    const float mix = load (p ("fx_dly_mix"));
    const float lpC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * load (p ("fx_dly_hicut")) / (float) sampleRate);

    auto* L = b.getWritePointer (0);
    auto* R = b.getWritePointer (b.getNumChannels() > 1 ? 1 : 0);

    auto readAt = [&] (const std::vector<float>& buf, float d)
    {
        float rp = (float) delayWrite - d;
        while (rp < 0.0f) rp += (float) size;
        const int i0 = (int) rp;
        const int i1 = (i0 + 1) % size;
        const float t = rp - (float) i0;
        return buf[(size_t) i0] + t * (buf[(size_t) i1] - buf[(size_t) i0]);
    };

    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        delaySmoothed += (target - delaySmoothed) * 0.0005f;
        float dl = readAt (delayBuf[0], delaySmoothed);
        float dr = readAt (delayBuf[1], delaySmoothed);
        delayLp[0] += (dl - delayLp[0]) * lpC; dl = delayLp[0];
        delayLp[1] += (dr - delayLp[1]) * lpC; dr = delayLp[1];

        const float inL = L[i], inR = R[i];
        if (ping)
        {
            delayBuf[0][(size_t) delayWrite] = (inL + inR) * 0.5f + fb * dr;
            delayBuf[1][(size_t) delayWrite] = fb * dl;
        }
        else
        {
            delayBuf[0][(size_t) delayWrite] = inL + fb * dl;
            delayBuf[1][(size_t) delayWrite] = inR + fb * dr;
        }
        delayWrite = (delayWrite + 1) % size;

        L[i] = inL * (1.0f - mix * 0.5f) + dl * mix;
        R[i] = inR * (1.0f - mix * 0.5f) + dr * mix;
    }
}

void FxRack::reverb (juce::AudioBuffer<float>& b)
{
    juce::dsp::Reverb::Parameters rp;
    rp.roomSize = load (p ("fx_rev_size"));
    rp.damping = load (p ("fx_rev_damp"));
    rp.width = load (p ("fx_rev_width"));
    const float mix = load (p ("fx_rev_mix"));
    rp.wetLevel = mix;
    rp.dryLevel = 1.0f - mix * 0.6f;
    reverbFx.setParameters (rp);
    juce::dsp::AudioBlock<float> block (b);
    reverbFx.process (juce::dsp::ProcessContextReplacing<float> (block));
}

float FxRack::Compressor::process (float x, float thr, float ratio, float att, float rel, float upward) noexcept
{
    const float a = std::abs (x);
    env += (a - env) * (a > env ? att : rel);
    const float lvl = juce::Decibels::gainToDecibels (env + 1.0e-9f, -120.0f);
    float gainDb = 0.0f;
    if (lvl > thr)
        gainDb = (thr - lvl) * (1.0f - 1.0f / ratio);
    else if (upward > 0.0f && lvl > -70.0f)
        gainDb = juce::jmin (18.0f, (thr - lvl) * (1.0f - 1.0f / ratio)) * upward;
    return x * juce::Decibels::decibelsToGain (gainDb);
}

void FxRack::compressor (juce::AudioBuffer<float>& b)
{
    const float thr = load (p ("fx_cmp_thresh"));
    const float ratio = load (p ("fx_cmp_ratio"));
    const float att = 1.0f - std::exp (-1.0f / (0.001f * load (p ("fx_cmp_attack")) * (float) sampleRate));
    const float rel = 1.0f - std::exp (-1.0f / (0.001f * load (p ("fx_cmp_release")) * (float) sampleRate));
    const float up = load (p ("fx_cmp_upward"));
    const float makeup = juce::Decibels::decibelsToGain (load (p ("fx_cmp_gain")));
    const float mix = load (p ("fx_cmp_mix"));
    const bool multi = on (p ("fx_cmp_multi"));
    const int n = b.getNumSamples();
    const int chans = juce::jmin (2, b.getNumChannels());

    if (! multi)
    {
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* x = b.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                const float y = comp[0][ch].process (x[i], thr, ratio, att, rel, up) * makeup;
                x[i] += (y - x[i]) * mix;
            }
        }
        return;
    }

    // Three bands: low (<120 Hz), mid, high (>2.5 kHz), each compressed on its own.
    for (int ch = 0; ch < chans; ++ch)
    {
        auto* x = b.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float lo2 = 0.0f, hi2 = 0.0f, lo1 = 0.0f, hi1 = 0.0f;
            highSplit.processSample (ch, x[i], lo2, hi2);   // split at 2.5k
            lowSplit.processSample (ch, lo2, lo1, hi1);     // split the lower part at 120
            const float low = comp[0][ch].process (lo1, thr, ratio, att, rel, up);
            const float mid = comp[1][ch].process (hi1, thr, ratio, att, rel, up);
            const float high = comp[2][ch].process (hi2, thr, ratio, att, rel, up);
            const float y = (low + mid + high) * makeup;
            x[i] += (y - x[i]) * mix;
        }
    }
}

void FxRack::eq (juce::AudioBuffer<float>& b)
{
    const float lf = load (p ("fx_eq_lowfreq")), lg = load (p ("fx_eq_lowgain"));
    const float hf = load (p ("fx_eq_highfreq")), hg = load (p ("fx_eq_highgain"));
    if (lf != eqCache[0] || lg != eqCache[1] || hf != eqCache[2] || hg != eqCache[3])
    {
        eqCache[0] = lf; eqCache[1] = lg; eqCache[2] = hf; eqCache[3] = hg;
        auto lc = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, lf, 0.7f, juce::Decibels::decibelsToGain (lg));
        auto hc = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, juce::jmin (hf, (float) sampleRate * 0.45f), 0.7f, juce::Decibels::decibelsToGain (hg));
        for (int ch = 0; ch < 2; ++ch)
        {
            *lowShelf[ch].coefficients = *lc;
            *highShelf[ch].coefficients = *hc;
        }
    }
    for (int ch = 0; ch < juce::jmin (2, b.getNumChannels()); ++ch)
    {
        auto* x = b.getWritePointer (ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            x[i] = highShelf[ch].processSample (lowShelf[ch].processSample (x[i]));
    }
}
} // namespace cs
