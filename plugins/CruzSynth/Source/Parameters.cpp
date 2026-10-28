#include "Parameters.h"

namespace cs
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;
using Attr = juce::AudioParameterFloatAttributes;

//==============================================================================
juce::StringArray modSourceNames()
{
    return { "None", "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
             "Velocity", "Note", "Mod Wheel", "Aftertouch", "Pitch Bend",
             "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random" };
}

juce::String modSourceShortName (int src)
{
    static const char* names[] = { "-", "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "LFO3", "LFO4",
                                   "VEL", "NOTE", "MW", "AT", "PB", "M1", "M2", "M3", "M4", "RND" };
    return juce::isPositiveAndBelow (src, kNumModSources) ? names[src] : "-";
}

juce::Colour modSourceColour (int src)
{
    switch ((ModSrc) src)
    {
        case ModSrc::Env1: case ModSrc::Env2: case ModSrc::Env3:  return juce::Colour (0xffffb347);
        case ModSrc::Lfo1: case ModSrc::Lfo2: case ModSrc::Lfo3: case ModSrc::Lfo4: return juce::Colour (0xff57e3c4);
        case ModSrc::Macro1: case ModSrc::Macro2: case ModSrc::Macro3: case ModSrc::Macro4: return juce::Colour (0xffd38bff);
        default: return juce::Colour (0xff8fb8ff);
    }
}

bool modSourceIsBipolar (int src)
{
    return src == (int) ModSrc::NoteTrack || src == (int) ModSrc::PitchBend;
}

//==============================================================================
namespace id
{
    juce::String osc (int o, const char* name) { return juce::String (o == 0 ? "oscA_" : "oscB_") + name; }
    juce::String env (int e, const char* name) { return "env" + juce::String (e + 1) + "_" + name; }
    juce::String lfo (int l, const char* name) { return "lfo" + juce::String (l + 1) + "_" + name; }
    juce::String macro (int m)                 { return "macro" + juce::String (m + 1); }
    juce::String slot (int s, const char* name) { return "mod" + juce::String (s + 1) + "_" + name; }
}

const juce::StringArray& modDestinationParamIDs()
{
    static const juce::StringArray ids = []
    {
        juce::StringArray a { "" };
        for (int o = 0; o < kNumOscs; ++o)
            for (auto* n : { "coarse", "fine", "pos", "detune", "blend", "warpAmt", "phase", "pan", "level" })
                a.add (id::osc (o, n));
        a.addArray ({ "sub_level", "sub_pan", "noise_level", "noise_pan", "noise_color",
                      "flt_cutoff", "flt_res", "flt_drive", "flt_mix" });
        for (int l = 0; l < kNumLfos; ++l)
            a.add (id::lfo (l, "rate"));
        a.add ("master_vol");
        a.addArray ({ "fx_hyp_detune", "fx_hyp_mix", "fx_hyp_wide",
                      "fx_dist_drive", "fx_dist_ffreq", "fx_dist_mix",
                      "fx_fla_feedback", "fx_fla_mix",
                      "fx_pha_freq", "fx_pha_feedback", "fx_pha_mix",
                      "fx_cho_depth", "fx_cho_mix",
                      "fx_dly_feedback", "fx_dly_mix",
                      "fx_mb_depth",
                      "fx_eq_lowgain", "fx_eq_highgain",
                      "fx_fil_cutoff", "fx_fil_res", "fx_fil_drive", "fx_fil_mix",
                      "fx_rev_size", "fx_rev_decay", "fx_rev_mix" });
        jassert (a.size() == DstCount);
        return a;
    }();
    return ids;
}

juce::StringArray modDestinationNames()
{
    juce::StringArray a { "None" };
    for (auto* o : { "Osc A", "Osc B" })
        for (auto* n : { "Pitch", "Fine", "WT Pos", "Detune", "Blend", "Warp", "Phase", "Pan", "Level" })
            a.add (juce::String (o) + " " + n);
    a.addArray ({ "Sub Level", "Sub Pan", "Noise Level", "Noise Pan", "Noise Color",
                  "Filter Cutoff", "Filter Res", "Filter Drive", "Filter Mix",
                  "LFO 1 Rate", "LFO 2 Rate", "LFO 3 Rate", "LFO 4 Rate", "Master Volume",
                  "FX Hyper Detune", "FX Hyper Mix", "FX Widen Mix",
                  "FX Dist Drive", "FX Dist Filter", "FX Dist Mix",
                  "FX Flanger Feedback", "FX Flanger Mix",
                  "FX Phaser Freq", "FX Phaser Feedback", "FX Phaser Mix",
                  "FX Chorus Depth", "FX Chorus Mix",
                  "FX Delay Feedback", "FX Delay Mix",
                  "FX Multiband Depth",
                  "FX EQ Low Gain", "FX EQ High Gain",
                  "FX Filter Cutoff", "FX Filter Res", "FX Filter Drive", "FX Filter Mix",
                  "FX Reverb Size", "FX Reverb Decay", "FX Reverb Mix" });
    jassert (a.size() == DstCount);
    return a;
}

int modDestinationForParam (const juce::String& paramID)
{
    if (paramID.isEmpty())
        return -1;
    const int i = modDestinationParamIDs().indexOf (paramID);
    return i > 0 ? i : -1;
}

//==============================================================================
juce::StringArray warpModeNames()
{
    return { "Off", "Sync", "Bend +", "Bend -", "Asym", "PWM", "Mirror", "Flip",
             "Quantize", "FM (other)", "AM (other)", "RM (other)" };
}

juce::StringArray filterTypeNames()
{
    return { "Low 12", "Low 24", "Ladder", "High 12", "High 24", "Band 12", "Band 24",
             "Notch", "Peak", "Comb +", "Comb -", "Vowel" };
}

juce::StringArray subShapeNames()     { return { "Sine", "Triangle", "Saw", "Square", "Pulse" }; }
juce::StringArray noiseTypeNames()    { return { "White", "Pink", "Brown", "Crackle", "Digital" }; }
juce::StringArray lfoShapeNames()     { return { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Custom", "S&H", "Drift" }; }
juce::StringArray lfoModeNames()      { return { "Free", "Retrig", "Envelope" }; }
juce::StringArray distortionModeNames()
{
    return { "Tube", "Soft Clip", "Hard Clip", "Sine Fold", "Crush", "Downsample",
             "Diode", "Linear Fold", "Asym", "Rectify", "Squash", "Fuzz", "Tape" };
}

juce::StringArray fxModuleNames()
{
    return { "Distortion", "Chorus", "Phaser", "Delay", "Reverb", "Compressor", "EQ",
             "Hyper", "Flanger", "Multiband", "Filter" };
}

juce::String fxOnParamID (int module)
{
    static const char* ids[] = { "fx_dist_on", "fx_cho_on", "fx_pha_on", "fx_dly_on", "fx_rev_on", "fx_cmp_on", "fx_eq_on",
                                 "fx_hyp_on", "fx_fla_on", "fx_mb_on", "fx_fil_on" };
    static_assert (std::size (ids) == kNumFx);
    return juce::isPositiveAndBelow (module, kNumFx) ? ids[module] : "";
}

juce::StringArray fxFilterTypeNames()
{
    return { "Low 12", "Low 24", "High 12", "High 24", "Band 12", "Band 24", "Notch", "Peak" };
}

juce::StringArray syncDivisionNames()
{
    return { "8 bars", "4 bars", "2 bars", "1 bar", "1/2", "1/2 T", "1/4.", "1/4", "1/4 T",
             "1/8.", "1/8", "1/8 T", "1/16.", "1/16", "1/16 T", "1/32" };
}

double syncDivisionBeats (int i)
{
    static const double beats[] = { 32.0, 16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0,
                                    0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
    return beats[juce::jlimit (0, (int) std::size (beats) - 1, i)];
}

//==============================================================================
namespace
{
    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    // A number with a unit, with fewer decimals as the number grows.
    Attr withLabel (const char* label)
    {
        return Attr().withLabel (label)
                     .withStringFromValueFunction ([label] (float v, int)
                     {
                         const float a = std::abs (v);
                         return juce::String (v, a < 10.0f ? 2 : (a < 100.0f ? 1 : 0)) + " " + label;
                     })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }

    // Whole numbers with a unit, e.g. "-12 st".
    Attr rounded (const char* unit)
    {
        return Attr().withLabel (unit)
                     .withStringFromValueFunction ([unit] (float v, int) { return juce::String (juce::roundToInt (v)) + " " + unit; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); });
    }

    Attr percent()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    }

    Attr bipolarPercent()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; })
                     .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue() / 100.0f; });
    }

    Attr pan()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            const int p = juce::roundToInt (v * 100.0f);
            return p == 0 ? juce::String ("C") : (p < 0 ? juce::String (-p) + "L" : juce::String (p) + "R");
        });
    }

    Attr seconds()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
        });
    }

    Attr hertz()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            return v < 1000.0f ? juce::String (v, v < 10.0f ? 2 : 0) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz";
        });
    }

    Attr gainDb()
    {
        return Attr().withStringFromValueFunction ([] (float v, int)
        {
            return v <= 0.0001f ? juce::String ("-inf") : juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB";
        });
    }

    Attr decibels()
    {
        return Attr().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " dB"; });
    }

    template <typename P, typename... Args>
    void add (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& pid, Args&&... args)
    {
        layout.add (std::make_unique<P> (juce::ParameterID { pid, 1 }, std::forward<Args> (args)...));
    }

    // Parameters added in 0.3 carry a newer version hint so hosts keep the older ones in place.
    template <typename P, typename... Args>
    void add3 (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const juce::String& pid, Args&&... args)
    {
        layout.add (std::make_unique<P> (juce::ParameterID { pid, 3 }, std::forward<Args> (args)...));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;

    // Oscillators
    for (int o = 0; o < kNumOscs; ++o)
    {
        const juce::String n = o == 0 ? "Osc A " : "Osc B ";
        add<APB> (L, id::osc (o, "on"), n + "On", o == 0);
        add<APF> (L, id::osc (o, "level"), n + "Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f, percent());
        add<APF> (L, id::osc (o, "pan"), n + "Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, pan());
        add<API> (L, id::osc (o, "octave"), n + "Octave", -4, 4, 0);
        add<API> (L, id::osc (o, "semi"), n + "Semi", -12, 12, 0);
        add<APF> (L, id::osc (o, "fine"), n + "Fine", juce::NormalisableRange<float> (-100.0f, 100.0f), 0.0f, rounded ("ct"));
        add<APF> (L, id::osc (o, "coarse"), n + "Pitch", juce::NormalisableRange<float> (-48.0f, 48.0f), 0.0f, rounded ("st"));
        add<APF> (L, id::osc (o, "pos"), n + "WT Pos", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
        add<API> (L, id::osc (o, "unison"), n + "Unison", 1, kMaxUnison, 1);
        add<APF> (L, id::osc (o, "detune"), n + "Detune", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, percent());
        add<APF> (L, id::osc (o, "blend"), n + "Blend", juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f, percent());
        add<APF> (L, id::osc (o, "width"), n + "Width", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent());
        add<APF> (L, id::osc (o, "phase"), n + "Phase", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
        add<APF> (L, id::osc (o, "rand"), n + "Random Phase", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent());
        add<APC> (L, id::osc (o, "warp"), n + "Warp", warpModeNames(), 0);
        add<APF> (L, id::osc (o, "warpAmt"), n + "Warp Amount", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
        add<APB> (L, id::osc (o, "toFilter"), n + "To Filter", true);
    }

    // Sub and noise
    add<APB> (L, "sub_on", "Sub On", false);
    add<APC> (L, "sub_shape", "Sub Shape", subShapeNames(), 0);
    add<API> (L, "sub_octave", "Sub Octave", -4, 0, -1);
    add<APF> (L, "sub_level", "Sub Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f, percent());
    add<APF> (L, "sub_pan", "Sub Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, pan());
    add<APB> (L, "sub_toFilter", "Sub To Filter", true);

    add<APB> (L, "noise_on", "Noise On", false);
    add<APC> (L, "noise_type", "Noise Type", noiseTypeNames(), 0);
    add<APF> (L, "noise_color", "Noise Color", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());
    add<APF> (L, "noise_level", "Noise Level", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent());
    add<APF> (L, "noise_pan", "Noise Pan", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, pan());
    add<APB> (L, "noise_toFilter", "Noise To Filter", true);

    // Filter
    add<APB> (L, "flt_on", "Filter On", false);
    add<APC> (L, "flt_type", "Filter Type", filterTypeNames(), 1);
    add<APF> (L, "flt_cutoff", "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 1500.0f, hertz());
    add<APF> (L, "flt_res", "Filter Resonance", juce::NormalisableRange<float> (0.0f, 1.0f), 0.15f, percent());
    add<APF> (L, "flt_drive", "Filter Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
    add<APF> (L, "flt_mix", "Filter Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent());
    add<APF> (L, "flt_keytrack", "Filter Keytrack", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());

    // Envelopes (Env 1 always drives the amplifier)
    for (int e = 0; e < kNumEnvs; ++e)
    {
        const juce::String n = "Env " + juce::String (e + 1) + " ";
        add<APF> (L, id::env (e, "attack"), n + "Attack", skewed (0.0f, 10.0f, 0.5f), e == 0 ? 0.002f : 0.01f, seconds());
        add<APF> (L, id::env (e, "hold"), n + "Hold", skewed (0.0f, 5.0f, 0.3f), 0.0f, seconds());
        add<APF> (L, id::env (e, "decay"), n + "Decay", skewed (0.0f, 10.0f, 1.0f), e == 0 ? 1.0f : 0.6f, seconds());
        add<APF> (L, id::env (e, "sustain"), n + "Sustain", juce::NormalisableRange<float> (0.0f, 1.0f), e == 0 ? 1.0f : 0.0f, percent());
        add<APF> (L, id::env (e, "release"), n + "Release", skewed (0.0f, 15.0f, 1.0f), e == 0 ? 0.15f : 0.3f, seconds());
        add<APF> (L, id::env (e, "curve"), n + "Curve", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());
    }

    // LFOs
    for (int l = 0; l < kNumLfos; ++l)
    {
        const juce::String n = "LFO " + juce::String (l + 1) + " ";
        add<APC> (L, id::lfo (l, "shape"), n + "Shape", lfoShapeNames(), 0);
        add<APC> (L, id::lfo (l, "mode"), n + "Mode", lfoModeNames(), 1);
        add<APB> (L, id::lfo (l, "sync"), n + "Sync", true);
        add<APF> (L, id::lfo (l, "rate"), n + "Rate", skewed (0.01f, 50.0f, 2.0f), 2.0f, hertz());
        add<APC> (L, id::lfo (l, "div"), n + "Division", syncDivisionNames(), 7);
        add<APF> (L, id::lfo (l, "rise"), n + "Rise", skewed (0.0f, 5.0f, 0.5f), 0.0f, seconds());
        add<APF> (L, id::lfo (l, "smooth"), n + "Smooth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
        add<APF> (L, id::lfo (l, "phase"), n + "Phase", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
    }

    for (int m = 0; m < kNumMacros; ++m)
        add<APF> (L, id::macro (m), "Macro " + juce::String (m + 1), juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());

    // Modulation matrix
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const juce::String n = "Mod " + juce::String (s + 1) + " ";
        add<APC> (L, id::slot (s, "src"), n + "Source", modSourceNames(), 0);
        add<APC> (L, id::slot (s, "dst"), n + "Destination", modDestinationNames(), 0);
        add<APF> (L, id::slot (s, "amount"), n + "Amount", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent());
        add<APB> (L, id::slot (s, "bipolar"), n + "Bipolar", false);
    }

    // Voicing and master
    add<APF> (L, "master_vol", "Master Volume", juce::NormalisableRange<float> (0.0f, 1.0f), 0.7f, gainDb());
    add<API> (L, "poly", "Polyphony", 1, kMaxVoices, 8);
    add<APB> (L, "mono", "Mono", false);
    add<APB> (L, "legato", "Legato", false);
    add<APF> (L, "glide", "Glide", skewed (0.0f, 2.0f, 0.2f), 0.0f, seconds());
    add<API> (L, "bend_range", "Pitch Bend Range", 0, 24, 2);
    add<APF> (L, "vel_sens", "Velocity Sensitivity", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());

    // Effects
    add<APB> (L, "fx_dist_on", "Distortion On", false);
    add<APC> (L, "fx_dist_mode", "Distortion Mode", distortionModeNames(), 0);
    add<APF> (L, "fx_dist_drive", "Distortion Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.3f, percent());
    add<APF> (L, "fx_dist_tone", "Distortion Tone", juce::NormalisableRange<float> (0.0f, 1.0f), 0.8f, percent());
    add<APF> (L, "fx_dist_mix", "Distortion Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent());

    add<APB> (L, "fx_cho_on", "Chorus On", false);
    add<APF> (L, "fx_cho_rate", "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.6f, hertz());
    add<APF> (L, "fx_cho_depth", "Chorus Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f, percent());
    add<APF> (L, "fx_cho_delay", "Chorus Delay", juce::NormalisableRange<float> (1.0f, 30.0f), 8.0f, withLabel ("ms"));
    add<APF> (L, "fx_cho_feedback", "Chorus Feedback", juce::NormalisableRange<float> (0.0f, 0.9f), 0.2f, percent());
    add<APF> (L, "fx_cho_mix", "Chorus Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());

    add<APB> (L, "fx_pha_on", "Phaser On", false);
    add<APF> (L, "fx_pha_rate", "Phaser Rate", skewed (0.05f, 10.0f, 1.0f), 0.4f, hertz());
    add<APF> (L, "fx_pha_depth", "Phaser Depth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent());
    add<APF> (L, "fx_pha_freq", "Phaser Frequency", skewed (100.0f, 5000.0f, 800.0f), 800.0f, hertz());
    add<APF> (L, "fx_pha_feedback", "Phaser Feedback", juce::NormalisableRange<float> (0.0f, 0.95f), 0.5f, percent());
    add<APF> (L, "fx_pha_mix", "Phaser Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());

    add<APB> (L, "fx_dly_on", "Delay On", false);
    add<APB> (L, "fx_dly_sync", "Delay Sync", true);
    add<APC> (L, "fx_dly_div", "Delay Division", syncDivisionNames(), 9);
    add<APF> (L, "fx_dly_time", "Delay Time", skewed (1.0f, 2000.0f, 300.0f), 350.0f, withLabel ("ms"));
    add<APF> (L, "fx_dly_feedback", "Delay Feedback", juce::NormalisableRange<float> (0.0f, 0.95f), 0.4f, percent());
    add<APB> (L, "fx_dly_pingpong", "Delay Ping Pong", true);
    add<APF> (L, "fx_dly_hicut", "Delay High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 8000.0f, hertz());
    add<APF> (L, "fx_dly_mix", "Delay Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, percent());

    add<APB> (L, "fx_rev_on", "Reverb On", false);
    add<APF> (L, "fx_rev_size", "Reverb Size", juce::NormalisableRange<float> (0.0f, 1.0f), 0.6f, percent());
    add<APF> (L, "fx_rev_damp", "Reverb Damping", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, percent());
    add<APF> (L, "fx_rev_width", "Reverb Width", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent());
    add<APF> (L, "fx_rev_mix", "Reverb Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.25f, percent());

    add<APB> (L, "fx_cmp_on", "Compressor On", false);
    add<APB> (L, "fx_cmp_multi", "Compressor Multiband", false);
    add<APF> (L, "fx_cmp_thresh", "Compressor Threshold", juce::NormalisableRange<float> (-60.0f, 0.0f), -18.0f, decibels());
    add<APF> (L, "fx_cmp_ratio", "Compressor Ratio", skewed (1.0f, 20.0f, 4.0f), 4.0f, withLabel (":1"));
    add<APF> (L, "fx_cmp_attack", "Compressor Attack", skewed (0.1f, 100.0f, 10.0f), 5.0f, withLabel ("ms"));
    add<APF> (L, "fx_cmp_release", "Compressor Release", skewed (10.0f, 1000.0f, 150.0f), 120.0f, withLabel ("ms"));
    add<APF> (L, "fx_cmp_upward", "Compressor Upward", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, percent());
    add<APF> (L, "fx_cmp_gain", "Compressor Gain", juce::NormalisableRange<float> (-12.0f, 24.0f), 0.0f, decibels());
    add<APF> (L, "fx_cmp_mix", "Compressor Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 1.0f, percent());

    add<APB> (L, "fx_eq_on", "EQ On", false);
    add<APF> (L, "fx_eq_lowfreq", "EQ Low Freq", skewed (20.0f, 1000.0f, 150.0f), 120.0f, hertz());
    add<APF> (L, "fx_eq_lowgain", "EQ Low Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, decibels());
    add<APF> (L, "fx_eq_highfreq", "EQ High Freq", skewed (1000.0f, 20000.0f, 5000.0f), 6000.0f, hertz());
    add<APF> (L, "fx_eq_highgain", "EQ High Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, decibels());

    // 0.3: the rest of the rack, and more controls on the modules above.
    const juce::NormalisableRange<float> unit (0.0f, 1.0f);
    add3<APB> (L, "fx_hyp_on", "Hyper On", false);
    add3<APF> (L, "fx_hyp_rate", "Hyper Rate", skewed (0.05f, 10.0f, 1.0f), 0.8f, hertz());
    add3<APF> (L, "fx_hyp_detune", "Hyper Detune", unit, 0.3f, percent());
    add3<API> (L, "fx_hyp_voices", "Hyper Voices", 2, 7, 4);
    add3<APF> (L, "fx_hyp_mix", "Hyper Mix", unit, 0.5f, percent());
    add3<APF> (L, "fx_hyp_size", "Widen Size", unit, 0.5f, percent());
    add3<APF> (L, "fx_hyp_wide", "Widen Mix", unit, 0.0f, percent());

    add3<APC> (L, "fx_dist_fmode", "Distortion Filter", juce::StringArray { "Off", "Pre", "Post" }, 0);
    add3<APC> (L, "fx_dist_ftype", "Distortion Filter Type", juce::StringArray { "Low", "Band", "High" }, 0);
    add3<APF> (L, "fx_dist_ffreq", "Distortion Filter Freq", skewed (20.0f, 20000.0f, 1000.0f), 1000.0f, hertz());
    add3<APF> (L, "fx_dist_fres", "Distortion Filter Res", unit, 0.2f, percent());

    add3<APB> (L, "fx_fla_on", "Flanger On", false);
    add3<APB> (L, "fx_fla_sync", "Flanger Sync", false);
    add3<APC> (L, "fx_fla_div", "Flanger Division", syncDivisionNames(), 3);
    add3<APF> (L, "fx_fla_rate", "Flanger Rate", skewed (0.02f, 10.0f, 0.5f), 0.3f, hertz());
    add3<APF> (L, "fx_fla_depth", "Flanger Depth", unit, 0.6f, percent());
    add3<APF> (L, "fx_fla_feedback", "Flanger Feedback", juce::NormalisableRange<float> (-0.95f, 0.95f), 0.5f, bipolarPercent());
    add3<APF> (L, "fx_fla_spread", "Flanger Stereo", unit, 0.25f, percent());
    add3<APF> (L, "fx_fla_mix", "Flanger Mix", unit, 0.5f, percent());

    add3<APC> (L, "fx_pha_stages", "Phaser Stages", juce::StringArray { "2", "4", "6", "8", "10", "12" }, 2);
    add3<APF> (L, "fx_pha_spread", "Phaser Stereo", unit, 0.25f, percent());

    add3<APF> (L, "fx_cho_hicut", "Chorus High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 20000.0f, hertz());
    add3<APF> (L, "fx_cho_spread", "Chorus Stereo", unit, 1.0f, percent());

    add3<APF> (L, "fx_dly_lowcut", "Delay Low Cut", skewed (20.0f, 2000.0f, 200.0f), 20.0f, hertz());
    add3<APF> (L, "fx_dly_offset", "Delay Stereo Offset", juce::NormalisableRange<float> (-0.5f, 0.5f), 0.0f, bipolarPercent());

    add3<APB> (L, "fx_mb_on", "Multiband On", false);
    add3<APF> (L, "fx_mb_depth", "Multiband Depth", unit, 1.0f, percent());
    add3<APF> (L, "fx_mb_time", "Multiband Time", unit, 0.5f, percent());
    add3<APF> (L, "fx_mb_up", "Multiband Upward", unit, 0.4f, percent());
    add3<APF> (L, "fx_mb_down", "Multiband Downward", unit, 1.0f, percent());
    add3<APF> (L, "fx_mb_low", "Multiband Low Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, decibels());
    add3<APF> (L, "fx_mb_mid", "Multiband Mid Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, decibels());
    add3<APF> (L, "fx_mb_high", "Multiband High Gain", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, decibels());
    add3<APF> (L, "fx_mb_gain", "Multiband Output", juce::NormalisableRange<float> (-18.0f, 18.0f), 0.0f, decibels());

    add3<APC> (L, "fx_eq_lowtype", "EQ Low Type", juce::StringArray { "Shelf", "Peak", "Low Cut" }, 0);
    add3<APF> (L, "fx_eq_lowq", "EQ Low Q", skewed (0.3f, 8.0f, 1.0f), 0.7f, withLabel ("Q"));
    add3<APC> (L, "fx_eq_hightype", "EQ High Type", juce::StringArray { "Shelf", "Peak", "High Cut" }, 0);
    add3<APF> (L, "fx_eq_highq", "EQ High Q", skewed (0.3f, 8.0f, 1.0f), 0.7f, withLabel ("Q"));

    add3<APB> (L, "fx_fil_on", "Filter On", false);
    add3<APC> (L, "fx_fil_type", "Filter Type", fxFilterTypeNames(), 0);
    add3<APF> (L, "fx_fil_cutoff", "Filter Cutoff", skewed (20.0f, 20000.0f, 1000.0f), 2000.0f, hertz());
    add3<APF> (L, "fx_fil_res", "Filter Res", unit, 0.2f, percent());
    add3<APF> (L, "fx_fil_drive", "Filter Drive", unit, 0.0f, percent());
    add3<APF> (L, "fx_fil_stereo", "Filter Stereo", juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f, bipolarPercent());
    add3<APF> (L, "fx_fil_mix", "Filter Mix", unit, 1.0f, percent());

    add3<APC> (L, "fx_rev_mode", "Reverb Type", juce::StringArray { "Hall", "Plate", "Room" }, 0);
    add3<APF> (L, "fx_rev_decay", "Reverb Decay", skewed (0.2f, 20.0f, 2.5f), 2.5f, seconds());
    add3<APF> (L, "fx_rev_predelay", "Reverb Pre-delay", skewed (0.0f, 250.0f, 40.0f), 0.0f, withLabel ("ms"));
    add3<APF> (L, "fx_rev_lowcut", "Reverb Low Cut", skewed (20.0f, 1000.0f, 150.0f), 20.0f, hertz());
    add3<APF> (L, "fx_rev_hicut", "Reverb High Cut", skewed (1000.0f, 20000.0f, 6000.0f), 14000.0f, hertz());

    return L;
}
} // namespace cs
