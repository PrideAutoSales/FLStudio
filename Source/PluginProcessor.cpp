#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzEQAudioProcessor::CruzEQAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzEQ", Params::createLayout())
{
    for (int b = 0; b < Params::numBands; ++b)
        bands[(size_t) b] = Params::BandRefs::fromState (state, b);

    outputGainDb = state.getRawParameterValue (Params::outputGain);
}

void CruzEQAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    eq.prepare (sampleRate);
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

    for (int b = 0; b < Params::numBands; ++b)
    {
        const auto& band = bands[(size_t) b];
        eq.setBand (b, { band.isActive(), band.read() });
    }

    preEqFifo.push (buffer);
    eq.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (outputGainDb->load()));

    if (outputGain.isSmoothing())
    {
        const auto numChannels = buffer.getNumChannels();

        for (int i = 0; i < numSamples; ++i)
        {
            const auto g = outputGain.getNextValue();

            for (int ch = 0; ch < numChannels; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }
    else
    {
        buffer.applyGain (outputGain.getTargetValue());
    }

    postEqFifo.push (buffer);
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
