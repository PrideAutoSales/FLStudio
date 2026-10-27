#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzOptoAudioProcessor::CruzOptoAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzOpto", createParameterLayout())
{
    peakReduction = state.getRawParameterValue (ParamIDs::peakReduction);
    gain = state.getRawParameterValue (ParamIDs::gain);
    mode = state.getRawParameterValue (ParamIDs::mode);
    emphasis = state.getRawParameterValue (ParamIDs::emphasis);
    mix = state.getRawParameterValue (ParamIDs::mix);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzOptoAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto plain = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::peakReduction, 1 }, "Peak Reduction",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, plain));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gain, 1 }, "Gain",
                                                       NormalisableRange<float> (-12.0f, 30.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::mode, 1 }, "Mode",
                                                        StringArray { "Compress", "Limit" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::emphasis, 1 }, "HF Emphasis",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, plain));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::meter, 1 }, "Meter",
                                                        StringArray { "Gain Reduction", "Output" }, 0));
    return layout;
}

void CruzOptoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    model.prepare (sampleRate);
    dryWet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) getTotalNumOutputChannels() });
    dryWet.reset();
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
}

bool CruzOptoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CruzOptoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    cruz::OptoModel::Settings s;
    s.peakReduction = peakReduction->load();
    s.gainDb = gain->load();
    s.limitMode = mode->load() > 0.5f;
    s.emphasis = emphasis->load();
    model.setSettings (s);

    juce::dsp::AudioBlock<float> block (buffer);
    dryWet.setWetMixProportion (mix->load() * 0.01f);
    dryWet.pushDrySamples (block);

    const float gr = model.process (buffer.getArrayOfWritePointers(), numOut, buffer.getNumSamples());

    dryWet.mixWetSamples (block);

    gainReductionDb.store (gr);
    float peak = 0.0f;
    for (int ch = 0; ch < numOut; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    outputDb.store (juce::Decibels::gainToDecibels (peak, -100.0f));
}

juce::AudioProcessorEditor* CruzOptoAudioProcessor::createEditor()
{
    return new CruzOptoAudioProcessorEditor (*this);
}

void CruzOptoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzOptoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzOptoAudioProcessor();
}
