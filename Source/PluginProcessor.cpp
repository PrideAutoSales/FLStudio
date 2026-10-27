#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzEQAudioProcessor::CruzEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzEQ", createParameterLayout())
{
    outputGainDb = state.getRawParameterValue (ParamIDs::outputGain);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzEQAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::outputGain, 1 },
        "Output Gain",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void CruzEQAudioProcessor::prepareToPlay (double sampleRate, int)
{
    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputGainDb->load()));
}

bool CruzEQAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void CruzEQAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb->load()));

    if (! outputGain.isSmoothing())
    {
        buffer.applyGain (outputGain.getTargetValue());
        return;
    }

    const auto numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = outputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

juce::AudioProcessorEditor* CruzEQAudioProcessor::createEditor()
{
    return new CruzEQAudioProcessorEditor (*this);
}

void CruzEQAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzEQAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzEQAudioProcessor();
}
