#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzFETAudioProcessor::CruzFETAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzFET", createParameterLayout())
{
    input = state.getRawParameterValue (ParamIDs::input);
    output = state.getRawParameterValue (ParamIDs::output);
    attack = state.getRawParameterValue (ParamIDs::attack);
    release = state.getRawParameterValue (ParamIDs::release);
    ratio = state.getRawParameterValue (ParamIDs::ratio);
    mix = state.getRawParameterValue (ParamIDs::mix);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzFETAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto db = AudioParameterFloatAttributes().withLabel ("dB");
    auto knob = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); });

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::input, 1 }, "Input",
                                                       NormalisableRange<float> (-24.0f, 36.0f, 0.1f), 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::output, 1 }, "Output",
                                                       NormalisableRange<float> (-36.0f, 24.0f, 0.1f), 0.0f, db));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::attack, 1 }, "Attack",
                                                       NormalisableRange<float> (1.0f, 7.0f, 0.01f), 3.0f, knob));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::release, 1 }, "Release",
                                                       NormalisableRange<float> (1.0f, 7.0f, 0.01f), 5.0f, knob));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::ratio, 1 }, "Ratio",
                                                        StringArray { "4:1", "8:1", "12:1", "20:1", "All" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::mix, 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::meter, 1 }, "Meter",
                                                        StringArray { "Gain Reduction", "Output" }, 0));
    return layout;
}

void CruzFETAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = (size_t) juce::jmax (1, getTotalNumOutputChannels());

    // 2x oversampling keeps the very fast envelope and the saturation free of aliasing.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        numChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) samplesPerBlock);
    oversampling->reset();

    const int latency = (int) oversampling->getLatencyInSamples();
    setLatencySamples (latency);

    model.prepare (sampleRate * 2.0);

    dryWet.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels });
    dryWet.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    dryWet.setWetLatency ((float) latency);
    dryWet.reset();

    inputGain.reset (sampleRate, 0.02);
    outputGain.reset (sampleRate, 0.02);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (input->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (output->load()));
}

bool CruzFETAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CruzFETAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    cruz::FetModel::Settings s;
    s.attack = attack->load();
    s.release = release->load();
    s.ratio = (int) std::lround (ratio->load());
    model.setSettings (s);

    juce::dsp::AudioBlock<float> block (buffer);
    dryWet.setWetMixProportion (mix->load() * 0.01f);
    dryWet.pushDrySamples (block);

    inputGain.setTargetValue (juce::Decibels::decibelsToGain (input->load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (output->load()));
    inputGain.applyGain (buffer, numSamples);

    auto up = oversampling->processSamplesUp (block);
    float* channels[2] = { up.getChannelPointer (0), up.getNumChannels() > 1 ? up.getChannelPointer (1) : nullptr };
    const float gr = model.process (channels, (int) up.getNumChannels(), (int) up.getNumSamples());
    oversampling->processSamplesDown (block);

    outputGain.applyGain (buffer, numSamples);
    dryWet.mixWetSamples (block);

    gainReductionDb.store (gr);
    float peak = 0.0f;
    for (int ch = 0; ch < numOut; ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, numSamples));
    outputDb.store (juce::Decibels::gainToDecibels (peak, -100.0f));
}

juce::AudioProcessorEditor* CruzFETAudioProcessor::createEditor()
{
    return new CruzFETAudioProcessorEditor (*this);
}

void CruzFETAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzFETAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzFETAudioProcessor();
}
