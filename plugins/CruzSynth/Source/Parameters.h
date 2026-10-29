#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Everything the synth exposes to the host, plus the lists the modulation
// matrix uses for its sources and destinations.
namespace cs
{
constexpr int kNumOscs = 2;
constexpr int kNumEnvs = 3;
constexpr int kNumLfos = 4;
constexpr int kNumMacros = 4;
constexpr int kNumModSlots = 16;
constexpr int kMaxUnison = 16;
constexpr int kMaxVoices = 32;
constexpr int kLfoTableSize = 512;

//==============================================================================
enum class ModSrc
{
    None,
    Env1, Env2, Env3,
    Lfo1, Lfo2, Lfo3, Lfo4,
    Velocity, NoteTrack, ModWheel, Aftertouch, PitchBend,
    Macro1, Macro2, Macro3, Macro4,
    NoteRandom,
    Count
};

constexpr int kNumModSources = (int) ModSrc::Count;

juce::StringArray modSourceNames();
juce::String modSourceShortName (int src);
juce::Colour modSourceColour (int src);
bool modSourceIsBipolar (int src);

//==============================================================================
// Each destination is a parameter. Modulation is added in the parameter's
// normalised 0..1 space, so one amount means the same knob travel everywhere.
enum ModDst
{
    DstNone,
    DstOscACoarse, DstOscAFine, DstOscAPos, DstOscADetune, DstOscABlend,
    DstOscAWarp, DstOscAPhase, DstOscAPan, DstOscALevel,
    DstOscBCoarse, DstOscBFine, DstOscBPos, DstOscBDetune, DstOscBBlend,
    DstOscBWarp, DstOscBPhase, DstOscBPan, DstOscBLevel,
    DstSubLevel, DstSubPan,
    DstNoiseLevel, DstNoisePan, DstNoiseColor,
    DstFltCutoff, DstFltRes, DstFltDrive, DstFltMix,
    DstLfo1Rate, DstLfo2Rate, DstLfo3Rate, DstLfo4Rate,
    DstMasterVol,
    // Effects. These follow the newest voice, or the macros and wheels when nothing plays.
    DstFxHypDetune, DstFxHypMix, DstFxHypWide,
    DstFxDistDrive, DstFxDistFreq, DstFxDistMix,
    DstFxFlaFeedback, DstFxFlaMix,
    DstFxPhaFreq, DstFxPhaFeedback, DstFxPhaMix,
    DstFxChoDepth, DstFxChoMix,
    DstFxDlyFeedback, DstFxDlyMix,
    DstFxMbDepth,
    DstFxEqLowGain, DstFxEqHighGain,
    DstFxFilCutoff, DstFxFilRes, DstFxFilDrive, DstFxFilMix,
    DstFxRevSize, DstFxRevDecay, DstFxRevMix,
    // Piano (0.4)
    DstPnoLevel, DstPnoTone,
    DstCount
};

const juce::StringArray& modDestinationParamIDs(); // index = ModDst, "" for None
juce::StringArray modDestinationNames();
int modDestinationForParam (const juce::String& paramID); // -1 if not modulatable

//==============================================================================
juce::StringArray warpModeNames();
enum WarpMode { WarpOff, WarpSync, WarpBendPlus, WarpBendMinus, WarpAsym, WarpPwm,
                WarpMirror, WarpFlip, WarpQuantize, WarpFm, WarpAm, WarpRm };

juce::StringArray filterTypeNames();
enum FilterType { FltLp12, FltLp24, FltLadder, FltHp12, FltHp24, FltBp12, FltBp24,
                  FltNotch, FltPeak, FltCombPlus, FltCombMinus, FltVowel };

juce::StringArray subShapeNames();
juce::StringArray noiseTypeNames();
juce::StringArray lfoShapeNames();
enum LfoShape { LfoSine, LfoTriangle, LfoSawUp, LfoSawDown, LfoSquare, LfoCustom, LfoSampleHold, LfoSmoothRandom };
juce::StringArray lfoModeNames();
enum LfoMode { LfoFree, LfoRetrig, LfoEnvelope };
juce::StringArray syncDivisionNames();
double syncDivisionBeats (int index);

juce::StringArray distortionModeNames();
juce::StringArray fxModuleNames();
constexpr int kNumFx = 11;
// New modules are appended so saved chains and presets keep their meaning.
enum FxModule { FxDistortion, FxChorus, FxPhaser, FxDelay, FxReverb, FxCompressor, FxEq,
                FxHyper, FxFlanger, FxMultiband, FxFilter };
juce::String fxOnParamID (int module);
juce::StringArray fxFilterTypeNames();

//==============================================================================
namespace id
{
    juce::String osc (int osc, const char* name);   // "oscA_level"
    juce::String env (int env, const char* name);   // "env1_attack"
    juce::String lfo (int lfo, const char* name);   // "lfo1_rate"
    juce::String macro (int m);                     // "macro1"
    juce::String slot (int s, const char* name);    // "mod3_amount"
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace cs
