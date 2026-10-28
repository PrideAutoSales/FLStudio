#include "Effects.h"
#include "Engine.h"
#include <complex>

namespace cs
{
using dsp::fastTanh;
using dsp::pi;
using dsp::twoPi;

namespace
{
    float onePole (double sampleRate, float hz)
    {
        return 1.0f - std::exp (-twoPi * hz / (float) sampleRate);
    }

    float wrap (float x) noexcept { return x - std::floor (x); }
}

//==============================================================================
namespace fx
{
void Biquad::design (Kind kind, double sr, float freq, float q, float gainDb) noexcept
{
    const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit (10.0, sr * 0.49, (double) freq) / sr;
    const double cw = std::cos (w0), sw = std::sin (w0);
    const double alpha = sw / (2.0 * juce::jmax (0.05f, q));
    const double A = std::pow (10.0, gainDb / 40.0);
    const double sA2a = 2.0 * std::sqrt (A) * alpha;
    double B0 = 1, B1 = 0, B2 = 0, A0 = 1, A1 = 0, A2 = 0;

    switch (kind)
    {
        case LowShelf:
            B0 = A * ((A + 1) - (A - 1) * cw + sA2a);
            B1 = 2 * A * ((A - 1) - (A + 1) * cw);
            B2 = A * ((A + 1) - (A - 1) * cw - sA2a);
            A0 = (A + 1) + (A - 1) * cw + sA2a;
            A1 = -2 * ((A - 1) + (A + 1) * cw);
            A2 = (A + 1) + (A - 1) * cw - sA2a;
            break;
        case HighShelf:
            B0 = A * ((A + 1) + (A - 1) * cw + sA2a);
            B1 = -2 * A * ((A - 1) + (A + 1) * cw);
            B2 = A * ((A + 1) + (A - 1) * cw - sA2a);
            A0 = (A + 1) - (A - 1) * cw + sA2a;
            A1 = 2 * ((A - 1) - (A + 1) * cw);
            A2 = (A + 1) - (A - 1) * cw - sA2a;
            break;
        case Peak:
            B0 = 1 + alpha * A; B1 = -2 * cw; B2 = 1 - alpha * A;
            A0 = 1 + alpha / A; A1 = -2 * cw; A2 = 1 - alpha / A;
            break;
        case LowCut:
            B0 = (1 + cw) * 0.5; B1 = -(1 + cw); B2 = B0;
            A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
            break;
        case HighCut:
            B0 = (1 - cw) * 0.5; B1 = 1 - cw; B2 = B0;
            A0 = 1 + alpha; A1 = -2 * cw; A2 = 1 - alpha;
            break;
    }
    b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0);
    a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
}

float Biquad::magnitudeDb (double freq, double sr) const noexcept
{
    const auto z = std::polar (1.0, -juce::MathConstants<double>::twoPi * freq / sr);
    const auto num = (double) b0 + (double) b1 * z + (double) b2 * z * z;
    const auto den = 1.0 + (double) a1 * z + (double) a2 * z * z;
    return (float) (20.0 * std::log10 (std::abs (num / den) + 1.0e-12));
}
} // namespace fx

//==============================================================================
FxRack::FxRack (juce::AudioProcessorValueTreeState& s, const LiveState& l) : apvts (s), live (l)
{
    for (int m = 0; m < kNumFx; ++m)
        onKnobs[(size_t) m] = knob (fxOnParamID (m).toRawUTF8());

    hypRate = knob ("fx_hyp_rate"); hypDetune = knob ("fx_hyp_detune"); hypVoices = knob ("fx_hyp_voices");
    hypMix = knob ("fx_hyp_mix"); hypSize = knob ("fx_hyp_size"); hypWide = knob ("fx_hyp_wide");

    distMode = knob ("fx_dist_mode"); distDrive = knob ("fx_dist_drive"); distTone = knob ("fx_dist_tone");
    distMix = knob ("fx_dist_mix"); distFMode = knob ("fx_dist_fmode"); distFType = knob ("fx_dist_ftype");
    distFFreq = knob ("fx_dist_ffreq"); distFRes = knob ("fx_dist_fres");

    flaSync = knob ("fx_fla_sync"); flaDiv = knob ("fx_fla_div"); flaRate = knob ("fx_fla_rate");
    flaDepth = knob ("fx_fla_depth"); flaFeedback = knob ("fx_fla_feedback"); flaSpread = knob ("fx_fla_spread");
    flaMix = knob ("fx_fla_mix");

    phaRate = knob ("fx_pha_rate"); phaDepth = knob ("fx_pha_depth"); phaFreq = knob ("fx_pha_freq");
    phaFeedback = knob ("fx_pha_feedback"); phaMix = knob ("fx_pha_mix"); phaStages = knob ("fx_pha_stages");
    phaSpread = knob ("fx_pha_spread");

    choRate = knob ("fx_cho_rate"); choDepth = knob ("fx_cho_depth"); choDelay = knob ("fx_cho_delay");
    choFeedback = knob ("fx_cho_feedback"); choMix = knob ("fx_cho_mix"); choHicut = knob ("fx_cho_hicut");
    choSpread = knob ("fx_cho_spread");

    dlySync = knob ("fx_dly_sync"); dlyDiv = knob ("fx_dly_div"); dlyTime = knob ("fx_dly_time");
    dlyFeedback = knob ("fx_dly_feedback"); dlyPing = knob ("fx_dly_pingpong"); dlyHicut = knob ("fx_dly_hicut");
    dlyMix = knob ("fx_dly_mix"); dlyLowcut = knob ("fx_dly_lowcut"); dlyOffset = knob ("fx_dly_offset");

    revMode = knob ("fx_rev_mode"); revSize = knob ("fx_rev_size"); revDecay = knob ("fx_rev_decay");
    revDamp = knob ("fx_rev_damp"); revWidth = knob ("fx_rev_width"); revMix = knob ("fx_rev_mix");
    revPre = knob ("fx_rev_predelay"); revLowcut = knob ("fx_rev_lowcut"); revHicut = knob ("fx_rev_hicut");

    cmpMulti = knob ("fx_cmp_multi"); cmpThresh = knob ("fx_cmp_thresh"); cmpRatio = knob ("fx_cmp_ratio");
    cmpAttack = knob ("fx_cmp_attack"); cmpRelease = knob ("fx_cmp_release"); cmpUpward = knob ("fx_cmp_upward");
    cmpGain = knob ("fx_cmp_gain"); cmpMix = knob ("fx_cmp_mix");

    mbDepth = knob ("fx_mb_depth"); mbTime = knob ("fx_mb_time"); mbUp = knob ("fx_mb_up"); mbDown = knob ("fx_mb_down");
    mbLow = knob ("fx_mb_low"); mbMid = knob ("fx_mb_mid"); mbHigh = knob ("fx_mb_high"); mbGain = knob ("fx_mb_gain");

    eqLowType = knob ("fx_eq_lowtype"); eqLowFreq = knob ("fx_eq_lowfreq"); eqLowQ = knob ("fx_eq_lowq");
    eqLowGain = knob ("fx_eq_lowgain"); eqHighType = knob ("fx_eq_hightype"); eqHighFreq = knob ("fx_eq_highfreq");
    eqHighQ = knob ("fx_eq_highq"); eqHighGain = knob ("fx_eq_highgain");

    filType = knob ("fx_fil_type"); filCutoff = knob ("fx_fil_cutoff"); filRes = knob ("fx_fil_res");
    filDrive = knob ("fx_fil_drive"); filStereo = knob ("fx_fil_stereo"); filMix = knob ("fx_fil_mix");

    setOrder (defaultOrder());
}

FxRack::Knob FxRack::knob (const char* pid)
{
    Knob k;
    k.raw = apvts.getRawParameterValue (pid);
    jassert (k.raw != nullptr);
    k.dst = modDestinationForParam (pid);
    if (auto* p = apvts.getParameter (pid))
        k.range = p->getNormalisableRange();
    return k;
}

float FxRack::get (const Knob& k) const noexcept
{
    if (k.dst > 0)
        return k.range.convertFrom0to1 (live.dstNorm[(size_t) k.dst].load (std::memory_order_relaxed));
    return k.raw->load (std::memory_order_relaxed);
}

//==============================================================================
FxRack::Order FxRack::defaultOrder()
{
    return { FxHyper, FxDistortion, FxFlanger, FxChorus, FxPhaser, FxDelay, FxReverb,
             FxCompressor, FxMultiband, FxEq, FxFilter };
}

void FxRack::setOrder (const Order& order)
{
    juce::uint64 packed = 0;
    for (int i = 0; i < kNumFx; ++i)
        packed |= (juce::uint64) (order[(size_t) i] & 0xf) << (i * 4);
    packedOrder.store (packed);
}

FxRack::Order FxRack::getOrder() const
{
    Order order {};
    const auto packed = packedOrder.load();
    for (int i = 0; i < kNumFx; ++i)
        order[(size_t) i] = (int) ((packed >> (i * 4)) & 0xf);
    return order;
}

juce::String FxRack::orderToString (const Order& order)
{
    juce::String s;
    for (auto v : order)
        s << juce::String::toHexString (v);
    return s;
}

FxRack::Order FxRack::orderFromString (const juce::String& s)
{
    juce::Array<int> list;
    for (auto c : s)
        list.add (juce::CharacterFunctions::getHexDigitValue (c));
    return orderFromList (list);
}

FxRack::Order FxRack::orderFromList (const juce::Array<int>& input)
{
    juce::Array<int> list;
    if (input.size() == 7)
    {
        // An older chain: slot the new modules in where the default chain has them.
        list.add (FxHyper);
        for (auto v : input)
        {
            list.add (v);
            if (v == FxDistortion) list.add (FxFlanger);
            if (v == FxCompressor) list.add (FxMultiband);
        }
        list.add (FxFilter);
    }
    else
    {
        list = input;
    }

    if (list.size() != kNumFx)
        return defaultOrder();
    Order order {};
    bool seen[kNumFx] {};
    for (int i = 0; i < kNumFx; ++i)
    {
        const int v = list[i];
        if (v < 0 || v >= kNumFx || seen[v])
            return defaultOrder();
        seen[v] = true;
        order[(size_t) i] = v;
    }
    return order;
}

//==============================================================================
void FxRack::prepare (double sr, int block)
{
    sampleRate = sr;
    maxBlock = juce::jmax (16, block);
    smoothC = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));

    for (auto& d : hypLine) d.prepare ((int) (0.05 * sr));
    wideLine.prepare ((int) (0.06 * sr));

    oversampler.initProcessing ((size_t) maxBlock);
    for (auto& d : distDry) d.prepare ((int) oversampler.getLatencyInSamples() + maxBlock + 8);
    for (auto& d : flaLine) d.prepare ((int) (0.02 * sr));
    for (auto& d : choLine) d.prepare ((int) (0.08 * sr));
    for (auto& d : dlyLine) d.prepare ((int) (sr * 3.0));

    for (auto& d : revPreLine) d.prepare ((int) (0.26 * sr));
    for (auto& d : revLine) d.prepare ((int) (0.13 * sr));
    for (auto& ch : revDiff) for (auto& d : ch) d.prepare ((int) (0.02 * sr));

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    for (auto* f : { &lowSplit, &highSplit, &mbLowSplit, &mbHighSplit, &mbAllpass })
        f->prepare (spec);
    lowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    lowSplit.setCutoffFrequency (120.0f);
    highSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    highSplit.setCutoffFrequency (2500.0f);
    mbLowSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    mbLowSplit.setCutoffFrequency (120.0f);
    mbHighSplit.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    mbHighSplit.setCutoffFrequency (2500.0f);
    mbAllpass.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    mbAllpass.setCutoffFrequency (120.0f);

    std::fill (std::begin (eqCache), std::end (eqCache), -999.0f);
    reset();
}

void FxRack::reset()
{
    for (auto& d : hypLine) d.clear();
    wideLine.clear();
    for (auto& p : hypPhase) p = 0.0;
    hypMixS = hypWideS = wideHp = 0.0f;

    oversampler.reset();
    for (auto& d : distDry) d.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        distSvf[ch].reset();
        distLp[ch] = distHeld[ch] = distDcX[ch] = distDcY[ch] = 0.0f;
        flaLast[ch] = phaLast[ch] = choLast[ch] = 0.0f;
        dlyLp[ch] = dlyHp[ch] = 0.0f;
        choLp[ch][0] = choLp[ch][1] = 0.0f;
        std::fill (std::begin (phaState[ch]), std::end (phaState[ch]), 0.0f);
        eqLow[ch].reset();
        eqHigh[ch].reset();
        revHp[ch].reset();
        revLp[ch].reset();
        for (auto& s : filSvf[ch]) s.reset();
    }
    distMixS = flaMixS = phaMixS = choMixS = dlyMixS = revMixS = filMixS = 0.0f;
    flaDepthS = dlyFbS = 0.0f;
    choDelayS = filCutS = revSizeS = -1.0f;
    dlyTimeS[0] = dlyTimeS[1] = -1.0f;
    distCounter = phaCounter = 0;
    flaPhase = phaPhase = choPhase = revModPhase = 0.0;

    for (auto& d : flaLine) d.clear();
    for (auto& d : choLine) d.clear();
    for (auto& d : dlyLine) d.clear();
    for (auto& d : revPreLine) d.clear();
    for (auto& d : revLine) d.clear();
    for (auto& ch : revDiff) for (auto& d : ch) d.clear();
    std::fill (std::begin (revDamping), std::end (revDamping), 0.0f);

    for (auto* f : { &lowSplit, &highSplit, &mbLowSplit, &mbHighSplit, &mbAllpass })
        f->reset();
    for (auto& band : comp)
        for (auto& c : band)
            c.env = 0.0f;
    std::fill (std::begin (mbEnv), std::end (mbEnv), 0.0f);
}

void FxRack::process (juce::AudioBuffer<float>& b, double bpm)
{
    juce::ScopedNoDenormals noDenormals;
    auto* L = b.getWritePointer (0);
    auto* R = b.getWritePointer (b.getNumChannels() > 1 ? 1 : 0);
    const int n = b.getNumSamples();
    for (int start = 0; start < n; start += maxBlock)
        processChunk (L + start, R + start, juce::jmin (maxBlock, n - start), bpm);
}

void FxRack::processChunk (float* L, float* R, int n, double bpm)
{
    for (auto m : getOrder())
    {
        if (! isOn (onKnobs[(size_t) m]))
        {
            // Start from silence and a closed mix the next time the module is switched on.
            switch (m)
            {
                case FxHyper:      if (hypMixS > 0.0f || hypWideS > 0.0f) { for (auto& d : hypLine) d.clear(); wideLine.clear(); hypMixS = hypWideS = 0.0f; } break;
                case FxDistortion: distMixS = 0.0f; break;
                case FxFlanger:    if (flaMixS > 0.0f) { for (auto& d : flaLine) d.clear(); flaMixS = 0.0f; flaLast[0] = flaLast[1] = 0.0f; } break;
                case FxPhaser:     phaMixS = 0.0f; break;
                case FxChorus:     if (choMixS > 0.0f) { for (auto& d : choLine) d.clear(); choMixS = 0.0f; } break;
                case FxDelay:      if (dlyMixS > 0.0f) { for (auto& d : dlyLine) d.clear(); dlyMixS = 0.0f; } break;
                case FxReverb:     if (revMixS > 0.0f) { for (auto& d : revLine) d.clear(); for (auto& d : revPreLine) d.clear(); revMixS = 0.0f; } break;
                case FxFilter:     filMixS = 0.0f; break;
                default: break;
            }
            continue;
        }

        switch (m)
        {
            case FxHyper:      hyper (L, R, n); break;
            case FxDistortion: distortion (L, R, n); break;
            case FxFlanger:    flanger (L, R, n, bpm); break;
            case FxPhaser:     phaser (L, R, n); break;
            case FxChorus:     chorus (L, R, n); break;
            case FxDelay:      delay (L, R, n, bpm); break;
            case FxReverb:     reverb (L, R, n); break;
            case FxCompressor: compressor (L, R, n); break;
            case FxMultiband:  multiband (L, R, n); break;
            case FxEq:         eq (L, R, n); break;
            case FxFilter:     filter (L, R, n); break;
            default: break;
        }
    }
}

//==============================================================================
// Hyper: up to seven slowly drifting copies spread across the stereo field, like
// an extra unison stage for the whole patch. Widen adds a mono-safe side signal.
void FxRack::hyper (float* L, float* R, int n)
{
    static constexpr float mult[7] = { 1.0f, 1.13f, 0.87f, 1.29f, 0.71f, 1.43f, 0.61f };
    const float rate = get (hypRate), detune = get (hypDetune), mixT = get (hypMix);
    const float size = get (hypSize), wideT = get (hypWide);
    const int voices = juce::jlimit (2, 7, choice (hypVoices));
    const float sr = (float) sampleRate;

    float inc[7], amp[7], base[7], gl[7], gr[7];
    for (int k = 0; k < voices; ++k)
    {
        const float r = rate * mult[k];
        inc[k] = r / sr;
        // A modulated delay detunes by about A * 2 pi f, so this gives up to ~30 cents.
        const float ampSec = juce::jmin (0.006f, detune * 0.0173f / (twoPi * r));
        amp[k] = ampSec * sr;
        base[k] = (0.004f + 0.0011f * (float) k) * sr + amp[k] + 2.0f;
        const float pan = -1.0f + 2.0f * (float) k / (float) (voices - 1);
        const float th = (pan + 1.0f) * pi * 0.25f;
        gl[k] = std::cos (th);
        gr[k] = std::sin (th);
    }
    const float norm = std::sqrt (2.0f / (float) voices);

    for (int i = 0; i < n; ++i)
    {
        hypLine[0].push (L[i]);
        hypLine[1].push (R[i]);
        float wl = 0.0f, wr = 0.0f;
        for (int k = 0; k < voices; ++k)
        {
            hypPhase[k] += inc[k];
            if (hypPhase[k] >= 1.0) hypPhase[k] -= 1.0;
            const float d = base[k] + amp[k] * std::sin (twoPi * (float) hypPhase[k]);
            const float v = hypLine[k & 1].read (d);
            wl += v * gl[k];
            wr += v * gr[k];
        }
        // Equal-power blend: the copies are mostly uncorrelated with the dry signal.
        hypMixS += (mixT - hypMixS) * smoothC;
        const float dg = std::cos (hypMixS * pi * 0.5f), wg = std::sin (hypMixS * pi * 0.5f) * norm;
        L[i] = L[i] * dg + wl * wg;
        R[i] = R[i] * dg + wr * wg;
    }

    if (wideT <= 0.0f && hypWideS <= 1.0e-4f)
        return;
    const float t1 = (0.006f + 0.014f * size) * sr;
    const float t2 = t1 * 1.47f;
    const float hpC = onePole (sampleRate, 180.0f);
    for (int i = 0; i < n; ++i)
    {
        wideLine.push ((L[i] + R[i]) * 0.5f);
        const float side = (wideLine.read (t1) - wideLine.read (t2)) * 0.5f;
        wideHp += (side - wideHp) * hpC;
        hypWideS += (wideT - hypWideS) * smoothC;
        const float s = (side - wideHp) * hypWideS;
        L[i] += s;
        R[i] -= s;
    }
}


//==============================================================================
namespace
{
    // Waveshapers for the distortion modes that take a driven input.
    float shape (int mode, float x) noexcept
    {
        switch (mode)
        {
            case 0:  return std::tanh (x + 0.25f) - 0.24491866f;                          // Tube
            case 1:  return std::tanh (x);                                                // Soft clip
            case 2:  return juce::jlimit (-1.0f, 1.0f, x);                                // Hard clip
            case 3:  return std::sin (x * 1.5707963f);                                    // Sine fold
            case 6:  return x >= 0.0f ? 1.0f - std::exp (-x) : -0.5f * (1.0f - std::exp (0.7f * x)); // Diode
            case 7:  return 1.0f - 4.0f * std::abs (wrap ((x + 1.0f) * 0.25f) - 0.5f);  // Linear fold
            case 8:  return x >= 0.0f ? std::tanh (x) : 0.6f * std::tanh (0.6f * x);      // Asym
            case 9:  return 1.4f * std::abs (std::tanh (x));                              // Rectify (octave up)
            case 10: { const float a = std::abs (x); return std::copysign (a * a / (1.0f + a * a), x); } // Squash
            case 11: return std::tanh (3.0f * std::tanh (x) + 0.3f) - 0.29131261f;       // Fuzz
            case 12: { const float c = juce::jlimit (-1.5f, 1.5f, x); return c - c * c * c / 6.75f; } // Tape
            default: return x;
        }
    }
}

float FxRack::transfer (int mode, float drive, float x)
{
    if (mode == 4)
    {
        const float bits = std::exp2 (12.0f - drive * 10.0f);
        return std::round (x * bits) / bits;
    }
    return mode == 5 ? x : shape (mode, x * std::pow (10.0f, drive * 36.0f / 20.0f));
}

void FxRack::distortion (float* L, float* R, int n)
{
    const int mode = choice (distMode);
    const float drive = get (distDrive);
    const float mixT = get (distMix);
    const float tone = get (distTone);
    const int fmode = choice (distFMode), ftype = choice (distFType);
    const float g = std::pow (10.0f, drive * 36.0f / 20.0f);
    const float lpC = onePole (sampleRate, juce::jmin (20000.0f, 800.0f * std::pow (25.0f, tone)));
    const float bits = std::exp2 (12.0f - drive * 10.0f);
    const int hold = 4 * (1 + (int) (drive * drive * 40.0f));   // the shaper runs at 4x
    const int lat = (int) oversampler.getLatencyInSamples();
    float* chans[2] = { L, R };

    if (fmode != 0)
    {
        const float fc = juce::jlimit (20.0f, (float) sampleRate * 0.45f, get (distFFreq));
        for (auto& s : distSvf)
            s.setCoefs (std::tan (pi * fc / (float) sampleRate), 2.0f - 1.9f * get (distFRes));
    }
    auto svf = [&] (int ch, float x)
    {
        const auto o = distSvf[ch].process (x);
        return ftype == 0 ? o.lp : ftype == 1 ? o.bp * distSvf[ch].k : o.hp;
    };

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
        {
            distDry[ch].push (chans[ch][i]);
            if (fmode == 1)
                chans[ch][i] = svf (ch, chans[ch][i]);
        }

    juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
    auto up = oversampler.processSamplesUp (block);
    int counter = distCounter;
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* x = up.getChannelPointer ((size_t) ch);
        counter = distCounter;
        for (size_t j = 0; j < up.getNumSamples(); ++j)
        {
            const float in = x[j];
            if (mode == 4)
                x[j] = std::round (in * bits) / bits;
            else if (mode == 5)
            {
                if (counter % hold == 0) distHeld[ch] = in;
                x[j] = distHeld[ch];
                ++counter;
            }
            else
                x[j] = shape (mode, in * g);
        }
    }
    distCounter = counter % 100000;
    oversampler.processSamplesDown (block);

    for (int i = 0; i < n; ++i)
    {
        distMixS += (mixT - distMixS) * smoothC;
        for (int ch = 0; ch < 2; ++ch)
        {
            float y = chans[ch][i];
            if (fmode == 2)
                y = svf (ch, y);
            distLp[ch] += (y - distLp[ch]) * lpC;
            y = distLp[ch] - distDcX[ch] + 0.9995f * distDcY[ch];   // DC blocker
            distDcX[ch] = distLp[ch];
            distDcY[ch] = y;
            const float dry = distDry[ch].readInt (lat + 1 + (n - 1 - i));
            chans[ch][i] = dry + (y - dry) * distMixS;
        }
    }
}

//==============================================================================
void FxRack::flanger (float* L, float* R, int n, double bpm)
{
    const double rate = isOn (flaSync) ? bpm / 60.0 / syncDivisionBeats (choice (flaDiv)) : (double) get (flaRate);
    const double inc = rate / sampleRate;
    const float depthT = get (flaDepth), fb = get (flaFeedback), mixT = get (flaMix);
    const float spread = get (flaSpread) * 0.5f;
    const float sr = (float) sampleRate;
    const float dMin = 0.0002f * sr;
    float* chans[2] = { L, R };

    for (int i = 0; i < n; ++i)
    {
        flaPhase += inc;
        if (flaPhase >= 1.0) flaPhase -= 1.0;
        flaMixS += (mixT - flaMixS) * smoothC;
        flaDepthS += (depthT - flaDepthS) * smoothC;
        const float dMax = dMin + flaDepthS * 0.007f * sr;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float ph = wrap ((float) flaPhase + (float) ch * spread);
            const float tri = 1.0f - std::abs (2.0f * ph - 1.0f) * 2.0f;          // -1..1
            const float t = tri * 0.5f + 0.5f;
            const float d = juce::jmax (2.0f, dMin * std::pow (dMax / dMin, t)); // sweeps evenly in pitch
            const float x = chans[ch][i];
            const float y = flaLine[ch].read (d);
            flaLine[ch].push (x + fastTanh (fb * y));
            chans[ch][i] = x + (y - x) * flaMixS;
        }
    }
}

//==============================================================================
void FxRack::phaser (float* L, float* R, int n)
{
    const double inc = (double) get (phaRate) / sampleRate;
    const float depth = get (phaDepth), freq = get (phaFreq), fb = get (phaFeedback), mixT = get (phaMix);
    const int stages = 2 * (juce::jlimit (0, 5, choice (phaStages)) + 1);
    const float spread = get (phaSpread) * 0.5f;
    const float maxF = (float) sampleRate * 0.45f;
    float* chans[2] = { L, R };

    for (int i = 0; i < n; ++i)
    {
        phaPhase += inc;
        if (phaPhase >= 1.0) phaPhase -= 1.0;
        if (phaCounter-- <= 0)
        {
            phaCounter = 7;
            for (int ch = 0; ch < 2; ++ch)
            {
                const float lfo = std::sin (twoPi * wrap ((float) phaPhase + (float) ch * spread));
                const float f = juce::jlimit (20.0f, maxF, freq * std::exp2 (depth * 2.5f * lfo));
                const float t = std::tan (pi * f / (float) sampleRate);
                phaCoef[ch] = (t - 1.0f) / (t + 1.0f);
            }
        }
        phaMixS += (mixT - phaMixS) * smoothC;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float a = phaCoef[ch];
            float x = chans[ch][i] + fb * phaLast[ch];
            for (int s = 0; s < stages; ++s)
            {
                const float y = a * x + phaState[ch][s];
                phaState[ch][s] = x - a * y;
                x = y;
            }
            phaLast[ch] = fastTanh (x);
            chans[ch][i] += (x - chans[ch][i]) * phaMixS;
        }
    }
}

//==============================================================================
void FxRack::chorus (float* L, float* R, int n)
{
    const double inc = (double) get (choRate) / sampleRate;
    const float depth = get (choDepth), centreT = get (choDelay), fb = get (choFeedback), mixT = get (choMix);
    const float hicut = get (choHicut);
    const bool filtered = hicut < 19500.0f;
    const float lpC = onePole (sampleRate, juce::jmin (hicut, (float) sampleRate * 0.45f));
    const float spread = get (choSpread) * 0.25f;
    const float msToS = (float) sampleRate * 0.001f;
    const float maxD = choLine[0].maxDelay() - 2.0f;
    if (choDelayS < 0.0f) choDelayS = centreT;
    float* chans[2] = { L, R };

    for (int i = 0; i < n; ++i)
    {
        choPhase += inc;
        if (choPhase >= 1.0) choPhase -= 1.0;
        choMixS += (mixT - choMixS) * smoothC;
        choDelayS += (centreT - choDelayS) * smoothC;
        const float amp = depth * juce::jmin (choDelayS * 0.8f, 8.0f);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float ph = (float) choPhase + (float) ch * spread;
            const float d1 = (choDelayS + amp * std::sin (twoPi * ph)) * msToS;
            const float d2 = (choDelayS * 1.6f + amp * std::sin (twoPi * (ph + 0.5f))) * msToS;
            const float x = chans[ch][i];
            float v = 0.5f * (choLine[ch].read (juce::jlimit (2.0f, maxD, d1)) + choLine[ch].read (juce::jlimit (2.0f, maxD, d2)));
            choLine[ch].push (x + fb * v);
            choLp[ch][0] += (v - choLp[ch][0]) * lpC;
            choLp[ch][1] += (choLp[ch][0] - choLp[ch][1]) * lpC;
            if (filtered) v = choLp[ch][1];
            chans[ch][i] = x * std::cos (choMixS * pi * 0.5f) + v * std::sin (choMixS * pi * 0.5f);
        }
    }
}

//==============================================================================
void FxRack::delay (float* L, float* R, int n, double bpm)
{
    const double seconds = isOn (dlySync) ? syncDivisionBeats (choice (dlyDiv)) * 60.0 / juce::jmax (20.0, bpm)
                                          : get (dlyTime) / 1000.0;
    const float maxD = dlyLine[0].maxDelay() - 2.0f;
    const float offset = get (dlyOffset);
    const float target[2] = { juce::jlimit (2.0f, maxD, (float) (seconds * sampleRate)),
                              juce::jlimit (2.0f, maxD, (float) (seconds * (1.0 + offset) * sampleRate)) };
    for (int ch = 0; ch < 2; ++ch)
        if (dlyTimeS[ch] <= 0.0f)
            dlyTimeS[ch] = target[ch];

    const float fbT = get (dlyFeedback), mixT = get (dlyMix);
    const bool ping = isOn (dlyPing);
    const float lpC = onePole (sampleRate, juce::jmin (get (dlyHicut), (float) sampleRate * 0.45f));
    const float hpC = onePole (sampleRate, get (dlyLowcut));

    for (int i = 0; i < n; ++i)
    {
        dlyMixS += (mixT - dlyMixS) * smoothC;
        dlyFbS += (fbT - dlyFbS) * smoothC;
        float out[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            dlyTimeS[ch] += (target[ch] - dlyTimeS[ch]) * 0.0005f;
            float y = dlyLine[ch].read (dlyTimeS[ch]);
            dlyLp[ch] += (y - dlyLp[ch]) * lpC;
            dlyHp[ch] += (dlyLp[ch] - dlyHp[ch]) * hpC;
            out[ch] = dlyLp[ch] - dlyHp[ch];
        }
        const float inL = L[i], inR = R[i];
        if (ping)
        {
            dlyLine[0].push ((inL + inR) * 0.5f + fastTanh (dlyFbS * out[1]));
            dlyLine[1].push (fastTanh (dlyFbS * out[0]));
        }
        else
        {
            dlyLine[0].push (inL + fastTanh (dlyFbS * out[0]));
            dlyLine[1].push (inR + fastTanh (dlyFbS * out[1]));
        }
        L[i] = inL * (1.0f - dlyMixS * 0.5f) + out[0] * dlyMixS;
        R[i] = inR * (1.0f - dlyMixS * 0.5f) + out[1] * dlyMixS;
    }
}

//==============================================================================
// Reverb: input diffusion into an eight-line feedback delay network with a
// Hadamard mixing matrix, damping in the loop and gently modulated lines.
void FxRack::reverb (float* L, float* R, int n)
{
    static constexpr float baseMs[kRevLines] = { 29.7f, 37.1f, 41.1f, 47.3f, 53.9f, 59.3f, 67.1f, 73.7f };
    static constexpr float diffMs[4] = { 1.7f, 3.1f, 4.9f, 7.3f };
    static constexpr float modeScale[3] = { 1.0f, 0.55f, 0.3f };     // Hall, Plate, Room
    static constexpr float diffScale[3] = { 1.0f, 1.4f, 0.6f };
    static constexpr float diffGain[3] = { 0.6f, 0.72f, 0.5f };
    static constexpr float wetScale = 0.55f;

    const int mode = juce::jlimit (0, 2, choice (revMode));
    const float sr = (float) sampleRate;
    const float sizeT = get (revSize);
    if (revSizeS < 0.0f) revSizeS = sizeT;
    const float sizeStart = revSizeS;
    revSizeS += (sizeT - revSizeS) * juce::jmin (1.0f, (float) n / (0.08f * sr));
    const float decay = juce::jmax (0.1f, get (revDecay));
    const float dampC = onePole (sampleRate, 20000.0f * std::pow (0.06f, get (revDamp)));
    const float width = get (revWidth), mixT = get (revMix);
    const int pre = (int) (get (revPre) * 0.001f * sr);
    const float maxLen = revLine[0].maxDelay() - 64.0f;

    float lenA[kRevLines], lenB[kRevLines], gain[kRevLines];
    for (int k = 0; k < kRevLines; ++k)
    {
        const float ms = baseMs[k] * modeScale[mode];
        lenA[k] = juce::jmin (maxLen, ms * (0.3f + 1.2f * sizeStart) * 0.001f * sr);
        lenB[k] = juce::jmin (maxLen, ms * (0.3f + 1.2f * revSizeS) * 0.001f * sr);
        gain[k] = std::pow (10.0f, -3.0f * lenB[k] / (sr * decay));
    }
    int diffLen[4];
    for (int j = 0; j < 4; ++j)
        diffLen[j] = juce::jlimit (2, (int) revDiff[0][j].maxDelay(), (int) (diffMs[j] * diffScale[mode] * 0.001f * sr));
    const float dg = diffGain[mode];

    const auto setSvf = [&] (dsp::Svf& s, float hz) { s.setCoefs (std::tan (pi * juce::jlimit (10.0f, sr * 0.45f, hz) / sr), 1.414f); };
    for (int ch = 0; ch < 2; ++ch)
    {
        setSvf (revHp[ch], get (revLowcut));
        setSvf (revLp[ch], get (revHicut));
    }

    const double modInc = 1.0 / sampleRate;
    const float modDepth = 0.00025f * sr;
    const float invN = 1.0f / (float) n;

    for (int i = 0; i < n; ++i)
    {
        revMixS += (mixT - revMixS) * smoothC;
        float in[2] = { L[i], R[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float x = in[ch];
            if (pre > 0)
            {
                revPreLine[ch].push (x);
                x = revPreLine[ch].readInt (pre);
            }
            for (int j = 0; j < 4; ++j)
            {
                const float v = revDiff[ch][j].readInt (diffLen[j]);
                const float w = x + dg * v;
                revDiff[ch][j].push (w);
                x = v - dg * w;
            }
            in[ch] = x;
        }

        revModPhase += modInc;
        if (revModPhase >= 1000.0) revModPhase -= 1000.0;
        const float t = (float) i * invN;
        float y[kRevLines];
        for (int k = 0; k < kRevLines; ++k)
        {
            float d = lenA[k] + (lenB[k] - lenA[k]) * t;
            if (k < 4)
                d += modDepth * (1.0f + std::sin (twoPi * (float) (revModPhase * (0.31 + 0.13 * k)) + (float) k));
            const float v = revLine[k].read (d);
            revDamping[k] += (v - revDamping[k]) * dampC;
            y[k] = revDamping[k];
        }

        // Fast Walsh-Hadamard transform: an energy-preserving mix of all eight lines.
        float h[kRevLines];
        std::copy (std::begin (y), std::end (y), std::begin (h));
        for (int len = 1; len < kRevLines; len <<= 1)
            for (int a = 0; a < kRevLines; a += len << 1)
                for (int b = a; b < a + len; ++b)
                {
                    const float u = h[b], v = h[b + len];
                    h[b] = u + v;
                    h[b + len] = u - v;
                }
        for (int k = 0; k < kRevLines; ++k)
            revLine[k].push (h[k] * 0.35355339f * gain[k] + in[k & 1] * 0.5f);

        float wl = (y[0] - y[2] + y[4] - y[6]) * wetScale;
        float wr = (y[1] - y[3] + y[5] - y[7]) * wetScale;
        const float mid = (wl + wr) * 0.5f, side = (wl - wr) * 0.5f * width;
        wl = mid + side;
        wr = mid - side;
        wl = revLp[0].process (revHp[0].process (wl).hp).lp;
        wr = revLp[1].process (revHp[1].process (wr).hp).lp;

        const float dryG = juce::jmin (1.0f, 2.0f * (1.0f - revMixS));
        const float wetG = juce::jmin (1.0f, 2.0f * revMixS);
        L[i] = L[i] * dryG + wl * wetG;
        R[i] = R[i] * dryG + wr * wetG;
    }
}

//==============================================================================
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

void FxRack::compressor (float* L, float* R, int n)
{
    const float thr = get (cmpThresh);
    const float ratio = get (cmpRatio);
    const float att = 1.0f - std::exp (-1.0f / (0.001f * get (cmpAttack) * (float) sampleRate));
    const float rel = 1.0f - std::exp (-1.0f / (0.001f * get (cmpRelease) * (float) sampleRate));
    const float up = get (cmpUpward);
    const float makeup = juce::Decibels::decibelsToGain (get (cmpGain));
    const float mix = get (cmpMix);
    const bool multi = isOn (cmpMulti);
    float* chans[2] = { L, R };

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* x = chans[ch];
        for (int i = 0; i < n; ++i)
        {
            float y;
            if (! multi)
                y = comp[0][ch].process (x[i], thr, ratio, att, rel, up) * makeup;
            else
            {
                // Three bands: low (<120 Hz), mid, high (>2.5 kHz), each compressed on its own.
                float lo2 = 0.0f, hi2 = 0.0f, lo1 = 0.0f, hi1 = 0.0f;
                highSplit.processSample (ch, x[i], lo2, hi2);
                lowSplit.processSample (ch, lo2, lo1, hi1);
                y = (comp[0][ch].process (lo1, thr, ratio, att, rel, up)
                     + comp[1][ch].process (hi1, thr, ratio, att, rel, up)
                     + comp[2][ch].process (hi2, thr, ratio, att, rel, up)) * makeup;
            }
            x[i] += (y - x[i]) * mix;
        }
    }
}

//==============================================================================
// Multiband: three-band upward and downward compression in the style of the
// classic "over the top" squash used all over EDM.
void FxRack::multiband (float* L, float* R, int n)
{
    const float depth = get (mbDepth), time = get (mbTime);
    const float up = get (mbUp), down = get (mbDown);
    const float bandDb[3] = { get (mbLow), get (mbMid), get (mbHigh) };
    const float outGain = juce::Decibels::decibelsToGain (get (mbGain));
    const float sr = (float) sampleRate;
    const float att = 1.0f - std::exp (-1.0f / ((0.001f + 0.02f * time) * sr));
    const float rel = 1.0f - std::exp (-1.0f / ((0.03f + 0.4f * time) * sr));
    static constexpr float downThr = -24.0f, upThr = -40.0f;
    static constexpr float downSlope = 1.0f - 1.0f / 30.0f, upSlope = 1.0f - 1.0f / 3.5f;

    for (int i = 0; i < n; ++i)
    {
        float band[2][3];
        const float in[2] = { L[i], R[i] };
        for (int ch = 0; ch < 2; ++ch)
        {
            float lo2 = 0.0f, hi2 = 0.0f, lo1 = 0.0f, hi1 = 0.0f;
            mbHighSplit.processSample (ch, in[ch], lo2, hi2);
            mbLowSplit.processSample (ch, lo2, lo1, hi1);
            band[ch][0] = lo1;
            band[ch][1] = hi1;
            band[ch][2] = mbAllpass.processSample (ch, hi2);   // keeps the bands in phase
        }
        float out[2] = { 0.0f, 0.0f };
        for (int b = 0; b < 3; ++b)
        {
            const float peak = juce::jmax (std::abs (band[0][b]), std::abs (band[1][b]));
            mbEnv[b] += (peak - mbEnv[b]) * (peak > mbEnv[b] ? att : rel);
            const float lvl = 20.0f * std::log10 (mbEnv[b] + 1.0e-9f);
            float gDb = 0.0f;
            if (lvl > downThr)
                gDb = (downThr - lvl) * downSlope * down;
            else if (lvl < upThr && lvl > -80.0f)
                gDb = juce::jmin (24.0f, (upThr - lvl) * upSlope) * up;
            // The fixed makeup keeps a typical synth part at about the same loudness.
            const float g = std::pow (10.0f, ((gDb + 8.0f * down) * depth + bandDb[b]) * 0.05f);
            out[0] += band[0][b] * g;
            out[1] += band[1][b] * g;
        }
        L[i] = out[0] * outGain;
        R[i] = out[1] * outGain;
    }
}

//==============================================================================
void FxRack::eq (float* L, float* R, int n)
{
    const float v[8] = { (float) choice (eqLowType), get (eqLowFreq), get (eqLowQ), get (eqLowGain),
                         (float) choice (eqHighType), get (eqHighFreq), get (eqHighQ), get (eqHighGain) };
    if (! std::equal (std::begin (v), std::end (v), std::begin (eqCache)))
    {
        std::copy (std::begin (v), std::end (v), std::begin (eqCache));
        static constexpr fx::Biquad::Kind lowKinds[] = { fx::Biquad::LowShelf, fx::Biquad::Peak, fx::Biquad::LowCut };
        static constexpr fx::Biquad::Kind highKinds[] = { fx::Biquad::HighShelf, fx::Biquad::Peak, fx::Biquad::HighCut };
        for (int ch = 0; ch < 2; ++ch)
        {
            eqLow[ch].design (lowKinds[juce::jlimit (0, 2, (int) v[0])], sampleRate, v[1], v[2], v[3]);
            eqHigh[ch].design (highKinds[juce::jlimit (0, 2, (int) v[4])], sampleRate, v[5], v[6], v[7]);
        }
    }
    float* chans[2] = { L, R };
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < n; ++i)
            chans[ch][i] = eqHigh[ch].process (eqLow[ch].process (chans[ch][i]));
}

//==============================================================================
void FxRack::filter (float* L, float* R, int n)
{
    const int type = juce::jlimit (0, 7, choice (filType));
    const float cutT = std::log2 (juce::jlimit (20.0f, 20000.0f, get (filCutoff)));
    const float res = get (filRes);
    const float k = juce::jmax (0.1f, 2.0f - 1.9f * res);
    const float resComp = 1.0f - 0.5f * res;   // keeps a screaming resonance in check
    const float drive = get (filDrive), stereo = get (filStereo), mixT = get (filMix);
    const float pre = 1.0f + drive * 8.0f;
    const float post = 1.0f / std::tanh (pre);
    const float maxF = (float) sampleRate * 0.45f;
    const bool twoStage = type == 1 || type == 3 || type == 5;
    if (filCutS < 0.0f) filCutS = cutT;
    float* chans[2] = { L, R };

    for (int i = 0; i < n; ++i)
    {
        filMixS += (mixT - filMixS) * smoothC;
        if ((i & 3) == 0)
        {
            filCutS += (cutT - filCutS) * juce::jmin (1.0f, smoothC * 4.0f);
            for (int ch = 0; ch < 2; ++ch)
            {
                const float f = juce::jmin (maxF, std::exp2 (filCutS + (ch == 0 ? -stereo : stereo)));
                const float g = std::tan (pi * f / (float) sampleRate);
                filSvf[ch][0].setCoefs (g, k);
                filSvf[ch][1].setCoefs (g, type == 5 ? k : 1.414f);
            }
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            const float dry = chans[ch][i];
            const float x = (drive > 0.0f ? std::tanh (dry * pre) * post : dry) * resComp;
            auto o = filSvf[ch][0].process (x);
            float y;
            switch (type)
            {
                case 0: case 1: y = o.lp; break;
                case 2: case 3: y = o.hp; break;
                case 4: case 5: y = o.bp * k; break;
                case 6:         y = o.lp + o.hp; break;
                default:        y = o.lp - o.hp; break;
            }
            if (twoStage)
            {
                const auto o2 = filSvf[ch][1].process (y);
                y = type == 1 ? o2.lp : type == 3 ? o2.hp : o2.bp * k;
            }
            chans[ch][i] = dry + (y - dry) * filMixS;
        }
    }
}
} // namespace cs
