#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzTapeAudioProcessor::CruzTapeAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzTape", createParameterLayout())
{
    raw.input = state.getRawParameterValue (ParamIDs::input);
    raw.output = state.getRawParameterValue (ParamIDs::output);
    raw.autoGain = state.getRawParameterValue (ParamIDs::autoGain);
    raw.speed = state.getRawParameterValue (ParamIDs::speed);
    raw.formula = state.getRawParameterValue (ParamIDs::formula);
    raw.calibration = state.getRawParameterValue (ParamIDs::calibration);
    raw.emphasis = state.getRawParameterValue (ParamIDs::emphasis);
    raw.bias = state.getRawParameterValue (ParamIDs::bias);
    raw.wow = state.getRawParameterValue (ParamIDs::wow);
    raw.flutter = state.getRawParameterValue (ParamIDs::flutter);
    raw.hiss = state.getRawParameterValue (ParamIDs::hiss);
    raw.mix = state.getRawParameterValue (ParamIDs::mix);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzTapeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto db = juce::AudioParameterFloatAttributes().withLabel ("dB");
    const auto percent = juce::AudioParameterFloatAttributes().withLabel ("%");

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::input, 1 }, "Input",
        juce::NormalisableRange<float> { -18.0f, 18.0f, 0.01f }, 0.0f, db));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::output, 1 }, "Output",
        juce::NormalisableRange<float> { -18.0f, 18.0f, 0.01f }, 0.0f, db));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::autoGain, 1 }, "Auto Gain", true));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::speed, 1 }, "Tape Speed",
        juce::StringArray { "7.5 ips", "15 ips", "30 ips" }, 1));

    juce::StringArray formulaNames;
    for (const auto& f : cruztape::formulas)
        formulaNames.add (f.name);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::formula, 1 }, "Tape Type", formulaNames, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::calibration, 1 }, "Calibration",
        juce::StringArray { "+3 dB", "+6 dB", "+9 dB" }, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::emphasis, 1 }, "EQ Curve",
        juce::StringArray { "NAB", "CCIR" }, 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::bias, 1 }, "Bias",
        juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::wow, 1 }, "Wow",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::flutter, 1 }, "Flutter",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::hiss, 1 }, "Hiss",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f, percent));

    return layout;
}

cruztape::Settings CruzTapeAudioProcessor::readSettings() const
{
    cruztape::Settings s;
    s.inputDb = raw.input->load();
    s.outputDb = raw.output->load();
    s.autoGain = raw.autoGain->load() > 0.5f;
    s.speed = juce::roundToInt (raw.speed->load());
    s.formula = juce::roundToInt (raw.formula->load());
    s.calibration = juce::roundToInt (raw.calibration->load());
    s.emphasis = juce::roundToInt (raw.emphasis->load());
    s.bias = raw.bias->load() / 100.0;
    s.wow = raw.wow->load() / 100.0;
    s.flutter = raw.flutter->load() / 100.0;
    s.hiss = raw.hiss->load() / 100.0;
    s.mix = raw.mix->load() / 100.0;
    return s;
}

void CruzTapeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = juce::jmax (1, getTotalNumOutputChannels());
    maxBlockSize = juce::jmax (1, samplesPerBlock);

    // 4x oversampling at normal rates, 2x at 88.2 kHz and above.
    const auto order = sampleRate < 60000.0 ? 2u : 1u;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) numChannels, order, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing ((size_t) maxBlockSize);

    const auto oversampledRate = sampleRate * (double) oversampling->getOversamplingFactor();

    for (auto& c : channels)
        c.prepare (sampleRate, oversampledRate);

    transport.prepare (sampleRate);

    for (auto& d : wowDelays)
    {
        d.prepare (transport.maxDelaySamples());
        d.reset();
    }

    latency = juce::roundToInt (oversampling->getLatencyInSamples()) + transport.centreDelaySamples();
    setLatencySamples (latency);

    dryDelay.setSize (numChannels, latency + 1);
    dryDelay.clear();
    dryWritePos = 0;

    wetBuffer.setSize (numChannels, maxBlockSize);

    const auto s = readSettings();
    inputGain.reset (sampleRate, 0.03);
    outputGain.reset (sampleRate, 0.03);
    mixAmount.reset (sampleRate, 0.03);
    hissGain.reset (sampleRate, 0.03);
    wowAmount.reset (sampleRate, 0.1);
    flutterAmount.reset (sampleRate, 0.1);

    inputGain.setCurrentAndTargetValue (1.0f);
    outputGain.setCurrentAndTargetValue (1.0f);
    mixAmount.setCurrentAndTargetValue ((float) s.mix);
    hissGain.setCurrentAndTargetValue (0.0f);
    wowAmount.setCurrentAndTargetValue ((float) s.wow);
    flutterAmount.setCurrentAndTargetValue ((float) s.flutter);

    needsConfigure = true;
    // Snap the gains to their targets on the first block.
    lastSettings.inputDb = std::numeric_limits<double>::quiet_NaN();
}

void CruzTapeAudioProcessor::reset()
{
    for (auto& c : channels)
        c.reset();

    for (auto& d : wowDelays)
        d.reset();

    if (oversampling != nullptr)
        oversampling->reset();

    dryDelay.clear();
    dryWritePos = 0;
}

bool CruzTapeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void CruzTapeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (oversampling == nullptr)
        return;

    for (int start = 0; start < numSamples; start += maxBlockSize)
        processChunk (buffer, start, juce::jmin (maxBlockSize, numSamples - start));

    const auto numChannels = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < 2; ++ch)
        outputLevel[(size_t) ch].store (buffer.getRMSLevel (juce::jmin (ch, numChannels - 1), 0, numSamples));

    lastProcessTime.store (juce::Time::getMillisecondCounter());
}

void CruzTapeAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int numSamples)
{
    const auto numChannels = juce::jmin (buffer.getNumChannels(), wetBuffer.getNumChannels(), 2);
    const auto s = readSettings();

    const auto settingsChanged = needsConfigure
        || s.speed != lastSettings.speed || s.formula != lastSettings.formula
        || s.calibration != lastSettings.calibration || s.emphasis != lastSettings.emphasis
        || ! juce::exactlyEqual (s.bias, lastSettings.bias) || ! juce::exactlyEqual (s.hiss, lastSettings.hiss)
        || ! juce::exactlyEqual (s.outputDb, lastSettings.outputDb);

    const auto derived = cruztape::derive (s);

    if (settingsChanged)
    {
        for (auto& c : channels)
        {
            c.configure (s, derived.hfScale, derived.bumpScale);
            c.setHysteresis (derived.drive, derived.saturation, derived.width);
        }
    }

    // The tape sees the input level pushed by the calibration. On the way out
    // that push is undone, and with Auto Gain on so is the Input knob, so
    // turning up the Input adds saturation without getting louder.
    const auto calGain = cruztape::dbToGain (cruztape::calibrationDb (s.calibration));
    const auto inGainDb = cruztape::dbToGain (s.inputDb);
    const auto preTarget = (float) (inGainDb * calGain * cruztape::tapeReferenceGain);
    const auto postTarget = (float) (cruztape::dbToGain (s.outputDb)
                                     / (calGain * cruztape::tapeReferenceGain)
                                     / (s.autoGain ? inGainDb : 1.0));
    const auto hissTarget = (float) (derived.noiseGain * cruztape::dbToGain (s.outputDb));

    if (std::isnan (lastSettings.inputDb))
    {
        inputGain.setCurrentAndTargetValue (preTarget);
        outputGain.setCurrentAndTargetValue (postTarget);
        hissGain.setCurrentAndTargetValue (hissTarget);
    }
    else
    {
        inputGain.setTargetValue (preTarget);
        outputGain.setTargetValue (postTarget);
        hissGain.setTargetValue (hissTarget);
    }

    mixAmount.setTargetValue ((float) s.mix);
    wowAmount.setTargetValue ((float) s.wow);
    flutterAmount.setTargetValue ((float) s.flutter);

    lastSettings = s;
    needsConfigure = false;

    // Record side: input gain and pre-emphasis.
    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = inputGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            wetBuffer.setSample (ch, i, channels[(size_t) ch].preProcess (buffer.getSample (ch, start + i) * g));
    }

    // The tape itself, oversampled to keep the harmonics from aliasing.
    {
        auto block = juce::dsp::AudioBlock<float> (wetBuffer).getSubsetChannelBlock (0, (size_t) numChannels)
                                                             .getSubBlock (0, (size_t) numSamples);
        auto up = oversampling->processSamplesUp (block);

        for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        {
            auto* data = up.getChannelPointer (ch);
            auto& tape = channels[ch];

            for (size_t i = 0; i < up.getNumSamples(); ++i)
                data[i] = tape.hysteresisProcess (data[i]);
        }

        oversampling->processSamplesDown (block);
    }

    // Playback side: de-emphasis, head response, wow and flutter, hiss, and
    // finally the dry/wet mix against the latency matched dry signal.
    const auto speedIndex = juce::jlimit (0, 2, s.speed);
    const auto modScale = cruztape::speeds[speedIndex].modScale;
    const auto dryLength = dryDelay.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto delay = transport.next (wowAmount.getNextValue(), flutterAmount.getNextValue(), modScale);
        const auto post = outputGain.getNextValue();
        const auto hiss = hissGain.getNextValue();
        const auto m = mixAmount.getNextValue();
        const auto readPos = (dryWritePos + 1) % dryLength;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& tape = channels[(size_t) ch];
            auto wet = tape.postProcess (wetBuffer.getSample (ch, i));
            wet = wowDelays[(size_t) ch].process (wet, delay);
            wet = wet * post + tape.hissSample() * hiss;

            const auto in = buffer.getSample (ch, start + i);
            dryDelay.setSample (ch, dryWritePos, in);
            const auto dry = dryDelay.getSample (ch, readPos);

            buffer.setSample (ch, start + i, dry + m * (wet - dry));
        }

        dryWritePos = readPos;
    }
}

juce::AudioProcessorEditor* CruzTapeAudioProcessor::createEditor()
{
    return new CruzTapeAudioProcessorEditor (*this);
}

void CruzTapeAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzTapeAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzTapeAudioProcessor();
}
