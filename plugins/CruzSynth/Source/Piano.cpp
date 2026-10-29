#include "Piano.h"
#include "GrandData.h"
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace cs::piano
{
juce::StringArray modelNames()
{
    return { "Concert Grand", "Bright Grand", "Upright", "Felt Piano", "Tine EP", "Reed EP" };
}

juce::String modelDescription (int m)
{
    switch (m)
    {
        case ConcertGrand: return "Measured from a real grand: each note's overtones, beating and decay at 16 strengths";
        case BrightGrand:  return "A modelled grand voiced bright for pop and dance mixes: harder hammers, more presence";
        case Upright:      return "Studio upright: shorter strings, a woody body and more beating between strings";
        case Felt:         return "Felt-muted piano: soft hammers, a close and intimate tone with plenty of action noise";
        case TineEP:       return "Tine electric piano: bell-like attack that barks when played hard";
        case ReedEP:       return "Reed electric piano: hollow, reedy tone with a gritty bite on hard notes";
        default: return {};
    }
}

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

    inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
    inline float clamp01 (float x) noexcept { return juce::jlimit (0.0f, 1.0f, x); }
    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    // Per-model voicing. Values are relative to the concert grand.
    struct ModelData
    {
        bool electric;
        float strikePos;   // hammer position along the string (fraction of its length)
        float hammerHz;    // hammer brightness at full velocity around middle C
        float tilt;        // spectral slope exponent
        float radiation;   // soundboard radiation corner, Hz
        float inharm;      // inharmonicity multiplier
        float prompt;      // prompt (first-stage) decay multiplier
        float after;       // aftersound decay multiplier
        float afterLevel;  // aftersound level offset, dB
        float detune;      // string detune multiplier
        float body;        // soundboard colour multiplier
        float knock;       // hammer noise level
        float knockHz;     // hammer noise centre around middle C
        float thump;       // soundboard knock level
        float damper;      // damper time multiplier
        float stretch;     // octave stretch multiplier
        float gain;
    };

    const ModelData& modelData (int m)
    {
        static const ModelData data[NumModels] = {
            // elec  pos     hammer   tilt   rad     B     prm    aft    aLvl   det   body  knock knHz     thump damp  str   gain
            { false, 0.122f, 5200.0f, 1.19f, 97.0f, 1.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1400.0f, 1.0f, 1.0f, 1.0f, 1.0f },    // Concert Grand
            { false, 0.118f, 6800.0f, 1.00f, 100.0f, 1.0f, 1.05f, 1.15f, -1.0f, 0.9f, 0.8f, 1.1f, 1800.0f, 0.8f, 0.9f, 1.0f, 0.85f }, // Bright Grand
            { false, 0.130f, 4300.0f, 1.12f, 130.0f, 1.9f, 1.3f, 1.55f, 1.0f, 1.6f, 1.7f, 1.3f, 1300.0f, 1.5f, 0.8f, 1.4f, 1.05f },  // Upright
            { false, 0.125f, 1500.0f, 1.35f, 105.0f, 1.6f, 0.8f, 1.35f, 0.0f, 1.0f, 1.3f, 0.5f, 650.0f, 1.8f, 1.3f, 1.2f, 1.3f },     // Felt Piano
            { true, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.35f, 3200.0f, 0.0f, 0.8f, 0.0f, 1.0f },            // Tine EP
            { true, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.0f, 0.3f, 2400.0f, 0.0f, 0.7f, 0.0f, 1.0f },             // Reed EP
        };
        return data[juce::jlimit (0, NumModels - 1, m)];
    }

    // The soundboard's response: a fixed pattern of bumps and dips, so every key has
    // its own slightly uneven colour the way a real instrument does.
    float bodyResponseDb (float f) noexcept
    {
        static const float bumps[][2] = {
            { 92.0f, 3.5f }, { 118.0f, -2.5f }, { 151.0f, 4.0f }, { 197.0f, -3.0f }, { 236.0f, 2.0f },
            { 292.0f, 3.0f }, { 361.0f, -4.0f }, { 433.0f, 2.5f }, { 520.0f, -1.5f }, { 610.0f, 3.5f },
            { 742.0f, -3.5f }, { 880.0f, 2.0f }, { 1043.0f, -2.0f }, { 1250.0f, 3.0f }, { 1520.0f, -3.0f },
            { 1860.0f, 2.0f }, { 2240.0f, -2.5f }, { 2750.0f, 2.5f }, { 3350.0f, -3.0f }, { 4100.0f, 1.5f },
            { 5000.0f, -2.0f }, { 6200.0f, 1.5f }
        };
        float db = 0.0f;
        const float lf = std::log2 (juce::jmax (20.0f, f));
        for (auto& b : bumps)
        {
            const float d = (lf - std::log2 (b[0])) / 0.13f;
            db += b[1] * std::exp (-d * d);
        }
        return db;
    }

    float hashNote (int note, int salt) noexcept
    {
        juce::uint32 h = (juce::uint32) (note * 2654435761u) ^ (juce::uint32) (salt * 40503u);
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        return (float) (h & 0xffffu) / 65535.0f;
    }

    void panGains (float p, float& l, float& r) noexcept
    {
        const float th = (juce::jlimit (-1.0f, 1.0f, p) + 1.0f) * kPi * 0.25f;
        l = std::cos (th) * juce::MathConstants<float>::sqrt2;
        r = std::sin (th) * juce::MathConstants<float>::sqrt2;
    }
}

//==============================================================================
void Voice::prepare (double sampleRate)
{
    sr = (float) sampleRate;
    reset();
}

void Voice::add (float f, float amp, float decayDbPerSec, float panL, float panR) noexcept
{
    if (count >= kMax || f <= 10.0f || f >= sr * 0.46f || amp < 1.0e-6f)
        return;
    const int i = count++;
    freq[i] = f;
    radius[i] = std::pow (10.0f, -decayDbPerSec / (20.0f * sr));
    const float w = kTwoPi * f / sr;
    cr[i] = radius[i] * std::cos (w);
    ci[i] = radius[i] * std::sin (w);
    // Start at zero displacement, moving: the output (imaginary part) begins at 0
    // and rises, so a strike never clicks.
    re[i] = 1.0f;
    im[i] = 0.0f;
    gl[i] = amp * panL;
    gr[i] = amp * panR;
    gli[i] = gri[i] = 0.0f;
}

void Voice::refreshRotations() noexcept
{
    const float ratio = std::exp2 (bendSemis / 12.0f);
    for (int i = 0; i < count; ++i)
    {
        const float w = kTwoPi * juce::jmin (freq[i] * ratio, sr * 0.48f) / sr;
        cr[i] = radius[i] * std::cos (w);
        ci[i] = radius[i] * std::sin (w);
    }
}

void Voice::retune (float semis)
{
    if (std::abs (semis - bendSemis) < 0.005f)
        return;
    bendSemis = semis;
    refreshRotations();
}

void Voice::strike (float pitch, float velocity, const Settings& s)
{
    const auto& M = modelData (s.model);
    const GrandData* grand = s.model == ConcertGrand ? GrandData::get() : nullptr;
    if ((grand != nullptr) != measured)
        count = 0; // the other engine's resonators can't be carried over
    electric = M.electric;
    note = pitch;
    // Anything still ringing in this voice (a stolen voice) fades out over a few
    // milliseconds instead of stopping dead, which would click.
    count = juce::jmin (count, 100);
    for (int i = 0; i < count; ++i)
        radius[i] = std::pow (10.0f, -60.0f / (0.02f * 20.0f * sr));
    refreshRotations();
    bendSemis = 0.0f;
    residual = nullptr;
    overtoneTable = nullptr;
    measured = grand != nullptr;
    if (grand != nullptr)
    {
        strikeGrand (*grand, pitch, velocity, s);
        return;
    }
    const float v = clamp01 (velocity);
    const float n = pitch;
    const int key = juce::roundToInt (pitch);

    float baseL, baseR;
    const float notePan = juce::jlimit (-1.0f, 1.0f, (n - 64.0f) / 40.0f) * s.width * 0.75f;
    panGains (notePan, baseL, baseR);

    // Loudness: a gentle curve from the dynamics setting, and the treble a little quieter.
    const float velExp = (0.3f + 1.2f * s.dynamics) * (1.0f - 0.4f * clamp01 ((n - 72.0f) / 24.0f));
    const float velGain = 0.03f + 0.97f * std::pow (v, velExp);
    const float keyGain = dbToGain (n > 60.0f ? -0.1f * (n - 60.0f) : -0.1f * (60.0f - n));
    const float sustainMul = std::exp2 ((0.5f - s.sustain) * 3.0f);
    const float maxF = juce::jmin (sr * 0.45f, 18000.0f);

    const float detuneCents = (0.3f + 25.0f * std::pow (s.strings, 2.2f)) * M.detune
                              * (0.6f + 0.8f * hashNote (key, 1));

    if (! electric)
    {
        // Pitch: the octave stretch of a tuned grand, then string inharmonicity.
        const float stretchCents = (n > 69.0f ? 0.012f * (n - 69.0f) * (n - 69.0f)
                                              : -0.008f * (69.0f - n) * (69.0f - n)) * M.stretch;
        const float f0 = 440.0f * std::exp2 ((n - 69.0f + stretchCents * 0.01f) / 12.0f);
        float logB = n <= 64.0f ? -3.52f : -3.52f + (n - 64.0f) * 0.026f;
        if (n < 33.0f) logB += 0.25f * (33.0f - n) / 12.0f;
        const float B = std::pow (10.0f, logB) * M.inharm;

        // Hammer: harder and brighter with velocity, the tone knob and higher keys.
        const float hammerHz = juce::jmax (f0 * 1.5f, M.hammerHz * std::pow (juce::jmax (0.05f, v), 0.74f) * std::exp2 (s.tone * 1.5f));
        const float strike = M.strikePos + 0.012f * (hashNote (key, 2) - 0.5f) + 0.0006f * (n - 60.0f);
        const float bodyAmt = s.body * 2.0f * M.body;
        // Short treble strings sound almost pure: their overtones fall away faster.
        const float trebleTilt = juce::jmax (0.0f, n - 66.0f) / 14.0f * (1.0f + juce::jmax (0.0f, n - 84.0f) / 16.0f);

        // Decay curves (dB per second), fitted to the recording.
        static const float promptCurve[][2] = { { 33.0f, 7.0f }, { 48.0f, 26.0f }, { 62.0f, 26.0f }, { 72.0f, 18.0f },
                                                { 80.0f, 20.0f }, { 84.0f, 32.0f }, { 93.0f, 42.0f }, { 96.0f, 52.0f }, { 108.0f, 70.0f } };
        float promptBase = promptCurve[0][1];
        for (int c = 1; c < 9; ++c)
            if (n >= promptCurve[c - 1][0])
                promptBase = lerp (promptCurve[c - 1][1], promptCurve[c][1],
                                   clamp01 ((n - promptCurve[c - 1][0]) / (promptCurve[c][0] - promptCurve[c - 1][0])));
        const float afterDb = (n < 36.0f ? lerp (-8.0f, -10.0f, clamp01 ((n - 21.0f) / 15.0f))
                              : n < 48.0f ? lerp (-10.0f, -18.0f, (n - 36.0f) / 12.0f)
                              : n < 60.0f ? lerp (-18.0f, -22.0f, (n - 48.0f) / 12.0f)
                              : lerp (-22.0f, -30.0f, clamp01 ((n - 72.0f) / 24.0f))) + M.afterLevel;
        const float afterAmp = dbToGain (afterDb);
        const float unisonNote = n < 29.0f ? 0.3f : 1.0f; // the lowest keys have one string

        struct Partial { float f, a, d1, d2; };
        Partial parts[96];
        int np = 0;
        float energy = 0.0f;
        for (int k = 1; k <= 90 && np < 96; ++k)
        {
            const float fk = (float) k * f0 * std::sqrt (1.0f + B * (float) (k * k));
            if (fk >= maxF)
                break;
            const float comb = juce::jmax (0.035f, std::abs (std::sin (kPi * (float) k * strike)));
            const float tilt = std::pow ((float) k, -(M.tilt + trebleTilt));
            const float rad = 1.0f / std::sqrt (1.0f + std::pow (M.radiation / fk, 7.5f));
            const float body = dbToGain (bodyResponseDb (fk) * bodyAmt);
            const float shape = comb * tilt * rad * body;
            energy += shape * shape;

            const float x = fk / hammerHz;
            const float hammer = 1.0f / ((1.0f + x * x) * (1.0f + x * x));
            const float fkK = fk * 0.001f;
            const float d2 = (0.5f + 3.8f * std::pow (fkK, 0.7f)) * M.after * sustainMul;
            const float d1 = (promptBase + 3.0f * fkK) * M.prompt * std::sqrt (sustainMul);
            parts[np++] = { fk, shape * hammer, d1, juce::jmin (d1 * 0.8f, d2) };
        }

        const float norm = (energy > 0.0f ? 1.0f / std::sqrt (energy) : 1.0f) * velGain * keyGain * M.gain * 0.5f;
        const float spread = s.width * 0.35f;
        for (int i = 0; i < np; ++i)
        {
            const auto& P = parts[i];
            const float a = P.a * norm;
            const float side = (i & 1) ? spread : -spread;
            float l1, r1, l2, r2;
            panGains (notePan + side, l1, r1);
            panGains (notePan - side, l2, r2);
            // Two resonators per partial: the strings moving together decay fast (prompt sound),
            // the out-of-phase motion lingers (aftersound). Their small detune makes the beating.
            add (P.f, a * (1.0f - afterAmp), P.d1, l1, r1);
            if (i < 40)
                {
                // Each partial beats at its own rate, as the strings' overtones never line up exactly.
                const float r = hashNote (key * 131 + i, 3);
                const float cents = detuneCents * unisonNote * (0.4f + 1.2f * r) * ((i % 3) == 2 ? -1.0f : 1.0f);
                add (P.f * std::exp2 (cents / 1200.0f), a * afterAmp, P.d2, l2, r2);
            }
        }

        stringCount = count;

        // Soundboard knock from the hammer blow, strongest in the bass.
        const float thumpAmt = M.thump * (0.3f + 1.4f * s.hammer) * s.body * 2.0f
                               * (n < 40.0f ? 1.4f : n > 84.0f ? 0.5f : lerp (1.4f, 0.5f, (n - 40.0f) / 44.0f))
                               * std::pow (v, 1.6f) * velGain * keyGain * 0.035f;
        static const float boardHz[] = { 58.0f, 83.0f, 121.0f, 167.0f, 236.0f, 330.0f, 452.0f };
        for (int b = 0; b < 7; ++b)
            add (boardHz[b] * (0.97f + 0.06f * hashNote (b, 7)), thumpAmt * (b < 3 ? 1.0f : 0.6f), 260.0f + 60.0f * (float) b, baseL, baseR);

        // Higher soundboard modes ring briefly after a strike; they matter most in the treble,
        // where the strings themselves are short and pure.
        const float ring = M.thump * s.body * 2.0f * (0.2f + 0.8f * clamp01 ((n - 60.0f) / 36.0f))
                           * std::pow (v, 1.4f) * velGain * keyGain * 0.02f;
        static const float ringHz[] = { 610.0f, 790.0f, 1040.0f, 1330.0f, 1720.0f, 2210.0f };
        for (int b = 0; b < 6; ++b)
        {
            float rl, rr;
            panGains (notePan + ((b & 1) ? 0.3f : -0.3f) * s.width, rl, rr);
            add (ringHz[b] * (0.95f + 0.1f * hashNote (key, 20 + b)), ring * (1.0f - 0.08f * (float) b), 70.0f + 12.0f * (float) b, rl, rr);
        }

        noiseDecay = std::exp (-1.0f / ((s.model == Felt ? 0.007f : 0.004f) * (1.0f + 4.0f * clamp01 ((n - 72.0f) / 24.0f)) * sr));
    }
    else
    {
        const float f0 = 440.0f * std::exp2 ((n - 69.0f) / 12.0f);
        const float tone = std::exp2 (s.tone);
        const float sustainTime = sustainMul;
        if (s.model == TineEP)
        {
            const float lowKeys = clamp01 ((72.0f - n) / 36.0f);
            const float d2 = (0.9f + 2.4f * f0 * 0.001f + (1.0f - lowKeys) * 1.5f) * sustainTime;
            const float d1 = 7.0f + 0.006f * f0;
            const float a = velGain * keyGain * 1.3f;
            const float det = std::exp2 (detuneCents * 0.25f / 1200.0f);
            add (f0, a * 0.55f, d1, 1.0f, 1.0f);
            add (f0 * det, a * 0.45f, d2, 1.0f, 1.0f);
            add (f0 * 2.0f, a * 0.06f, d1 * 1.6f, 1.0f, 1.0f);
            // The tine's own bell overtones: bright on hard hits, gone in a moment.
            const float bell = a * std::pow (v, 1.3f) * tone;
            add (f0 * 7.1f, bell * 0.22f, 55.0f + f0 * 0.01f, 1.0f, 1.0f);
            add (f0 * 19.8f, bell * 0.08f * v, 140.0f, 1.0f, 1.0f);
            pickupDrive = 0.15f + 0.9f * v * v * tone;
            pickupAsym = 0.35f;
            pickupIn = a;
        }
        else
        {
            const float hammerHz = f0 * (2.5f + 7.0f * std::pow (v, 1.5f)) * tone;
            const float a = velGain * keyGain * 1.25f;
            float energy = 0.0f;
            struct P { float f, a; } parts[32];
            int np = 0;
            for (int k = 1; k <= 30; ++k)
            {
                const float fk = f0 * (float) k;
                if (fk >= maxF) break;
                const float x = fk / hammerHz;
                const float shape = std::pow ((float) k, -1.1f) * ((k & 1) ? 1.0f : 0.5f);
                energy += shape * shape;
                parts[np++] = { fk, shape / (1.0f + x * x) };
            }
            const float norm = energy > 0.0f ? 1.0f / std::sqrt (energy) : 1.0f;
            for (int i = 0; i < np; ++i)
            {
                const float fkK = parts[i].f * 0.001f;
                add (parts[i].f, parts[i].a * norm * a * 0.7f, (12.0f + 9.0f * fkK), 1.0f, 1.0f);
                add (parts[i].f, parts[i].a * norm * a * 0.3f, (2.5f + 5.0f * fkK) * sustainTime, 1.0f, 1.0f);
            }
            pickupDrive = 0.3f + 1.6f * v * v * tone;
            pickupAsym = 0.5f;
            pickupIn = a;
        }
        pickupNorm = pickupIn / pickupDrive;
        pickupIn = pickupDrive / juce::jmax (1.0e-6f, pickupIn);
        panGains (notePan * 0.6f, pickupL, pickupR);
        noiseDecay = std::exp (-1.0f / (0.0015f * sr));
    }

    // Hammer and action noise.
    const float knockHz = juce::jlimit (400.0f, 3000.0f, M.knockHz * std::exp2 ((n - 60.0f) / 18.0f));
    bpG = std::tan (kPi * juce::jmin (knockHz, sr * 0.45f) / sr);
    bpK = 1.6f;
    lpC = 1.0f - std::exp (-kTwoPi * 7000.0f / sr);
    bpS1 = bpS2 = 0.0f;
    noiseEnv = M.knock * s.hammer * 2.0f * std::pow (v, 1.8f) * velGain * keyGain * 0.2f
               * (1.0f + 0.8f * clamp01 ((n - 72.0f) / 24.0f));
    panGains (notePan, noiseL, noiseR);
    seed = 0x9e3779b9u ^ (juce::uint32) (key * 7919);
    cullCounter = 0;
    silentRun = 0;
}

namespace
{
    // Where a MIDI velocity falls among the recorded strikes, 0 (softest) to 1
    // (hardest). Velocity 64 lands on the middle recordings, as on the real piano.
    float velocityToStrength (float v) noexcept
    {
        return std::pow (clamp01 (v), 1.15f);
    }

    constexpr float kGrandLevel = 1.0f;
}

void Voice::strikeGrand (const GrandData& D, float pitch, float velocity, const Settings& s)
{
    const float v = clamp01 (velocity);
    const int key = D.nearestKey (pitch);
    const float ratio = std::exp2 ((pitch - (float) D.keyNotes[(size_t) key]) / 12.0f);
    const int layers = D.numLayers;

    // The velocity picks the recorded strike, which sets the tone; the level follows
    // the velocity smoothly between recordings. Tone plays a harder or softer
    // strike's sound at the same level, like re-voicing the hammers.
    const float strength = velocityToStrength (v) * (float) (layers - 1);
    const int layer = juce::jlimit (0, layers - 1, juce::roundToInt (strength + s.tone * 4.0f));
    const auto& R = D.record (key, layer);
    // Dynamics: 0.5 keeps the real piano's range, lower squeezes it, higher widens it.
    const float top = D.levelAt (key, (float) (layers - 1));
    const float targetDb = top + (D.levelAt (key, strength) - top) * (0.4f + 1.2f * s.dynamics);
    const float gain = dbToGain (targetDb - R.loudness) * kGrandLevel;

    const float sustainMul = std::exp2 ((0.5f - s.sustain) * 2.0f);
    const float detune = s.strings / 0.3f;   // 0.3 is the piano as recorded
    const float side = s.width / 0.7f;       // 0.7 is the recording's own width
    const float maxF = juce::jmin (sr * 0.45f, 20000.0f);

    const auto* modes = D.modes.data() + R.firstMode;
    for (int m = 0; m < R.numModes && count < kMax - 8; ++m)
    {
        const auto& md = modes[m];
        const float f = (md.centre + md.offset * detune) * ratio;
        if (f <= 10.0f || f >= maxF)
            continue;
        // Only the strings' ringing follows Sustain; the fast attack modes keep their shape.
        const float decay = md.decay < 15.0f ? md.decay * sustainMul : md.decay;
        const int i = count++;
        freq[i] = f;
        radius[i] = std::exp (-decay / sr);
        const float w = kTwoPi * f / sr;
        cr[i] = radius[i] * std::cos (w);
        ci[i] = radius[i] * std::sin (w);
        re[i] = 1.0f;
        im[i] = 0.0f;
        const float midRe = 0.5f * (md.lRe + md.rRe), midIm = 0.5f * (md.lIm + md.rIm);
        const float sideRe = 0.5f * (md.lRe - md.rRe) * side, sideIm = 0.5f * (md.lIm - md.rIm) * side;
        gl[i] = (midRe + sideRe) * gain;
        gli[i] = (midIm + sideIm) * gain;
        gr[i] = (midRe - sideRe) * gain;
        gri[i] = (midIm - sideIm) * gain;
    }
    stringCount = count;
    padModes();

    // The modes swell in along the measured curve while the recorded hammer
    // attack plays; Body sets its low part (the knock through the soundboard),
    // Hammer its high part (felt on steel).
    attackCurve = R.attack;
    attackEnd = R.attackTime * D.sampleRate;
    attackPos = 0.0;
    residual = R.residualLength > 1 ? D.residual.data() + 2 * (size_t) R.firstResidual : nullptr;
    residualLength = R.residualLength;
    residualPos = 0.0;
    residualStep = (double) ratio * D.sampleRate / sr;
    residualLowGain = gain * 2.0f * s.body;
    residualHighGain = gain * 2.0f * s.hammer;
    residualSplit = 1.0f - std::exp (-kTwoPi * 500.0f / sr);
    residualLowL = residualLowR = 0.0f;

    overtoneTable = D.overtones.data() + R.firstOvertone;
    numOvertones = R.numOvertones;
    overtoneRatio = ratio;
    overtoneGain = gain;

    strikeLevel = dbToGain (targetDb);
    noiseEnv = 0.0f;
    seed = 0x9e3779b9u ^ (juce::uint32) (juce::roundToInt (pitch) * 7919);
    cullCounter = 0;
    silentRun = 0;
}

void Voice::padModes() noexcept
{
    // The renderer runs eight modes at a time; silent ones fill the last group.
    for (int i = count; i < ((count + 7) & ~7) && i < kMax; ++i)
        re[i] = im[i] = cr[i] = ci[i] = gl[i] = gr[i] = gli[i] = gri[i] = 0.0f;
}

void Voice::renderGrand (float* bufL, float* bufR, int n) noexcept
{
    using V = juce::dsp::SIMDRegister<float>;
    static_assert (V::SIMDNumElements == 4, "the grand's renderer works on four modes per register");

    for (int i = 0; i < count; i += 8)
    {
        V xa = V::fromRawArray (re + i), xb = V::fromRawArray (re + i + 4);
        V ya = V::fromRawArray (im + i), yb = V::fromRawArray (im + i + 4);
        const V ca = V::fromRawArray (cr + i), cb = V::fromRawArray (cr + i + 4);
        const V sa = V::fromRawArray (ci + i), sb = V::fromRawArray (ci + i + 4);
        const V lra = V::fromRawArray (gl + i), lrb = V::fromRawArray (gl + i + 4);
        const V lia = V::fromRawArray (gli + i), lib = V::fromRawArray (gli + i + 4);
        const V rra = V::fromRawArray (gr + i), rrb = V::fromRawArray (gr + i + 4);
        const V ria = V::fromRawArray (gri + i), rib = V::fromRawArray (gri + i + 4);
        for (int k = 0; k < n; ++k)
        {
            // Output is the real part of gain * state; then the state turns and decays.
            bufL[k] += (xa * lra - ya * lia + xb * lrb - yb * lib).sum();
            bufR[k] += (xa * rra - ya * ria + xb * rrb - yb * rib).sum();
            const V nxa = xa * ca - ya * sa, nxb = xb * cb - yb * sb;
            ya = xa * sa + ya * ca;
            yb = xb * sb + yb * cb;
            xa = nxa;
            xb = nxb;
        }
        xa.copyToRawArray (re + i); xb.copyToRawArray (re + i + 4);
        ya.copyToRawArray (im + i); yb.copyToRawArray (im + i + 4);
    }

    if (attackPos < attackEnd)
    {
        const float scale = (float) (GrandData::kGainPoints - 1) / attackEnd;
        for (int k = 0; k < n && attackPos < attackEnd; ++k)
        {
            const float pos = (float) attackPos * scale;
            const int a = juce::jmin ((int) pos, GrandData::kGainPoints - 2);
            const float g = lerp (attackCurve[a], attackCurve[a + 1], pos - (float) a);
            bufL[k] *= g;
            bufR[k] *= g;
            attackPos += residualStep;
        }
    }

    if (residual != nullptr)
    {
        float lowL = residualLowL, lowR = residualLowR;
        for (int k = 0; k < n; ++k)
        {
            const int idx = (int) residualPos;
            if (idx + 1 >= residualLength)
            {
                residual = nullptr;
                break;
            }
            const float t = (float) (residualPos - (double) idx);
            const float* p = residual + 2 * idx;
            const float inL = lerp (p[0], p[2], t), inR = lerp (p[1], p[3], t);
            lowL += (inL - lowL) * residualSplit;
            lowR += (inR - lowR) * residualSplit;
            bufL[k] += lowL * residualLowGain + (inL - lowL) * residualHighGain;
            bufR[k] += lowR * residualLowGain + (inR - lowR) * residualHighGain;
            residualPos += residualStep;
        }
        residualLowL = lowL;
        residualLowR = lowR;
    }
}

void Voice::addDecay (float dbPerSec, float dbPerSecPerKHz) noexcept
{
    for (int i = 0; i < count; ++i)
        radius[i] *= std::pow (10.0f, -(dbPerSec + dbPerSecPerKHz * freq[i] * 0.001f) / (20.0f * sr));
    refreshRotations();
    padModes();
}

void Voice::choke() noexcept
{
    // About a tenth of a second to fall silent: quick, but not a click.
    addDecay (600.0f, 0.0f);
    residual = nullptr;
}

int Voice::snapshot (float* freqs, float* levels, int maxCount) const noexcept
{
    if (measured && overtoneTable != nullptr)
    {
        const auto* o = static_cast<const GrandData::Overtone*> (overtoneTable);
        const int n = juce::jmin (numOvertones, maxCount);
        for (int i = 0; i < n; ++i)
        {
            freqs[i] = o[i].freq * overtoneRatio;
            levels[i] = dbToGain (o[i].level) * overtoneGain;
        }
        return n;
    }
    const int n = juce::jmin (electric ? count : stringCount, maxCount);
    for (int i = 0; i < n; ++i)
    {
        freqs[i] = freq[i];
        levels[i] = (std::abs (gl[i]) + std::abs (gr[i])) * 0.5f;
    }
    return n;
}

void Voice::release (const Settings& s)
{
    if (measured)
    {
        // The key and damper landing: a soft, low thud. The recorded grand's release
        // noises sit mostly between 60 and 250 Hz and fall 20 dB in about 0.1 s.
        bpG = std::tan (kPi * juce::jmin (140.0f, sr * 0.45f) / sr);
        bpK = 1.3f;
        lpC = 1.0f - std::exp (-kTwoPi * 1500.0f / sr);
        bpS1 = bpS2 = noiseLpState = noiseLpState2 = 0.0f;
        noiseEnv = strikeLevel * dbToGain (-30.0f) * s.hammer * 2.0f * (note < 89.0f ? 1.0f : 0.3f);
        noiseDecay = std::exp (-1.0f / (0.04f * sr));
        panGains (juce::jlimit (-1.0f, 1.0f, (note - 64.0f) / 40.0f) * s.width * 0.5f, noiseL, noiseR);

        // The top keys have no dampers.
        if (note < 89.0f)
        {
            const float time = (0.1f + 0.45f * clamp01 ((76.0f - note) / 44.0f)) * std::exp2 ((s.damper - 0.5f) * 3.0f);
            addDecay (60.0f / time, 60.0f / time * 0.25f);
        }
        return;
    }

    const auto& M = modelData (s.model);
    // The top keys have no dampers on a real piano.
    if (! electric && note >= 89.0f)
        return;

    const float time = (electric ? 0.09f : 0.12f + 0.4f * clamp01 ((76.0f - note) / 40.0f))
                       * M.damper * std::exp2 ((s.damper - 0.5f) * 3.0f);
    for (int i = 0; i < count; ++i)
    {
        const float dbPerSec = 60.0f / time * (1.0f + freq[i] / 4000.0f);
        radius[i] *= std::pow (10.0f, -dbPerSec / (20.0f * sr));
    }
    refreshRotations();
}

void Voice::cull() noexcept
{
    for (int i = 0; i < count;)
    {
        const float mag2 = (re[i] * re[i] + im[i] * im[i]) * (gl[i] * gl[i] + gr[i] * gr[i] + gli[i] * gli[i] + gri[i] * gri[i]);
        if (mag2 < 1.0e-12f)
        {
            const int last = --count;
            re[i] = re[last]; im[i] = im[last]; cr[i] = cr[last]; ci[i] = ci[last];
            gl[i] = gl[last]; gr[i] = gr[last]; freq[i] = freq[last]; radius[i] = radius[last];
            gli[i] = gli[last]; gri[i] = gri[last];
        }
        else
            ++i;
    }
    padModes();
}

void Voice::render (float* l, float* r, int n, float gain) noexcept
{
    while (n > 0)
    {
        const int todo = juce::jmin (n, 64);
        float bufL[64] {}, bufR[64] {};

        if (measured)
            renderGrand (bufL, bufR, todo);
        else if (! electric)
        {
            for (int i = 0; i < count; ++i)
            {
                float x = re[i], y = im[i];
                const float c = cr[i], s = ci[i], al = gl[i], ar = gr[i];
                for (int k = 0; k < todo; ++k)
                {
                    const float nx = x * c - y * s;
                    y = x * s + y * c;
                    x = nx;
                    bufL[k] += y * al;
                    bufR[k] += y * ar;
                }
                re[i] = x; im[i] = y;
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                float x = re[i], y = im[i];
                const float c = cr[i], s = ci[i], a = gl[i];
                for (int k = 0; k < todo; ++k)
                {
                    const float nx = x * c - y * s;
                    y = x * s + y * c;
                    x = nx;
                    bufL[k] += y * a;
                }
                re[i] = x; im[i] = y;
            }
            // Pickup: the tine or reed swinging past the magnet gives asymmetric
            // soft distortion that grows with how hard the key was hit.
            for (int k = 0; k < todo; ++k)
            {
                const float m = bufL[k] * pickupIn;
                const float shaped = m + pickupAsym * m * m;
                const float y = shaped / (1.0f + std::abs (shaped)) * pickupNorm;
                dcState += (y - dcState) * 0.0015f; // slow DC tracker for the asymmetric term
                const float out = y - dcState;
                bufL[k] = out * pickupL;
                bufR[k] = out * pickupR;
            }
        }

        if (noiseEnv > 1.0e-5f)
        {
            float s1 = bpS1, s2 = bpS2, lpS = noiseLpState, lpS2 = noiseLpState2;
            const float g = bpG, k2 = bpK;
            const float a1 = 1.0f / (1.0f + g * (g + k2));
            for (int k = 0; k < todo; ++k)
            {
                seed = seed * 1664525u + 1013904223u;
                const float w = (float) (seed >> 8) * (2.0f / 16777216.0f) - 1.0f;
                // State-variable band-pass.
                const float hp = (w - (g + k2) * s1 - s2) * a1;
                const float bp = g * hp + s1;
                s1 = g * hp + bp;
                const float lp = g * bp + s2;
                s2 = g * bp + lp;
                lpS += (bp - lpS) * lpC;
                lpS2 += (lpS - lpS2) * lpC;
                const float out = lpS2 * noiseEnv;
                noiseEnv *= noiseDecay;
                bufL[k] += out * noiseL;
                bufR[k] += out * noiseR;
            }
            bpS1 = s1; bpS2 = s2; noiseLpState = lpS; noiseLpState2 = lpS2;
        }

        for (int k = 0; k < todo; ++k)
        {
            l[k] += bufL[k] * gain;
            r[k] += bufR[k] * gain;
        }

        cullCounter += todo;
        if (cullCounter >= 512)
        {
            cullCounter = 0;
            cull();
        }

        l += todo; r += todo; n -= todo;
    }
}
} // namespace cs::piano
