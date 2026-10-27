#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values; // anything not listed goes back to its default
    };

    const std::vector<Preset>& presets()
    {
        using namespace ParamIDs;

        static const std::vector<Preset> list {
            { "Medium Hall", {} },
            { "Small Room", { { space, 12 }, { decay, 0.6f }, { predelay, 3 }, { distance, 20 }, { brightness, 55 },
                              { character, 15 }, { width, 90 }, { decayLowMult, 1.0f }, { decayHighMult, 0.6f }, { mix, 20 } } },
            { "Drum Room", { { space, 22 }, { decay, 0.9f }, { predelay, 0 }, { distance, 15 }, { brightness, 70 },
                             { character, 10 }, { width, 120 }, { decayLowMult, 0.8f }, { decayHighMult, 0.7f },
                             { lowCut, 120 }, { mix, 18 } } },
            { "Vocal Plate", { { space, 35 }, { decay, 1.8f }, { predelay, 25 }, { distance, 55 }, { brightness, 78 },
                               { character, 45 }, { width, 130 }, { decayLowMult, 0.7f }, { decayHighMult, 0.9f },
                               { lowCut, 180 }, { mix, 22 } } },
            { "Bright Plate", { { space, 40 }, { decay, 2.6f }, { predelay, 10 }, { distance, 60 }, { brightness, 90 },
                                { character, 35 }, { width, 140 }, { decayLowMult, 0.6f }, { decayHighMult, 1.1f },
                                { lowCut, 250 }, { mix, 20 } } },
            { "Concert Hall", { { space, 70 }, { decay, 3.2f }, { predelay, 30 }, { distance, 60 }, { brightness, 60 },
                                { character, 30 }, { width, 120 }, { decayLowMult, 1.3f }, { decayHighMult, 0.45f }, { mix, 22 } } },
            { "Cathedral", { { space, 92 }, { decay, 7.5f }, { predelay, 45 }, { distance, 80 }, { brightness, 50 },
                             { character, 25 }, { width, 140 }, { decayLowMult, 1.4f }, { decayHighMult, 0.4f }, { mix, 25 } } },
            { "Dark Chamber", { { space, 50 }, { decay, 2.4f }, { predelay, 15 }, { distance, 45 }, { brightness, 25 },
                                { character, 20 }, { decayLowMult, 1.2f }, { decayHighMult, 0.35f }, { mix, 22 } } },
            { "Ducked Vocal Verb", { { space, 45 }, { decay, 2.5f }, { predelay, 35 }, { distance, 60 }, { brightness, 75 },
                                     { character, 40 }, { width, 130 }, { decayLowMult, 0.7f }, { decayHighMult, 0.8f },
                                     { lowCut, 200 }, { ducking, 60 }, { mix, 30 } } },
            { "Ambient Wash", { { space, 100 }, { decay, 14 }, { predelay, 60 }, { distance, 95 }, { brightness, 65 },
                                { character, 85 }, { width, 180 }, { decayLowMult, 0.9f }, { decayHighMult, 0.7f },
                                { lowCut, 150 }, { mix, 35 } } },
        };

        return list;
    }

    juce::NormalisableRange<float> skewedRange (float start, float end, float centre, float interval = 0.0f)
    {
        juce::NormalisableRange<float> r { start, end, interval };
        r.setSkewForCentre (centre);
        return r;
    }
}

CruzVerbAudioProcessor::CruzVerbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzVerb", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzVerbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percent = juce::AudioParameterFloatAttributes().withLabel ("%");
    const auto hz = juce::AudioParameterFloatAttributes()
                        .withLabel ("Hz")
                        .withStringFromValueFunction ([] (float v, int)
                        {
                            return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (v));
                        });
    const auto times = juce::AudioParameterFloatAttributes()
                           .withLabel ("x")
                           .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2); });

    const auto add = [&] (const char* id, const char* name, juce::NormalisableRange<float> range, float def,
                          juce::AudioParameterFloatAttributes attrs)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def, attrs));
    };

    using namespace ParamIDs;
    const cruzverb::ReverbSettings d;

    add (space, "Space", { 0.0f, 100.0f, 0.1f }, d.space, percent);
    add (decay, "Decay", skewedRange (0.1f, 20.0f, 2.0f, 0.01f), d.decaySeconds,
         juce::AudioParameterFloatAttributes().withLabel ("s"));
    add (predelay, "Pre-delay", skewedRange (0.0f, 500.0f, 60.0f, 0.1f), d.predelayMs,
         juce::AudioParameterFloatAttributes().withLabel ("ms"));
    add (distance, "Distance", { 0.0f, 100.0f, 0.1f }, d.distance, percent);
    add (brightness, "Brightness", { 0.0f, 100.0f, 0.1f }, d.brightness, percent);
    add (character, "Character", { 0.0f, 100.0f, 0.1f }, d.character, percent);
    add (width, "Width", { 0.0f, 200.0f, 0.1f }, d.width, percent);
    add (decayLowFreq, "Decay Low Freq", skewedRange (20.0f, 2000.0f, 200.0f), d.decayLowFreq, hz);
    add (decayLowMult, "Decay Low Time", skewedRange (0.1f, 4.0f, 1.0f), d.decayLowMult, times);
    add (decayHighFreq, "Decay High Freq", skewedRange (500.0f, 20000.0f, 3200.0f), d.decayHighFreq, hz);
    add (decayHighMult, "Decay High Time", skewedRange (0.1f, 4.0f, 1.0f), d.decayHighMult, times);
    add (lowCut, "Low Cut", skewedRange (10.0f, 2000.0f, 140.0f), d.lowCut, hz);
    add (highCut, "High Cut", skewedRange (1000.0f, 22000.0f, 6000.0f), d.highCut, hz);
    add (lowShelfFreq, "Low Shelf Freq", skewedRange (30.0f, 1000.0f, 180.0f), d.lowShelfFreq, hz);
    add (lowShelfGain, "Low Shelf Gain", { -18.0f, 18.0f, 0.01f }, d.lowShelfGain,
         juce::AudioParameterFloatAttributes().withLabel ("dB"));
    add (highShelfFreq, "High Shelf Freq", skewedRange (1000.0f, 16000.0f, 4000.0f), d.highShelfFreq, hz);
    add (highShelfGain, "High Shelf Gain", { -18.0f, 18.0f, 0.01f }, d.highShelfGain,
         juce::AudioParameterFloatAttributes().withLabel ("dB"));
    add (ducking, "Ducking", { 0.0f, 100.0f, 0.1f }, d.ducking, percent);

    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { freeze, 1 }, "Freeze", false));

    add (mix, "Mix", { 0.0f, 100.0f, 0.1f }, 25.0f, percent);
    add (outputGain, "Output", { -24.0f, 12.0f, 0.01f }, 0.0f,
         juce::AudioParameterFloatAttributes().withLabel ("dB"));

    return layout;
}

cruzverb::ReverbSettings CruzVerbAudioProcessor::readSettings() const noexcept
{
    using namespace ParamIDs;
    cruzverb::ReverbSettings s;
    s.space = param (space);
    s.decaySeconds = param (decay);
    s.predelayMs = param (predelay);
    s.distance = param (distance);
    s.brightness = param (brightness);
    s.character = param (character);
    s.width = param (width);
    s.decayLowFreq = param (decayLowFreq);
    s.decayLowMult = param (decayLowMult);
    s.decayHighFreq = param (decayHighFreq);
    s.decayHighMult = param (decayHighMult);
    s.lowCut = param (lowCut);
    s.highCut = param (highCut);
    s.lowShelfFreq = param (lowShelfFreq);
    s.lowShelfGain = param (lowShelfGain);
    s.highShelfFreq = param (highShelfFreq);
    s.highShelfGain = param (highShelfGain);
    s.ducking = param (ducking);
    s.freeze = param (freeze) > 0.5f;
    return s;
}

void CruzVerbAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    engine.prepare (sampleRate);

    for (auto* v : { &wetGain, &dryGain, &outputLevel })
        v->reset (sampleRate, 0.03);

    reset();
}

void CruzVerbAudioProcessor::reset()
{
    engine.setSettings (readSettings());
    engine.reset();

    const auto m = param (ParamIDs::mix) / 100.0f * juce::MathConstants<float>::halfPi;
    wetGain.setCurrentAndTargetValue (std::sin (m));
    dryGain.setCurrentAndTargetValue (std::cos (m));
    outputLevel.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (param (ParamIDs::outputGain)));
}

bool CruzVerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

double CruzVerbAudioProcessor::getTailLengthSeconds() const
{
    const auto longest = param (ParamIDs::decay)
                       * std::max ({ 1.0f, param (ParamIDs::decayLowMult), param (ParamIDs::decayHighMult) });
    return longest * 1.2 + param (ParamIDs::predelay) * 0.001 + 0.2;
}

void CruzVerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = getTotalNumOutputChannels();

    for (auto ch = getTotalNumInputChannels(); ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numChannels == 0)
        return;

    engine.setSettings (readSettings());

    const auto m = param (ParamIDs::mix) / 100.0f * juce::MathConstants<float>::halfPi;
    wetGain.setTargetValue (std::sin (m));
    dryGain.setTargetValue (std::cos (m));
    outputLevel.setTargetValue (juce::Decibels::decibelsToGain (param (ParamIDs::outputGain)));

    auto* left = buffer.getWritePointer (0);
    auto* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const auto n = std::min (chunkSize, numSamples - start);
        auto* l = left + start;
        auto* r = right != nullptr ? right + start : l;

        engine.process (l, r, wetL.data(), wetR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            analyzerChunk[(size_t) i] = 0.5f * (wetL[(size_t) i] + wetR[(size_t) i]);

            const auto wg = wetGain.getNextValue(), dg = dryGain.getNextValue(), og = outputLevel.getNextValue();

            if (right != nullptr)
            {
                l[i] = (dg * l[i] + wg * wetL[(size_t) i]) * og;
                r[i] = (dg * r[i] + wg * wetR[(size_t) i]) * og;
            }
            else
            {
                l[i] = (dg * l[i] + wg * analyzerChunk[(size_t) i]) * og;
            }
        }

        analyzerFifo.push (analyzerChunk.data(), n);
    }
}

int CruzVerbAudioProcessor::getNumPrograms()
{
    return (int) presets().size();
}

const juce::String CruzVerbAudioProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets().size()) ? presets()[(size_t) index].name : "";
}

void CruzVerbAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets().size()))
        return;

    currentProgram = index;
    const auto& preset = presets()[(size_t) index];

    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            continue;

        // Freeze and output level stay where the user left them.
        const auto& id = ranged->getParameterID();
        if (id == ParamIDs::freeze || id == ParamIDs::outputGain)
            continue;

        auto value = ranged->getDefaultValue();

        for (const auto& [presetId, v] : preset.values)
            if (id == presetId)
                value = ranged->convertTo0to1 (v);

        ranged->setValueNotifyingHost (value);
    }
}

juce::AudioProcessorEditor* CruzVerbAudioProcessor::createEditor()
{
    return new CruzVerbAudioProcessorEditor (*this);
}

void CruzVerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("program", currentProgram, nullptr);

    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzVerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0));
            state.replaceState (tree);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzVerbAudioProcessor();
}
