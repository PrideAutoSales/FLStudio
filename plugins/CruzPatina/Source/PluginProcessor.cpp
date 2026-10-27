#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values; // anything not listed uses its default
    };

    const std::vector<Preset>& getPresets()
    {
        using namespace ParamIDs;
        static const std::vector<Preset> presets {
            { "Init", {} },
            { "Dusty Sampler", { { noiseOn, 1 }, { noiseType, 1 }, { noiseAmount, 35 }, { noiseFollow, 30 },
                                 { wobbleWow, 15 }, { wobbleFlutter, 10 },
                                 { digitalOn, 1 }, { digitalMix, 100 }, { digitalRate, 26000 }, { digitalBits, 12 },
                                 { distortType, 1 }, { distortDrive, 25 }, { tone, -20 } } },
            { "Worn Cassette", { { noiseAmount, 40 }, { noiseTone, 45 },
                                 { wobbleWow, 45 }, { wobbleRate, 0.6f }, { wobbleFlutter, 35 }, { wobbleStereo, 30 },
                                 { distortType, 1 }, { distortDrive, 35 }, { distortTone, 60 },
                                 { magneticSaturation, 45 }, { magneticWear, 55 }, { magneticDropouts, 25 } } },
            { "Old Vinyl", { { noiseType, 1 }, { noiseAmount, 55 }, { noiseTone, 60 },
                             { wobbleWow, 20 }, { wobbleRate, 0.55f }, { wobbleFlutter, 5 },
                             { distortOn, 0 }, { magneticWear, 35 }, { width, 80 }, { tone, -15 } } },
            { "Broken Radio", { { noiseAmount, 30 }, { noiseTone, 80 },
                                { wobbleOn, 0 },
                                { distortType, 2 }, { distortDrive, 60 }, { distortTone, 15 },
                                { digitalOn, 1 }, { digitalMix, 70 }, { digitalRate, 11000 }, { digitalBits, 10 },
                                { magneticOn, 0 }, { width, 0 }, { tone, 40 } } },
            { "Lo-fi Keys", { { noiseAmount, 30 }, { noiseFollow, 50 },
                              { wobbleWow, 50 }, { wobbleRate, 1.1f }, { wobbleFlutter, 20 }, { wobbleStereo, 50 },
                              { spaceOn, 1 }, { spaceMix, 30 }, { spaceSize, 55 }, { spaceDecay, 45 },
                              { magneticWear, 40 } } },
            { "Crushed Drums", { { noiseOn, 0 }, { wobbleOn, 0 },
                                 { distortType, 3 }, { distortDrive, 55 }, { distortMix, 60 },
                                 { digitalOn, 1 }, { digitalMix, 80 }, { digitalRate, 16000 }, { digitalBits, 7 },
                                 { magneticOn, 0 } } },
            { "Seasick Tape", { { noiseAmount, 25 },
                                { wobbleWow, 90 }, { wobbleRate, 0.35f }, { wobbleFlutter, 50 }, { wobbleStereo, 70 },
                                { magneticSaturation, 50 }, { magneticWear, 60 }, { magneticDropouts, 45 } } },
            { "Distant Memory", { { noiseType, 1 }, { noiseAmount, 30 },
                                  { wobbleWow, 35 }, { wobbleRate, 0.5f },
                                  { spaceOn, 1 }, { spaceMix, 55 }, { spaceSize, 85 }, { spaceDecay, 75 },
                                  { magneticWear, 75 }, { tone, -40 }, { width, 120 } } },
        };
        return presets;
    }

    std::unique_ptr<juce::AudioParameterFloat> percent (const char* id, const char* name, float def)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, def,
            juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); }));
    }

    // An Off/On choice rather than AudioParameterBool: a bool keeps whatever raw value the host
    // sends (e.g. 0.54), so restoring a saved state could leave it reading as half on.
    std::unique_ptr<juce::AudioParameterChoice> toggle (const char* id, const char* name, bool def)
    {
        return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name,
                                                             juce::StringArray { "Off", "On" }, def ? 1 : 0);
    }

    std::unique_ptr<juce::AudioParameterFloat> decibels (const char* id, const char* name)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.01f }, 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB"));
    }
}

CruzPatinaAudioProcessor::CruzPatinaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzPatina", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
            params[withId->paramID] = state.getRawParameterValue (withId->paramID);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzPatinaAudioProcessor::createParameterLayout()
{
    using namespace ParamIDs;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (percent (magnitude, "Magnitude", 100.0f),
                decibels (inputGain, "Input Gain"),
                decibels (outputGain, "Output Gain"),
                percent (mix, "Mix", 100.0f),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { tone, 1 }, "Tone",
                    juce::NormalisableRange<float> { -100.0f, 100.0f, 0.1f }, 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return juce::String (juce::roundToInt (v)); })),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { width, 1 }, "Width",
                    juce::NormalisableRange<float> { 0.0f, 200.0f, 0.1f }, 100.0f,
                    juce::AudioParameterFloatAttributes()
                        .withLabel ("%")
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)); })));

    layout.add (toggle (noiseOn, "Noise On", true),
                percent (noiseAmount, "Noise Amount", 25.0f),
                std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { noiseType, 1 }, "Noise Type", getNoiseTypeNames(), 0),
                percent (noiseTone, "Noise Tone", 50.0f),
                percent (noiseFollow, "Noise Follow", 0.0f));

    juce::NormalisableRange<float> rateRange { 0.1f, 8.0f, 0.01f };
    rateRange.setSkewForCentre (1.0f);

    layout.add (toggle (wobbleOn, "Wobble On", true),
                percent (wobbleWow, "Wow", 30.0f),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { wobbleRate, 1 }, "Wow Rate", rateRange, 0.8f,
                    juce::AudioParameterFloatAttributes().withLabel ("Hz")),
                percent (wobbleFlutter, "Flutter", 15.0f),
                percent (wobbleStereo, "Wobble Stereo", 20.0f));

    layout.add (toggle (distortOn, "Distort On", true),
                percent (distortDrive, "Drive", 25.0f),
                std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { distortType, 1 }, "Distort Type", getDistortTypeNames(), 0),
                percent (distortTone, "Distort Tone", 75.0f),
                percent (distortMix, "Distort Mix", 100.0f));

    juce::NormalisableRange<float> srRange { 1000.0f, 48000.0f, 1.0f };
    srRange.setSkewForCentre (8000.0f);

    layout.add (toggle (digitalOn, "Digital On", false),
                percent (digitalMix, "Digital Mix", 100.0f),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { digitalRate, 1 }, "Sample Rate", srRange, 22000.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return juce::String (v / 1000.0f, 1) + " kHz"; })),
                std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { digitalBits, 1 }, "Bits",
                    juce::NormalisableRange<float> { 2.0f, 16.0f, 0.1f }, 12.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return juce::String (v, 1); })));

    layout.add (toggle (spaceOn, "Space On", false),
                percent (spaceMix, "Space Mix", 30.0f),
                percent (spaceSize, "Space Size", 50.0f),
                percent (spaceDecay, "Space Decay", 50.0f));

    layout.add (toggle (magneticOn, "Magnetic On", true),
                percent (magneticSaturation, "Saturation", 30.0f),
                percent (magneticWear, "Wear", 25.0f),
                percent (magneticDropouts, "Dropouts", 0.0f));

    return layout;
}

void CruzPatinaAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    noise.prepare (sampleRate);
    wobble.prepare (sampleRate);
    distort.prepare (sampleRate, maxChunk);
    digital.prepare (sampleRate);
    space.prepare (sampleRate);
    magnetic.prepare (sampleRate);

    const auto latency = wobble.getLatencySamples() + distort.getLatencySamples();
    dryDelay.prepare (latency);
    dryDelay.setDelay (latency);
    setLatencySamples (latency);

    dryBuffer.setSize (patina::maxChannels, maxChunk);

    for (auto* s : { &inputGainSmoothed, &outputGainSmoothed, &mixSmoothed, &widthSmoothed })
        s->reset (sampleRate, 0.03);

    inputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (get (ParamIDs::inputGain)));
    outputGainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (get (ParamIDs::outputGain)));
    mixSmoothed.setCurrentAndTargetValue (get (ParamIDs::mix) * 0.01f);
    widthSmoothed.setCurrentAndTargetValue (get (ParamIDs::width) * 0.01f);

    lastTilt = 1000.0f;
    updateTiltFilters();
    reset();
}

void CruzPatinaAudioProcessor::reset()
{
    noise.reset();
    wobble.reset();
    distort.reset();
    digital.reset();
    space.reset();
    magnetic.reset();
    dryDelay.reset();
    for (auto& f : tiltLow) f.reset();
    for (auto& f : tiltHigh) f.reset();
}

bool CruzPatinaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void CruzPatinaAudioProcessor::updateTiltFilters()
{
    const auto tilt = get (ParamIDs::tone) * 0.01f;
    if (std::abs (tilt - lastTilt) < 1.0e-4f)
        return;

    for (auto& f : tiltLow)
        f.setup (patina::Svf::Type::lowShelf, (float) sampleRate, 900.0f, 0.5f, -6.0f * tilt);
    for (auto& f : tiltHigh)
        f.setup (patina::Svf::Type::highShelf, (float) sampleRate, 900.0f, 0.5f, 6.0f * tilt);
    lastTilt = tilt;
}

void CruzPatinaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const auto numChannels = juce::jmin (buffer.getNumChannels(), patina::maxChannels);
    std::array<float*, patina::maxChannels> ptrs {};

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const auto n = juce::jmin (maxChunk, numSamples - start);
        for (int ch = 0; ch < numChannels; ++ch)
            ptrs[(size_t) ch] = buffer.getWritePointer (ch, start);
        processChunk (ptrs.data(), numChannels, n);
    }
}

void CruzPatinaAudioProcessor::processChunk (float* const* channels, int numChannels, int numSamples)
{
    using namespace ParamIDs;

    // Magnitude scales the intensity of every module at once.
    const auto mag = get (magnitude) * 0.01f;

    inputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (get (ParamIDs::inputGain)));
    outputGainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (get (ParamIDs::outputGain)));
    mixSmoothed.setTargetValue (get (ParamIDs::mix) * 0.01f);
    widthSmoothed.setTargetValue (get (ParamIDs::width) * 0.01f);
    updateTiltFilters();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto g = inputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] *= g;
    }

    std::array<float*, patina::maxChannels> dry {};
    for (int ch = 0; ch < numChannels; ++ch)
        dry[(size_t) ch] = dryBuffer.getWritePointer (ch);
    dryDelay.process (channels, dry.data(), numChannels, numSamples);

    noise.process (channels, numChannels, numSamples,
                   { isOn (noiseOn), get (noiseAmount) * 0.01f * mag, (int) get (noiseType),
                     get (noiseTone) * 0.01f, get (noiseFollow) * 0.01f });

    wobble.process (channels, numChannels, numSamples,
                    { isOn (wobbleOn), get (wobbleWow) * 0.01f * mag, get (wobbleRate),
                      get (wobbleFlutter) * 0.01f * mag, get (wobbleStereo) * 0.01f });

    distort.process (juce::dsp::AudioBlock<float> (channels, (size_t) numChannels, (size_t) numSamples),
                     { isOn (distortOn), get (distortDrive) * 0.01f * mag, (int) get (distortType),
                       get (distortTone) * 0.01f, get (distortMix) * 0.01f });

    const auto bits = get (digitalBits);
    const auto rate = get (digitalRate);
    const auto nyquistRate = (float) sampleRate;
    digital.process (channels, numChannels, numSamples,
                     { isOn (digitalOn), get (digitalMix) * 0.01f,
                       nyquistRate * std::pow (rate / nyquistRate, mag),
                       16.0f - (16.0f - bits) * mag });

    space.process (channels, numChannels, numSamples,
                   { isOn (spaceOn), get (spaceMix) * 0.01f * mag, get (spaceSize) * 0.01f,
                     get (spaceDecay) * 0.01f });

    magnetic.process (channels, numChannels, numSamples,
                      { isOn (magneticOn), get (magneticSaturation) * 0.01f * mag,
                        get (magneticWear) * 0.01f * mag, get (magneticDropouts) * 0.01f * mag });

    for (int i = 0; i < numSamples; ++i)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = tiltHigh[(size_t) ch].process (tiltLow[(size_t) ch].process (channels[ch][i]));

        const auto w = widthSmoothed.getNextValue();
        if (numChannels == 2)
        {
            const auto mid = 0.5f * (channels[0][i] + channels[1][i]);
            const auto side = 0.5f * (channels[0][i] - channels[1][i]) * w;
            channels[0][i] = mid + side;
            channels[1][i] = mid - side;
        }

        const auto m = mixSmoothed.getNextValue();
        const auto out = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = (dry[(size_t) ch][i] + m * (channels[ch][i] - dry[(size_t) ch][i])) * out;
    }
}

int CruzPatinaAudioProcessor::getNumPrograms()
{
    return (int) getPresets().size();
}

const juce::String CruzPatinaAudioProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : "";
}

void CruzPatinaAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;

    for (auto* p : getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (ranged == nullptr)
            continue;

        auto value = ranged->getDefaultValue();
        for (const auto& [id, v] : presets[(size_t) index].values)
            if (ranged->paramID == id)
                value = ranged->convertTo0to1 (v);

        ranged->setValueNotifyingHost (value);
    }
}

juce::AudioProcessorEditor* CruzPatinaAudioProcessor::createEditor()
{
    return new CruzPatinaAudioProcessorEditor (*this);
}

void CruzPatinaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("program", currentProgram, nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzPatinaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram = tree.getProperty ("program", 0);
            state.replaceState (tree);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzPatinaAudioProcessor();
}
