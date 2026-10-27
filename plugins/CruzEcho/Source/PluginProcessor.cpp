#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzEchoAudioProcessor::CruzEchoAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzEcho", createParameterLayout())
{
    sync = state.getRawParameterValue (ParamIDs::sync);
    timeNote = state.getRawParameterValue (ParamIDs::timeNote);
    timeMs = state.getRawParameterValue (ParamIDs::timeMs);
    feel = state.getRawParameterValue (ParamIDs::feel);
    feedback = state.getRawParameterValue (ParamIDs::feedback);
    groove = state.getRawParameterValue (ParamIDs::groove);
    style = state.getRawParameterValue (ParamIDs::style);
    mode = state.getRawParameterValue (ParamIDs::mode);
    lowCut = state.getRawParameterValue (ParamIDs::lowCut);
    highCut = state.getRawParameterValue (ParamIDs::highCut);
    saturation = state.getRawParameterValue (ParamIDs::saturation);
    wobble = state.getRawParameterValue (ParamIDs::wobble);
    width = state.getRawParameterValue (ParamIDs::width);
    duck = state.getRawParameterValue (ParamIDs::duck);
    mix = state.getRawParameterValue (ParamIDs::mix);
    output = state.getRawParameterValue (ParamIDs::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzEchoAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = [] (const char* id, const char* name, float max, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> { 0.0f, max, 0.1f }, def,
            AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; }));
    };

    auto hertz = [] (const char* id, const char* name, float min, float max, float def)
    {
        NormalisableRange<float> range { min, max, 1.0f };
        range.setSkewForCentre (std::sqrt (min * max));
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int)
                {
                    return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz" : String (roundToInt (v)) + " Hz";
                }));
    };

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Sync", true));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::timeNote, 1 }, "Time (Note)",
                                                        NoteValues::names(), NoteValues::names().indexOf ("1/8D")));

    NormalisableRange<float> msRange { 1.0f, cruzecho::maxDelayMs, 0.1f };
    msRange.setSkewForCentre (350.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::timeMs, 1 }, "Time (ms)", msRange, 375.0f,
        AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int) { return v < 100.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; })));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::feel, 1 }, "Feel", NormalisableRange<float> { -30.0f, 30.0f, 0.1f }, 0.0f,
        AudioParameterFloatAttributes().withLabel ("ms")
            .withStringFromValueFunction ([] (float v, int)
            {
                if (std::abs (v) < 0.05f) return String ("On grid");
                return (v > 0 ? "Late " : "Early ") + String (std::abs (v), 1) + " ms";
            })));

    layout.add (percent (ParamIDs::feedback, "Feedback", 110.0f, 40.0f));
    layout.add (percent (ParamIDs::groove, "Groove", 100.0f, 0.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::style, 1 }, "Style",
                                                        StringArray { "Digital", "Analog", "Tape", "Space", "Lo-Fi" }, 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode",
                                                        StringArray { "Single", "Stereo", "Ping-Pong" }, 0));
    layout.add (hertz (ParamIDs::lowCut, "Low Cut", 20.0f, 2000.0f, 20.0f));
    layout.add (hertz (ParamIDs::highCut, "High Cut", 1000.0f, 20000.0f, 20000.0f));
    layout.add (percent (ParamIDs::saturation, "Saturation", 100.0f, 25.0f));
    layout.add (percent (ParamIDs::wobble, "Wobble", 100.0f, 30.0f));
    layout.add (percent (ParamIDs::width, "Width", 200.0f, 100.0f));
    layout.add (percent (ParamIDs::duck, "Duck", 100.0f, 0.0f));
    layout.add (percent (ParamIDs::mix, "Mix", 100.0f, 30.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::output, 1 }, "Output", NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    return layout;
}

cruzecho::Params CruzEchoAudioProcessor::readParams()
{
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 1.0)
                hostBpm.store ((float) *bpm);

    float ms = timeMs->load();

    if (sync->load() > 0.5f)
    {
        const auto index = juce::jlimit (0, (int) std::size (NoteValues::beats) - 1, (int) timeNote->load());
        ms = NoteValues::beats[index] * 60000.0f / hostBpm.load();
    }

    ms = juce::jlimit (1.0f, cruzecho::maxDelayMs, ms + feel->load());
    echoTimeMs.store (ms);

    cruzecho::Params p;
    p.delayMs = ms;
    p.feedback = feedback->load() / 100.0f;
    p.groove = groove->load() / 100.0f;
    p.style = (cruzecho::Style) juce::jlimit (0, 4, (int) style->load());
    p.mode = (cruzecho::Mode) juce::jlimit (0, 2, (int) mode->load());
    p.lowCutHz = lowCut->load();
    p.highCutHz = highCut->load();
    p.saturation = saturation->load() / 100.0f;
    p.wobble = wobble->load() / 100.0f;
    p.width = width->load() / 100.0f;
    p.duck = duck->load() / 100.0f;
    p.mix = mix->load() / 100.0f;
    p.outputGain = juce::Decibels::decibelsToGain (output->load());
    return p;
}

void CruzEchoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setParams (readParams());
    engine.prepare (sampleRate, samplesPerBlock);
}

void CruzEchoAudioProcessor::reset()
{
    engine.reset();
}

bool CruzEchoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void CruzEchoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    engine.setParams (readParams());
    engine.process (buffer.getArrayOfWritePointers(), juce::jmin (2, buffer.getNumChannels()), numSamples);
    wetLevel.store (engine.getWetPeak());
}

juce::AudioProcessorEditor* CruzEchoAudioProcessor::createEditor()
{
    return new CruzEchoAudioProcessorEditor (*this);
}

void CruzEchoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzEchoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzEchoAudioProcessor();
}
