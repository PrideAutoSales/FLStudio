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
    autoGainOn = state.getRawParameterValue (Params::autoGain);
    gainScale = state.getRawParameterValue (Params::gainScale);
}

void CruzEQAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    eq.prepare (sampleRate);
    updateTargets();
    autoGainDirty = true;
    autoGainCountdown = 0;
    outputGain.reset (sampleRate, 0.05);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (totalOutputGainDb (0)));
}

void CruzEQAudioProcessor::updateTargets() noexcept
{
    using cruz::dsp::exactlyEqual;
    const auto scale = gainScale->load() / 100.0f;

    for (int b = 0; b < Params::numBands; ++b)
    {
        const auto& band = bands[(size_t) b];
        cruz::dsp::BandTarget t { band.isActive(), band.read(), band.placement(), band.dynamics() };
        t.params.gainDb *= scale;
        t.dynamics.rangeDb *= scale;

        auto& old = targets[(size_t) b];
        const bool changed = t.active != old.active || t.placement != old.placement
                          || t.params.shape != old.params.shape || t.params.slopeDbPerOct != old.params.slopeDbPerOct
                          || ! exactlyEqual (t.params.freqHz, old.params.freqHz) || ! exactlyEqual (t.params.gainDb, old.params.gainDb)
                          || ! exactlyEqual (t.params.q, old.params.q);

        // Dynamics settings do not move the auto gain estimate.
        if (changed)
            autoGainDirty = true;

        old = t;
    }
}

float CruzEQAudioProcessor::totalOutputGainDb (int numSamples) noexcept
{
    float autoDb = 0.0f;

    if (autoGainOn->load() >= 0.5f)
    {
        // Measuring the curve costs a little, so while bands move it is redone at most every 20 ms.
        autoGainCountdown -= numSamples;

        if (autoGainDirty && autoGainCountdown <= 0)
        {
            autoGainEstimateDb = cruz::dsp::averageGainDb (targets, currentSampleRate.load());
            autoGainDirty = false;
            autoGainCountdown = (int) (0.02 * currentSampleRate.load());
        }

        autoDb = (float) juce::jlimit (-24.0, 24.0, -autoGainEstimateDb);
    }

    autoGainDb = autoDb;
    return outputGainDb->load() + autoDb;
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

    updateTargets();

    for (int b = 0; b < Params::numBands; ++b)
        eq.setBand (b, targets[(size_t) b]);

    preEqFifo.push (buffer);
    eq.process (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), numSamples);

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (totalOutputGainDb (numSamples)));

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

    for (int b = 0; b < Params::numBands; ++b)
        dynamicGainDb[(size_t) b] = (float) eq.currentDynamicGainDb (b);
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
