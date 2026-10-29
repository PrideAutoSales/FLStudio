#include "Engine.h"
#include "Piano.h"
#include "GrandData.h"

namespace cs
{
namespace
{
    constexpr int kCtrl = 16; // samples between modulation updates

    inline double wrap01 (double x) noexcept { return x - std::floor (x); }

    inline float midiToHz (float note) noexcept
    {
        return 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
    }
}

//==============================================================================
struct Engine::Block
{
    struct OscP
    {
        std::atomic<float>* on; std::atomic<float>* octave; std::atomic<float>* semi; std::atomic<float>* unison;
        std::atomic<float>* width; std::atomic<float>* rand; std::atomic<float>* warp; std::atomic<float>* toFilter;
    };
    struct EnvP { std::atomic<float>* a; std::atomic<float>* h; std::atomic<float>* d; std::atomic<float>* s; std::atomic<float>* r; std::atomic<float>* c; };
    struct LfoP { std::atomic<float>* shape; std::atomic<float>* mode; std::atomic<float>* sync; std::atomic<float>* div;
                  std::atomic<float>* rise; std::atomic<float>* smooth; std::atomic<float>* phase; };
    struct SlotP { std::atomic<float>* src; std::atomic<float>* dst; std::atomic<float>* amount; std::atomic<float>* bipolar; };

    // Parameter handles, resolved once.
    OscP oscP[kNumOscs];
    EnvP envP[kNumEnvs];
    LfoP lfoP[kNumLfos];
    SlotP slotP[kNumModSlots];
    std::atomic<float>* macroP[kNumMacros];
    std::atomic<float>* subOnP; std::atomic<float>* subShapeP; std::atomic<float>* subOctP; std::atomic<float>* subToFilterP;
    std::atomic<float>* noiseOnP; std::atomic<float>* noiseTypeP; std::atomic<float>* noiseToFilterP;
    std::atomic<float>* fltOnP; std::atomic<float>* fltTypeP; std::atomic<float>* fltKeyP;
    std::atomic<float>* polyP; std::atomic<float>* monoP; std::atomic<float>* legatoP; std::atomic<float>* glideP;
    std::atomic<float>* bendP; std::atomic<float>* velP;
    std::atomic<float>* pnoOnP; std::atomic<float>* pnoModelP; std::atomic<float>* pnoToFilterP;
    std::atomic<float>* pnoDynP; std::atomic<float>* pnoSusP; std::atomic<float>* pnoDampP; std::atomic<float>* pnoStrP;
    std::atomic<float>* pnoBodyP; std::atomic<float>* pnoHamP; std::atomic<float>* pnoWidthP;
    std::atomic<float>* dstP[DstCount] {};
    juce::NormalisableRange<float> ranges[DstCount];

    // Snapshot for the current block.
    struct Osc { bool on; int octave, semi, unison, warp; float width, rand; bool toFilter; } osc[kNumOscs];
    bool subOn, subToFilter, noiseOn, noiseToFilter, fltOn;
    int subShape, subOctave, noiseType, fltType;
    float keytrack;
    dsp::Envelope::Settings env[kNumEnvs];
    struct Lfo { int shape, mode; bool sync; float rise, smooth, phase; double syncRate; } lfo[kNumLfos];
    float macros[kNumMacros];
    int poly; bool mono, legato; float glide; int bendRange; float velSens;
    bool pnoOn, pnoToFilter; piano::Settings piano;
    ModSlot slots[kNumModSlots]; int numSlots = 0;
    float baseNorm[DstCount] {}, baseReal[DstCount] {};
    int modulatedDst[DstCount] {}; int numModulated = 0;
    double bpm = 120.0;

    Block (juce::AudioProcessorValueTreeState& s)
    {
        auto p = [&] (const juce::String& pid)
        {
            auto* v = s.getRawParameterValue (pid);
            jassert (v != nullptr);
            return v;
        };

        for (int o = 0; o < kNumOscs; ++o)
            oscP[o] = { p (id::osc (o, "on")), p (id::osc (o, "octave")), p (id::osc (o, "semi")), p (id::osc (o, "unison")),
                        p (id::osc (o, "width")), p (id::osc (o, "rand")), p (id::osc (o, "warp")), p (id::osc (o, "toFilter")) };
        for (int e = 0; e < kNumEnvs; ++e)
            envP[e] = { p (id::env (e, "attack")), p (id::env (e, "hold")), p (id::env (e, "decay")),
                        p (id::env (e, "sustain")), p (id::env (e, "release")), p (id::env (e, "curve")) };
        for (int l = 0; l < kNumLfos; ++l)
            lfoP[l] = { p (id::lfo (l, "shape")), p (id::lfo (l, "mode")), p (id::lfo (l, "sync")), p (id::lfo (l, "div")),
                        p (id::lfo (l, "rise")), p (id::lfo (l, "smooth")), p (id::lfo (l, "phase")) };
        for (int i = 0; i < kNumModSlots; ++i)
            slotP[i] = { p (id::slot (i, "src")), p (id::slot (i, "dst")), p (id::slot (i, "amount")), p (id::slot (i, "bipolar")) };
        for (int m = 0; m < kNumMacros; ++m)
            macroP[m] = p (id::macro (m));

        subOnP = p ("sub_on"); subShapeP = p ("sub_shape"); subOctP = p ("sub_octave"); subToFilterP = p ("sub_toFilter");
        noiseOnP = p ("noise_on"); noiseTypeP = p ("noise_type"); noiseToFilterP = p ("noise_toFilter");
        fltOnP = p ("flt_on"); fltTypeP = p ("flt_type"); fltKeyP = p ("flt_keytrack");
        polyP = p ("poly"); monoP = p ("mono"); legatoP = p ("legato"); glideP = p ("glide");
        bendP = p ("bend_range"); velP = p ("vel_sens");
        pnoOnP = p ("pno_on"); pnoModelP = p ("pno_model"); pnoToFilterP = p ("pno_toFilter");
        pnoDynP = p ("pno_dynamics"); pnoSusP = p ("pno_sustain"); pnoDampP = p ("pno_damper"); pnoStrP = p ("pno_strings");
        pnoBodyP = p ("pno_body"); pnoHamP = p ("pno_hammer"); pnoWidthP = p ("pno_width");

        const auto& ids = modDestinationParamIDs();
        for (int d = 1; d < DstCount; ++d)
        {
            dstP[d] = p (ids[d]);
            if (auto* rp = s.getParameter (ids[d]))
                ranges[d] = rp->getNormalisableRange();
        }
    }

    void read (double newBpm)
    {
        bpm = newBpm;
        auto b = [] (std::atomic<float>* v) { return v->load (std::memory_order_relaxed) > 0.5f; };
        auto i = [] (std::atomic<float>* v) { return juce::roundToInt (v->load (std::memory_order_relaxed)); };
        auto f = [] (std::atomic<float>* v) { return v->load (std::memory_order_relaxed); };

        for (int o = 0; o < kNumOscs; ++o)
        {
            auto& P = oscP[o];
            osc[o] = { b (P.on), i (P.octave), i (P.semi), juce::jlimit (1, kMaxUnison, i (P.unison)), i (P.warp),
                       f (P.width), f (P.rand), b (P.toFilter) };
        }
        subOn = b (subOnP); subShape = i (subShapeP); subOctave = i (subOctP); subToFilter = b (subToFilterP);
        noiseOn = b (noiseOnP); noiseType = i (noiseTypeP); noiseToFilter = b (noiseToFilterP);
        fltOn = b (fltOnP); fltType = i (fltTypeP); keytrack = f (fltKeyP);

        for (int e = 0; e < kNumEnvs; ++e)
            env[e] = { f (envP[e].a), f (envP[e].h), f (envP[e].d), f (envP[e].s), f (envP[e].r), f (envP[e].c) };

        for (int l = 0; l < kNumLfos; ++l)
        {
            const auto& P = lfoP[l];
            lfo[l] = { i (P.shape), i (P.mode), b (P.sync), f (P.rise), f (P.smooth), f (P.phase),
                       bpm / 60.0 / syncDivisionBeats (i (P.div)) };
        }

        for (int m = 0; m < kNumMacros; ++m)
            macros[m] = f (macroP[m]);

        poly = juce::jlimit (1, kMaxVoices, i (polyP));
        mono = b (monoP); legato = b (legatoP); glide = f (glideP);
        bendRange = i (bendP); velSens = f (velP);
        pnoOn = b (pnoOnP); pnoToFilter = b (pnoToFilterP);
        piano.model = i (pnoModelP);
        piano.dynamics = f (pnoDynP); piano.sustain = f (pnoSusP); piano.damper = f (pnoDampP);
        piano.strings = f (pnoStrP); piano.body = f (pnoBodyP); piano.hammer = f (pnoHamP); piano.width = f (pnoWidthP);

        bool used[DstCount] {};
        numSlots = 0;
        for (int s = 0; s < kNumModSlots; ++s)
        {
            const int src = i (slotP[s].src), dst = i (slotP[s].dst);
            const float amt = f (slotP[s].amount);
            if (src <= 0 || dst <= 0 || dst >= DstCount || src >= kNumModSources || std::abs (amt) < 1.0e-6f)
                continue;
            slots[numSlots++] = { src, dst, amt, b (slotP[s].bipolar) };
            used[dst] = true;
        }

        numModulated = 0;
        for (int d = 1; d < DstCount; ++d)
        {
            baseReal[d] = f (dstP[d]);
            baseNorm[d] = ranges[d].convertTo0to1 (baseReal[d]);
            if (used[d])
                modulatedDst[numModulated++] = d;
        }
    }
};

//==============================================================================
class Engine::Voice
{
public:
    explicit Voice (Engine& e) : engine (e) {}

    void prepare (double sr)
    {
        sampleRate = (float) sr;
        for (auto& c : comb)
            c.prepare ((int) (sr / 18.0) + 8);
        for (int ch = 0; ch < 2; ++ch)
            noise[ch].seed ((juce::uint32) (std::rand() + ch * 7919));
        piano.prepare (sr);
        kill();
    }

    bool isPlaying() const noexcept { return playing; }

    void start (int newNote, float vel, juce::uint32 order, float glideFrom, bool retrigger)
    {
        const auto& B = *engine.block;
        const bool wasPlaying = playing;
        note = newNote;
        velocity = vel;
        startOrder = order;
        held = true;
        sustained = false;
        targetPitch = (float) newNote;
        pitch = (B.glide > 0.0f && glideFrom >= 0.0f) ? glideFrom : targetPitch;

        if (! retrigger && wasPlaying)
            return; // legato: keep envelopes and phases running

        playing = true;
        pianoStrike = true; // struck on the next control tick, once modulation is known
        randomValue = engine.rng.nextFloat();
        for (auto& e : env)
            e.noteOn (wasPlaying);

        for (int l = 0; l < kNumLfos; ++l)
        {
            auto& s = lfo[l];
            s.phase = B.lfo[l].mode == LfoFree ? engine.freeLfoPhase[(size_t) l] : 0.0;
            s.elapsed = 0.0f;
            s.done = false;
            s.rnd0 = engine.rng.nextFloat();
            s.rnd1 = engine.rng.nextFloat();
            if (! wasPlaying)
                s.value = s.smoothed = 0.0f;
        }

        for (int o = 0; o < kNumOscs; ++o)
        {
            const float rnd = B.osc[o].rand;
            for (int u = 0; u < kMaxUnison; ++u)
                phase[o][u] = wrap01 ((double) rnd * engine.rng.nextDouble());
            lastOut[o] = 0.0f;
        }
        subPhase = 0.0;

        if (! wasPlaying)
        {
            for (auto& ch : svf) for (auto& s : ch) s.reset();
            for (auto& ch : vowel) for (auto& s : ch) s.reset();
            for (auto& l : ladder) l.reset();
            for (auto& c : comb) c.reset();
            amp = 0.0f;
        }
        ctrlLeft = 0;
    }

    void release()
    {
        held = false;
        for (auto& e : env)
            e.noteOff();
        if (pianoActive)
            piano.release (engine.block->piano);
    }

    // The same key struck again: its strings are still ringing from the last strike,
    // and the hammer stops them as it strikes them anew.
    void chokePiano() noexcept
    {
        if (pianoActive)
            piano.choke();
    }

    void kill()
    {
        playing = false;
        held = sustained = false;
        note = -1;
        amp = 0.0f;
        for (auto& e : env)
            e.reset();
        piano.reset();
        pianoActive = pianoStrike = false;
    }

    void render (float* outL, float* outR, int numSamples)
    {
        int i = 0;
        while (i < numSamples && playing)
        {
            if (ctrlLeft <= 0)
            {
                controlTick();
                ctrlLeft = kCtrl;
            }
            const int todo = juce::jmin (numSamples - i, ctrlLeft);
            renderSamples (outL + i, outR + i, todo);
            ctrlLeft -= todo;
            i += todo;

            if (ending && ctrlLeft <= 0)
                kill();
        }
    }

    // Public so the engine can show it in the editor.
    int note = -1;
    float velocity = 0.0f;
    juce::uint32 startOrder = 0;
    bool held = false, sustained = false;
    float pitch = 60.0f, targetPitch = 60.0f;
    dsp::Envelope env[kNumEnvs];
    struct LfoState { double phase = 0.0; float value = 0.0f, smoothed = 0.0f, rnd0 = 0.0f, rnd1 = 0.0f, elapsed = 0.0f, shown = 0.0f; bool done = false; };
    LfoState lfo[kNumLfos];
    float effNorm[DstCount] {};
    float framePos[kNumOscs] {};

private:
    void tickLfo (int l, float dt)
    {
        const auto& B = *engine.block;
        const auto& P = B.lfo[l];
        auto& s = lfo[l];

        const double rate = P.sync ? P.syncRate : (double) eff[DstLfo1Rate + l];
        if (! s.done)
        {
            s.phase += rate * dt;
            if (s.phase >= 1.0)
            {
                if (P.mode == LfoEnvelope)
                {
                    s.phase = 0.99999;
                    s.done = true;
                }
                else
                {
                    s.phase = wrap01 (s.phase);
                    s.rnd0 = s.rnd1;
                    s.rnd1 = engine.rng.nextFloat();
                }
            }
        }

        const float p = (float) wrap01 (s.phase + P.phase);
        float u = 0.0f;
        switch (P.shape)
        {
            case LfoSine:     u = 0.5f + 0.5f * std::sin (dsp::twoPi * p); break;
            case LfoTriangle: u = 1.0f - 2.0f * std::abs (p - 0.5f); break;
            case LfoSawUp:    u = p; break;
            case LfoSawDown:  u = 1.0f - p; break;
            case LfoSquare:   u = p < 0.5f ? 1.0f : 0.0f; break;
            case LfoCustom:
            {
                const auto& t = engine.lfoTables[(size_t) l];
                const float x = p * (kLfoTableSize - 1);
                const int i0 = (int) x;
                const int i1 = juce::jmin (kLfoTableSize - 1, i0 + 1);
                const float a = t[(size_t) i0].load (std::memory_order_relaxed);
                const float b = t[(size_t) i1].load (std::memory_order_relaxed);
                u = a + (x - (float) i0) * (b - a);
                break;
            }
            case LfoSampleHold: u = s.rnd1; break;
            default:
            {
                const float t = 0.5f - 0.5f * std::cos (dsp::pi * (float) wrap01 (s.phase));
                u = s.rnd0 + (s.rnd1 - s.rnd0) * t;
                break;
            }
        }

        s.elapsed += dt;
        if (P.rise > 0.0f)
            u *= juce::jmin (1.0f, s.elapsed / P.rise);

        if (P.smooth > 0.0f)
        {
            const float c = 1.0f - std::exp (-dt / (P.smooth * P.smooth * 0.5f + 1.0e-4f));
            s.smoothed += (u - s.smoothed) * c;
            s.value = s.smoothed;
        }
        else
        {
            s.value = s.smoothed = u;
        }
        s.shown = p;
    }

    void controlTick()
    {
        auto& E = engine;
        const auto& B = *E.block;
        const float dt = (float) kCtrl / sampleRate;

        // Glide
        if (B.glide > 0.0f)
            pitch += (targetPitch - pitch) * (1.0f - std::exp (-dt / (B.glide * 0.3f)));
        else
            pitch = targetPitch;

        // Sources
        for (int e = 0; e < kNumEnvs; ++e)
            env[e].tick (B.env[e], dt);
        for (int l = 0; l < kNumLfos; ++l)
            tickLfo (l, dt);

        float src[kNumModSources] {};
        for (int e = 0; e < kNumEnvs; ++e) src[(int) ModSrc::Env1 + e] = env[e].getValue();
        for (int l = 0; l < kNumLfos; ++l) src[(int) ModSrc::Lfo1 + l] = lfo[l].value;
        src[(int) ModSrc::Velocity] = velocity;
        src[(int) ModSrc::NoteTrack] = juce::jlimit (-1.0f, 1.0f, ((float) note - 60.0f) / 48.0f);
        src[(int) ModSrc::ModWheel] = E.modWheel;
        src[(int) ModSrc::Aftertouch] = E.aftertouch;
        src[(int) ModSrc::PitchBend] = E.pitchBend;
        for (int m = 0; m < kNumMacros; ++m) src[(int) ModSrc::Macro1 + m] = B.macros[m];
        src[(int) ModSrc::NoteRandom] = randomValue;

        // Matrix
        std::copy (std::begin (B.baseReal), std::end (B.baseReal), std::begin (eff));
        std::copy (std::begin (B.baseNorm), std::end (B.baseNorm), std::begin (effNorm));
        if (B.numModulated > 0)
        {
            float acc[DstCount] {};
            for (int s = 0; s < B.numSlots; ++s)
            {
                const auto& S = B.slots[s];
                float v = src[S.src];
                if (S.bipolar && ! modSourceIsBipolar (S.src))
                    v = v * 2.0f - 1.0f;
                acc[S.dst] += S.amount * v;
            }
            for (int k = 0; k < B.numModulated; ++k)
            {
                const int d = B.modulatedDst[k];
                const float n = juce::jlimit (0.0f, 1.0f, B.baseNorm[d] + acc[d]);
                effNorm[d] = n;
                eff[d] = B.ranges[d].convertFrom0to1 (n);
            }
        }

        // Oscillators
        const float bend = E.pitchBend * (float) B.bendRange;
        for (int o = 0; o < kNumOscs; ++o)
        {
            const auto& O = B.osc[o];
            const auto* table = E.tables[o];
            oscOn[o] = O.on && table != nullptr;
            if (! oscOn[o])
                continue;

            const int base = o == 0 ? DstOscACoarse : DstOscBCoarse;
            const float coarse = eff[base + 0], fine = eff[base + 1], pos = eff[base + 2];
            const float detune = eff[base + 3], blend = eff[base + 4];
            warpAmt[o] = eff[base + 5];
            phaseOff[o] = eff[base + 6];
            const float pan = eff[base + 7], level = eff[base + 8];

            framePos[o] = pos * (float) (table->getNumFrames() - 1);
            const float semis = pitch + (float) (O.octave * 12 + O.semi) + coarse + fine * 0.01f + bend;
            const float freq = midiToHz (semis);

            float stretch = 1.0f;
            syncRatio[o] = 1.0f;
            switch (O.warp)
            {
                case WarpSync: syncRatio[o] = 1.0f + warpAmt[o] * 15.0f; stretch = syncRatio[o]; break;
                case WarpBendPlus: case WarpBendMinus: case WarpAsym: case WarpPwm: stretch = 1.0f + warpAmt[o] * 3.0f; break;
                case WarpMirror: stretch = 2.0f; break;
                case WarpFm: stretch = 1.0f + warpAmt[o] * 4.0f; break;
                default: break;
            }

            const int n = O.unison;
            unison[o] = n;
            const float spread = detune * detune * 1.2f;
            float gainSq = 0.0f;
            for (int u = 0; u < n; ++u)
            {
                const float off = n == 1 ? 0.0f : (2.0f * (float) u / (float) (n - 1) - 1.0f);
                const double f = freq * std::exp2 (off * spread / 12.0f);
                inc[o][u] = juce::jmin (0.49, f / sampleRate);
                mip[o][u] = Wavetable::levelForIncrement (inc[o][u] * stretch);
                const bool centre = n <= 2 || std::abs (off) <= 1.0f / (float) (n - 1) + 1.0e-4f;
                const float w = centre ? 1.0f : blend;
                const float p = juce::jlimit (-1.0f, 1.0f, off * O.width + pan);
                const float th = (p + 1.0f) * dsp::pi * 0.25f;
                gl[o][u] = std::cos (th) * w;
                gr[o][u] = std::sin (th) * w;
                gainSq += w * w;
            }
            const float norm = level * juce::MathConstants<float>::sqrt2 / std::sqrt (juce::jmax (1.0e-4f, gainSq));
            for (int u = 0; u < n; ++u)
            {
                gl[o][u] *= norm;
                gr[o][u] *= norm;
            }
            warpMode[o] = O.warp;
            toFilter[o] = O.toFilter;
        }

        // Sub and noise
        if (B.subOn)
        {
            subInc = midiToHz (pitch + (float) (B.subOctave * 12) + bend) / sampleRate;
            const float th = (eff[DstSubPan] + 1.0f) * dsp::pi * 0.25f;
            subGl = std::cos (th) * eff[DstSubLevel] * juce::MathConstants<float>::sqrt2;
            subGr = std::sin (th) * eff[DstSubLevel] * juce::MathConstants<float>::sqrt2;
        }
        if (B.noiseOn)
        {
            const float th = (eff[DstNoisePan] + 1.0f) * dsp::pi * 0.25f;
            noiseGl = std::cos (th) * eff[DstNoiseLevel] * juce::MathConstants<float>::sqrt2 * 0.5f;
            noiseGr = std::sin (th) * eff[DstNoiseLevel] * juce::MathConstants<float>::sqrt2 * 0.5f;
        }

        // Piano
        if (B.pnoOn)
        {
            if (pianoStrike)
            {
                auto settings = B.piano;
                settings.tone = eff[DstPnoTone];
                piano.strike (pitch, velocity, settings);
                pianoPitch = pitch;
                pianoActive = true;
                pianoStrike = false;
                if (! held && ! sustained)
                    piano.release (settings);
            }
            if (pianoActive)
                piano.retune (pitch - pianoPitch + bend);
            pianoGain = eff[DstPnoLevel] * 1.6f;
        }
        else
        {
            pianoActive = pianoStrike = false;
        }

        // Filter
        if (B.fltOn)
        {
            float fc = eff[DstFltCutoff] * std::exp2 (((float) note - 60.0f) / 12.0f * B.keytrack);
            fc = juce::jlimit (20.0f, sampleRate * 0.45f, fc);
            const float g = std::tan (dsp::pi * fc / sampleRate);
            const float res = eff[DstFltRes];
            const float k = 1.414f - 1.384f * res;
            for (int ch = 0; ch < 2; ++ch)
            {
                svf[ch][0].setCoefs (g, k);
                svf[ch][1].setCoefs (g, juce::jmax (0.4f, 1.414f - 1.0f * res));
                ladder[ch].setCoefs (g, res);
            }
            combDelay = sampleRate / fc;
            combFb = 0.2f + res * 0.75f;
            const float drive = eff[DstFltDrive];
            driveGain = 1.0f + drive * drive * 12.0f;
            driveOn = drive > 0.001f;
            fltMix = eff[DstFltMix];
            fltK = k;

            if (B.fltType == FltVowel)
            {
                static const float formants[5][3] = { { 800, 1150, 2900 }, { 400, 1700, 2600 }, { 300, 2200, 3000 },
                                                      { 450, 800, 2830 }, { 325, 700, 2530 } };
                const float pos = juce::jlimit (0.0f, 3.999f, (float) (std::log2 (eff[DstFltCutoff] / 20.0f) / std::log2 (1000.0f)) * 4.0f);
                const int v0 = (int) pos;
                const float t = pos - (float) v0;
                const float vk = 0.25f - 0.2f * res;
                for (int f = 0; f < 3; ++f)
                {
                    const float hz = juce::jmin (sampleRate * 0.45f, formants[v0][f] * (1.0f - t) + formants[v0 + 1][f] * t);
                    const float vg = std::tan (dsp::pi * hz / sampleRate);
                    for (int ch = 0; ch < 2; ++ch)
                        vowel[ch][f].setCoefs (vg, vk);
                }
                vowelK = vk;
            }
        }

        // Amplitude: Env 1 x velocity x master volume, ramped across the block
        const float velGain = 1.0f - B.velSens * (1.0f - velocity);
        const float target = env[0].getValue() * velGain * eff[DstMasterVol] * 0.4f; // headroom for chords
        ampStep = (target - amp) / (float) kCtrl;
        ending = ! env[0].isActive();

        // A piano-only sound ends when its strings fall silent, whatever the envelope says.
        if (pianoActive && ! held && ! anyOscillatorOn() && piano.isSilent())
            ending = true;
    }

    bool anyOscillatorOn() const noexcept
    {
        const auto& B = *engine.block;
        return oscOn[0] || oscOn[1] || B.subOn || B.noiseOn;
    }

    float readWarped (int o, double ph, int level, float other) const noexcept
    {
        const auto& T = *engine.tables[o];
        const float fp = framePos[o];
        const float a = warpAmt[o];
        switch (warpMode[o])
        {
            case WarpOff: return T.read (fp, level, ph);
            case WarpSync: return T.read (fp, level, wrap01 (ph * syncRatio[o]));
            case WarpBendPlus:
            case WarpBendMinus:
            {
                const double e = warpMode[o] == WarpBendPlus ? 1.0 + a * 5.0 : 1.0 / (1.0 + a * 5.0);
                const double p = ph < 0.5 ? 0.5 * std::pow (2.0 * ph, e) : 1.0 - 0.5 * std::pow (2.0 - 2.0 * ph, e);
                return T.read (fp, level, juce::jlimit (0.0, 0.999999, p));
            }
            case WarpAsym:
            {
                const double b = 0.5 - 0.45 * a;
                const double p = ph < b ? 0.5 * ph / b : 0.5 + 0.5 * (ph - b) / (1.0 - b);
                return T.read (fp, level, juce::jlimit (0.0, 0.999999, p));
            }
            case WarpPwm:
            {
                const double w = 1.0 - 0.95 * a;
                return ph >= w ? 0.0f : T.read (fp, level, ph / w);
            }
            case WarpMirror:
            {
                const double m = ph < 0.5 ? 2.0 * ph : 2.0 - 2.0 * ph;
                const float s0 = T.read (fp, level, ph);
                const float s1 = T.read (fp, level, juce::jlimit (0.0, 0.999999, m));
                return s0 + a * (s1 - s0);
            }
            case WarpFlip:
            {
                const float s = T.read (fp, level, ph);
                return ph > 1.0 - 0.5 * a ? -s : s;
            }
            case WarpQuantize:
            {
                const float s = T.read (fp, level, ph);
                if (a <= 0.0f) return s;
                const float steps = std::exp2 (6.0f - 5.0f * a);
                return std::round (s * steps) / steps;
            }
            case WarpFm: return T.read (fp, level, wrap01 (ph + a * 1.5 * other));
            case WarpAm: return T.read (fp, level, ph) * ((1.0f - a) + a * (0.5f + 0.5f * other));
            case WarpRm: return T.read (fp, level, ph) * ((1.0f - a) + a * other);
            default: return T.read (fp, level, ph);
        }
    }

    float filter (int ch, float x) noexcept
    {
        const float in = x;
        if (driveOn)
            x = dsp::fastTanh (x * driveGain);

        float y = x;
        switch (engine.block->fltType)
        {
            case FltLp12:  y = svf[ch][0].process (x).lp; break;
            case FltLp24:  y = svf[ch][1].process (svf[ch][0].process (x).lp).lp; break;
            case FltLadder: y = ladder[ch].process (x); break;
            case FltHp12:  y = svf[ch][0].process (x).hp; break;
            case FltHp24:  y = svf[ch][1].process (svf[ch][0].process (x).hp).hp; break;
            case FltBp12:  y = svf[ch][0].process (x).bp * fltK; break;
            case FltBp24:  y = svf[ch][1].process (svf[ch][0].process (x).bp * fltK).bp * svf[ch][1].k; break;
            case FltNotch: { const auto o = svf[ch][0].process (x); y = o.lp + o.hp; break; }
            case FltPeak:  { const auto o = svf[ch][0].process (x); y = x + o.bp * fltK * 3.0f; break; }
            case FltCombPlus:  y = comb[ch].process (x, combDelay, combFb); break;
            case FltCombMinus: y = comb[ch].process (x, combDelay, -combFb); break;
            case FltVowel:
            {
                static const float gains[3] = { 1.0f, 0.7f, 0.45f };
                y = 0.0f;
                for (int f = 0; f < 3; ++f)
                    y += vowel[ch][f].process (x).bp * vowelK * gains[f] * 2.5f;
                break;
            }
            default: break;
        }
        return in + (y - in) * fltMix;
    }

    void renderSamples (float* outL, float* outR, int n) noexcept
    {
        const auto& B = *engine.block;
        float pl[kCtrl] {}, pr[kCtrl] {};
        const bool pianoOn = pianoActive && B.pnoOn;
        if (pianoOn)
            piano.render (pl, pr, n, pianoGain);

        for (int i = 0; i < n; ++i)
        {
            float fl = 0.0f, fr = 0.0f, dl = 0.0f, dr = 0.0f;
            if (pianoOn)
            {
                if (B.pnoToFilter) { fl += pl[i]; fr += pr[i]; } else { dl += pl[i]; dr += pr[i]; }
            }

            for (int o = 0; o < kNumOscs; ++o)
            {
                if (! oscOn[o])
                    continue;
                const float other = lastOut[1 - o];
                float sl = 0.0f, sr = 0.0f, raw = 0.0f;
                for (int u = 0; u < unison[o]; ++u)
                {
                    const double ph = wrap01 (phase[o][u] + (double) phaseOff[o]);
                    const float s = readWarped (o, ph, mip[o][u], other);
                    raw += s;
                    sl += s * gl[o][u];
                    sr += s * gr[o][u];
                    phase[o][u] += inc[o][u];
                    if (phase[o][u] >= 1.0)
                        phase[o][u] -= 1.0;
                }
                lastOut[o] = raw / (float) unison[o];
                if (toFilter[o]) { fl += sl; fr += sr; } else { dl += sl; dr += sr; }
            }

            if (B.subOn)
            {
                const float s = dsp::subOsc (B.subShape, (float) subPhase, (float) subInc);
                subPhase += subInc;
                if (subPhase >= 1.0) subPhase -= 1.0;
                if (B.subToFilter) { fl += s * subGl; fr += s * subGr; } else { dl += s * subGl; dr += s * subGr; }
            }

            if (B.noiseOn)
            {
                const float color = eff[DstNoiseColor];
                const float nl = noise[0].next (B.noiseType, color) * noiseGl;
                const float nr = noise[1].next (B.noiseType, color) * noiseGr;
                if (B.noiseToFilter) { fl += nl; fr += nr; } else { dl += nl; dr += nr; }
            }

            if (B.fltOn)
            {
                fl = filter (0, fl);
                fr = filter (1, fr);
            }

            amp += ampStep;
            outL[i] += (fl + dl) * amp;
            outR[i] += (fr + dr) * amp;
        }
    }

    Engine& engine;
    float sampleRate = 48000.0f;
    bool playing = false, ending = false;
    int ctrlLeft = 0;
    float randomValue = 0.0f;
    float eff[DstCount] {};

    double phase[kNumOscs][kMaxUnison] {};
    double inc[kNumOscs][kMaxUnison] {};
    int mip[kNumOscs][kMaxUnison] {};
    float gl[kNumOscs][kMaxUnison] {}, gr[kNumOscs][kMaxUnison] {};
    int unison[kNumOscs] { 1, 1 };
    bool oscOn[kNumOscs] {}, toFilter[kNumOscs] {};
    int warpMode[kNumOscs] {};
    float warpAmt[kNumOscs] {}, phaseOff[kNumOscs] {}, syncRatio[kNumOscs] { 1.0f, 1.0f };
    float lastOut[kNumOscs] {};

    double subPhase = 0.0, subInc = 0.0;
    float subGl = 0, subGr = 0, noiseGl = 0, noiseGr = 0;
    dsp::Noise noise[2];

    dsp::Svf svf[2][2];
    dsp::Svf vowel[2][3];
    dsp::Ladder ladder[2];
    dsp::Comb comb[2];
    float combDelay = 100.0f, combFb = 0.0f, driveGain = 1.0f, fltMix = 1.0f, fltK = 1.4f, vowelK = 0.2f;
    bool driveOn = false;

    float amp = 0.0f, ampStep = 0.0f;

    piano::Voice piano;
    bool pianoActive = false, pianoStrike = false;
    float pianoGain = 1.0f, pianoPitch = 60.0f;
};

//==============================================================================
Engine::Engine (juce::AudioProcessorValueTreeState& s, LiveState& l, LfoTables& t)
    : apvts (s), live (l), lfoTables (t), block (std::make_unique<Block> (s))
{
    for (int i = 0; i < kMaxVoices; ++i)
        voices.push_back (std::make_unique<Voice> (*this));
    block->read (120.0);
}

Engine::~Engine() = default;

void Engine::prepare (double sr, int)
{
    sampleRate = sr;
    for (auto& v : voices)
        v->prepare (sr);
    reset();

    // Unpack the measured grand now, so its first note doesn't do it on the audio thread.
    piano::GrandData::get();
}

void Engine::reset()
{
    for (auto& v : voices)
        v->kill();
    monoStack.clear();
    sustainDown = false;
    pitchBend = 0.0f;
    lastNote = -1.0f;
}

void Engine::allNotesOff()
{
    for (auto& v : voices)
        if (v->isPlaying())
            v->release();
    monoStack.clear();
}

void Engine::readParameters (const Transport& t)
{
    block->read (t.bpm);
}

void Engine::render (juce::AudioBuffer<float>& out, const juce::MidiBuffer& midi, const Transport& transport)
{
    readParameters (transport);
    const int numSamples = out.getNumSamples();
    auto* L = out.getWritePointer (0);
    auto* R = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        if (end <= pos)
            return;
        for (auto& v : voices)
            if (v->isPlaying())
                v->render (L + pos, R + pos, end - pos);
        pos = end;
    };

    for (const auto meta : midi)
    {
        renderTo (juce::jlimit (0, numSamples, meta.samplePosition));
        handleMidi (meta.getMessage());
    }
    renderTo (numSamples);

    // Free-running LFO clocks, shared by voices in Free mode.
    const double secs = numSamples / sampleRate;
    for (int l = 0; l < kNumLfos; ++l)
    {
        const auto& P = block->lfo[l];
        if (P.sync && transport.playing)
            freeLfoPhase[(size_t) l] = wrap01 (transport.ppq / syncDivisionBeats (juce::roundToInt (block->lfoP[l].div->load())));
        else
        {
            const double rate = P.sync ? P.syncRate : (double) block->baseReal[DstLfo1Rate + l];
            freeLfoPhase[(size_t) l] = wrap01 (freeLfoPhase[(size_t) l] + rate * secs);
        }
    }

    publishLiveState();
}

void Engine::publishLiveState()
{
    Voice* shown = nullptr;
    int active = 0;
    for (auto& v : voices)
    {
        if (! v->isPlaying())
            continue;
        ++active;
        if (shown == nullptr || v->startOrder > shown->startOrder)
            shown = v.get();
    }
    live.activeVoices.store (active, std::memory_order_relaxed);
    live.anyVoice.store (shown != nullptr, std::memory_order_relaxed);

    if (shown != nullptr)
    {
        for (int d = 1; d < DstCount; ++d)
            live.dstNorm[(size_t) d].store (shown->effNorm[d], std::memory_order_relaxed);
    }
    else
    {
        // Nothing playing: apply only the sources that exist without a note (macros and
        // the wheels), so a macro on an effect still works while the synth is silent.
        const auto& B = *block;
        float acc[DstCount] {};
        for (int s = 0; s < B.numSlots; ++s)
        {
            const auto& S = B.slots[s];
            float v;
            if (S.src >= (int) ModSrc::Macro1 && S.src <= (int) ModSrc::Macro4) v = B.macros[S.src - (int) ModSrc::Macro1];
            else if (S.src == (int) ModSrc::ModWheel) v = modWheel;
            else if (S.src == (int) ModSrc::Aftertouch) v = aftertouch;
            else if (S.src == (int) ModSrc::PitchBend) v = pitchBend;
            else continue;
            if (S.bipolar && ! modSourceIsBipolar (S.src))
                v = v * 2.0f - 1.0f;
            acc[S.dst] += S.amount * v;
        }
        for (int d = 1; d < DstCount; ++d)
            live.dstNorm[(size_t) d].store (juce::jlimit (0.0f, 1.0f, B.baseNorm[d] + acc[d]), std::memory_order_relaxed);
    }

    for (int e = 0; e < kNumEnvs; ++e)
    {
        live.envValue[(size_t) e].store (shown != nullptr ? shown->env[e].getValue() : 0.0f, std::memory_order_relaxed);
        live.envStage[(size_t) e].store (shown != nullptr ? (int) shown->env[e].getStage() : 0, std::memory_order_relaxed);
    }
    for (int l = 0; l < kNumLfos; ++l)
    {
        const float ph = shown != nullptr ? shown->lfo[l].shown
                                          : (float) wrap01 (freeLfoPhase[(size_t) l] + block->lfo[l].phase);
        live.lfoPhase[(size_t) l].store (ph, std::memory_order_relaxed);
        live.lfoValue[(size_t) l].store (shown != nullptr ? shown->lfo[l].value : 0.0f, std::memory_order_relaxed);
    }
    for (int o = 0; o < kNumOscs; ++o)
    {
        const int d = o == 0 ? DstOscAPos : DstOscBPos;
        live.wtPos[(size_t) o].store (shown != nullptr ? shown->effNorm[d] : block->baseNorm[d], std::memory_order_relaxed);
    }
}

//==============================================================================
void Engine::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        pitchBend = juce::jlimit (-1.0f, 1.0f, (float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        aftertouch = (float) m.getChannelPressureValue() / 127.0f;
    else if (m.isAftertouch())
        aftertouch = (float) m.getAfterTouchValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        allNotesOff();
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1)
            modWheel = (float) m.getControllerValue() / 127.0f;
        else if (m.getControllerNumber() == 64)
        {
            sustainDown = m.getControllerValue() >= 64;
            if (! sustainDown)
                for (auto& v : voices)
                    if (v->isPlaying() && v->sustained)
                    {
                        v->sustained = false;
                        v->release();
                    }
        }
    }
}

Engine::Voice* Engine::findVoiceFor (int)
{
    const int limit = block->poly;
    Voice* best = nullptr;
    for (int i = 0; i < limit; ++i)
        if (! voices[(size_t) i]->isPlaying())
            return voices[(size_t) i].get();

    // Steal: prefer released voices, then the oldest.
    for (int i = 0; i < limit; ++i)
    {
        auto* v = voices[(size_t) i].get();
        const bool vReleased = ! v->held && ! v->sustained;
        if (best == nullptr)
            best = v;
        else
        {
            const bool bReleased = ! best->held && ! best->sustained;
            if ((vReleased && ! bReleased) || (vReleased == bReleased && v->startOrder < best->startOrder))
                best = v;
        }
    }
    return best;
}

void Engine::noteOn (int note, float velocity)
{
    const auto& B = *block;
    if (B.mono)
    {
        monoStack.removeAllInstancesOf (note);
        monoStack.add (note);
        auto& v = *voices[0];
        const bool legatoMove = B.legato && v.isPlaying() && v.held;
        v.start (note, velocity, ++noteCounter, v.isPlaying() ? v.pitch : lastNote, ! legatoMove);
        for (size_t i = 1; i < voices.size(); ++i)
            if (voices[i]->isPlaying())
                voices[i]->release();
    }
    else
    {
        if (B.pnoOn)
            for (auto& other : voices)
                if (other->isPlaying() && other->note == note)
                    other->chokePiano();
        auto* v = findVoiceFor (note);
        if (v != nullptr)
            v->start (note, velocity, ++noteCounter, lastNote, true);
    }
    lastNote = (float) note;
}

void Engine::noteOff (int note)
{
    const auto& B = *block;
    if (B.mono)
    {
        monoStack.removeAllInstancesOf (note);
        auto& v = *voices[0];
        if (! v.isPlaying() || v.note != note)
            return;

        if (! monoStack.isEmpty())
        {
            const int back = monoStack.getLast();
            v.start (back, v.velocity, v.startOrder, v.pitch, ! B.legato);
            lastNote = (float) back;
        }
        else if (sustainDown)
            v.sustained = true, v.held = false;
        else
            v.release();
        return;
    }

    for (auto& v : voices)
    {
        if (v->isPlaying() && v->note == note && v->held)
        {
            if (sustainDown)
            {
                v->held = false;
                v->sustained = true;
            }
            else
                v->release();
        }
    }
}
} // namespace cs
