#include "Parameters.h"

namespace Params
{
namespace
{
juce::NormalisableRange<float> logRange (float min, float max)
{
    return { min, max,
             [] (float start, float end, float t) { return start * std::pow (end / start, t); },
             [] (float start, float end, float v) { return std::log (v / start) / std::log (end / start); },
             [] (float start, float end, float v) { return juce::jlimit (start, end, v); } };
}
} // namespace

juce::String bandId (int band, BandField field)
{
    static const char* names[] { "used", "bypass", "freq", "gain", "q", "shape", "slope", "place" };
    return "b" + juce::String (band + 1) + "_" + names[(int) field];
}

const juce::StringArray& shapeNames()
{
    static const juce::StringArray names { "Bell", "Low Shelf", "Low Cut", "High Shelf", "High Cut", "Notch", "Band Pass", "Tilt Shelf", "Flat Tilt" };
    return names;
}

const juce::StringArray& slopeNames()
{
    static const juce::StringArray names = []
    {
        juce::StringArray result;
        for (int s : cruz::dsp::slopesDbPerOct)
            result.add (juce::String (s) + " dB/oct");
        return result;
    }();
    return names;
}

const juce::StringArray& placementNames()
{
    static const juce::StringArray names { "Stereo", "Left", "Right", "Mid", "Side" };
    return names;
}

juce::String formatFrequency (double hz)
{
    if (hz < 1000.0)
        return juce::String (hz, hz < 100.0 ? 1 : 0) + " Hz";

    return juce::String (hz / 1000.0, hz < 10000.0 ? 2 : 1) + " kHz";
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace cruz::dsp;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { outputGain, 1 }, "Output Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { autoGain, 1 }, "Auto Gain", false));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gainScale, 1 }, "Gain Scale",
        juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f }, 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0) + "%"; })));

    const auto freqText = [] (float v, int) { return formatFrequency (v); };
    const auto dbText = [] (float v, int) { return juce::String (v, 2) + " dB"; };
    const auto qText = [] (float v, int) { return juce::String (v, 3); };

    for (int b = 0; b < numBands; ++b)
    {
        const auto name = "Band " + juce::String (b + 1) + " ";
        auto group = std::make_unique<juce::AudioProcessorParameterGroup> ("band" + juce::String (b + 1), name.trim(), "|");

        // Spread the default frequencies so new bands start in sensible places.
        const float defaultFreq = (float) (30.0 * std::pow (16000.0 / 30.0, b / (numBands - 1.0)));

        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bandId (b, BandField::used), 1 }, name + "Used", false));
        group->addChild (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { bandId (b, BandField::bypass), 1 }, name + "Bypass", false));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { bandId (b, BandField::freq), 1 }, name + "Frequency",
            logRange ((float) minFreqHz, (float) maxFreqHz), defaultFreq,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (freqText)));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { bandId (b, BandField::gain), 1 }, name + "Gain",
            juce::NormalisableRange<float> { (float) minGainDb, (float) maxGainDb, 0.01f }, 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbText)));
        group->addChild (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { bandId (b, BandField::q), 1 }, name + "Q",
            logRange ((float) minQ, (float) maxQ), 1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (qText)));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, BandField::shape), 1 }, name + "Shape", shapeNames(), 0));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, BandField::slope), 1 }, name + "Slope", slopeNames(), 1));
        group->addChild (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { bandId (b, BandField::place), 1 }, name + "Placement", placementNames(), 0));

        layout.add (std::move (group));
    }

    return layout;
}

BandRefs BandRefs::fromState (juce::AudioProcessorValueTreeState& state, int band)
{
    BandRefs r;
    r.used = state.getRawParameterValue (bandId (band, BandField::used));
    r.bypass = state.getRawParameterValue (bandId (band, BandField::bypass));
    r.freq = state.getRawParameterValue (bandId (band, BandField::freq));
    r.gain = state.getRawParameterValue (bandId (band, BandField::gain));
    r.q = state.getRawParameterValue (bandId (band, BandField::q));
    r.shape = state.getRawParameterValue (bandId (band, BandField::shape));
    r.slope = state.getRawParameterValue (bandId (band, BandField::slope));
    r.place = state.getRawParameterValue (bandId (band, BandField::place));
    return r;
}

cruz::dsp::BandParams BandRefs::read() const noexcept
{
    using namespace cruz::dsp;
    const int shapeIndex = juce::jlimit (0, numShapes - 1, juce::roundToInt (shape->load()));
    const int slopeIndex = juce::jlimit (0, (int) slopesDbPerOct.size() - 1, juce::roundToInt (slope->load()));
    return { (Shape) shapeIndex, freq->load(), gain->load(), q->load(), slopesDbPerOct[(size_t) slopeIndex] };
}

cruz::dsp::Placement BandRefs::placement() const noexcept
{
    return (cruz::dsp::Placement) juce::jlimit (0, cruz::dsp::numPlacements - 1, juce::roundToInt (place->load()));
}
} // namespace Params
