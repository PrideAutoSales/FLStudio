#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::String formatDb (float value, int)       { return juce::String (value, 1) + " dB"; }
    juce::String formatMs (float value, int)       { return value < 10.0f ? juce::String (value, 2) + " ms" : juce::String (juce::roundToInt (value)) + " ms"; }
    juce::String formatPercent (float value, int)  { return juce::String (juce::roundToInt (value)) + "%"; }

    float parseNumber (const juce::String& text)    { return text.retainCharacters ("-0123456789.").getFloatValue(); }
}

CruzLimiterAudioProcessor::CruzLimiterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzLimiter", createParameterLayout())
{
    params.gain = state.getRawParameterValue (ParamIDs::gain);
    params.ceiling = state.getRawParameterValue (ParamIDs::ceiling);
    params.style = state.getRawParameterValue (ParamIDs::style);
    params.lookahead = state.getRawParameterValue (ParamIDs::lookahead);
    params.attack = state.getRawParameterValue (ParamIDs::attack);
    params.release = state.getRawParameterValue (ParamIDs::release);
    params.linkTransients = state.getRawParameterValue (ParamIDs::linkTransients);
    params.linkRelease = state.getRawParameterValue (ParamIDs::linkRelease);
    params.oversampling = state.getRawParameterValue (ParamIDs::oversampling);
    params.truePeak = state.getRawParameterValue (ParamIDs::truePeak);
    params.dcFilter = state.getRawParameterValue (ParamIDs::dcFilter);
    params.unityGain = state.getRawParameterValue (ParamIDs::unityGain);
    params.audition = state.getRawParameterValue (ParamIDs::audition);
    params.dither = state.getRawParameterValue (ParamIDs::dither);
    params.noiseShaping = state.getRawParameterValue (ParamIDs::noiseShaping);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzLimiterAudioProcessor::createParameterLayout()
{
    using Float = juce::AudioParameterFloat;
    using Attributes = juce::AudioParameterFloatAttributes;

    auto dbAttributes = Attributes().withStringFromValueFunction (formatDb).withValueFromStringFunction (parseNumber);
    auto msAttributes = Attributes().withStringFromValueFunction (formatMs).withValueFromStringFunction (parseNumber);
    auto percentAttributes = Attributes().withStringFromValueFunction (formatPercent).withValueFromStringFunction (parseNumber);

    juce::NormalisableRange<float> releaseRange (1.0f, 1000.0f, 0.1f);
    releaseRange.setSkewForCentre (80.0f);

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::gain, 1 }, "Gain",
                                         juce::NormalisableRange<float> (0.0f, 30.0f, 0.01f), 0.0f, dbAttributes));
    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::ceiling, 1 }, "Output Ceiling",
                                         juce::NormalisableRange<float> (-30.0f, 0.0f, 0.01f), -1.0f, dbAttributes));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::style, 1 }, "Style",
                                                              cruz::getStyleNames(), (int) cruz::LimiterStyle::allround));
    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::lookahead, 1 }, "Lookahead",
                                         juce::NormalisableRange<float> (0.0f, cruz::LimiterEngine::maxLookaheadMs, 0.01f), 1.0f, msAttributes));
    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::attack, 1 }, "Attack",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f, percentAttributes));
    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::release, 1 }, "Release",
                                         releaseRange, 100.0f, msAttributes));
    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::linkTransients, 1 }, "Link Transients",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 75.0f, percentAttributes));
    layout.add (std::make_unique<Float> (juce::ParameterID { ParamIDs::linkRelease, 1 }, "Link Release",
                                         juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, percentAttributes));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::oversampling, 1 }, "Oversampling",
                                                              cruz::getOversamplingNames(), 1));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::truePeak, 1 }, "True Peak", true));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::dcFilter, 1 }, "DC Filter", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::unityGain, 1 }, "Unity Gain", false));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::audition, 1 }, "Audition Limiting", false));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamIDs::dither, 1 }, "Dither",
                                                              juce::StringArray { "Off", "24 bit", "20 bit", "16 bit" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::noiseShaping, 1 }, "Noise Shaping", true));

    return layout;
}

bool CruzLimiterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

cruz::LimiterSettings CruzLimiterAudioProcessor::readSettings() const
{
    static constexpr int ditherBits[] { 0, 24, 20, 16 };

    cruz::LimiterSettings s;
    s.inputGainDb = params.gain->load();
    s.ceilingDb = params.ceiling->load();
    s.style = (cruz::LimiterStyle) juce::jlimit (0, cruz::getStyleNames().size() - 1, (int) params.style->load());
    s.lookaheadMs = params.lookahead->load();
    s.attackPercent = params.attack->load();
    s.releaseMs = params.release->load();
    s.linkTransientsPercent = params.linkTransients->load();
    s.linkReleasePercent = params.linkRelease->load();
    s.oversamplingIndex = (int) params.oversampling->load();
    s.truePeak = params.truePeak->load() > 0.5f;
    s.dcFilter = params.dcFilter->load() > 0.5f;
    s.unityGain = params.unityGain->load() > 0.5f;
    s.audition = params.audition->load() > 0.5f;
    s.ditherBits = ditherBits[juce::jlimit (0, 3, (int) params.dither->load())];
    s.noiseShaping = params.noiseShaping->load() > 0.5f;
    return s;
}

void CruzLimiterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto channels = juce::jmax (1, getTotalNumOutputChannels());
    blockLimit = juce::jmax (32, samplesPerBlock);

    engine.prepare (sampleRate, blockLimit, channels);
    engine.setSettings (readSettings());
    setLatencySamples (engine.getLatencySamples());

    loudness.prepare (sampleRate, channels);
    for (auto& m : truePeakMeters)
        m.reset();

    frameLength = juce::jmax (1, juce::roundToInt (sampleRate / framesPerSecond));
    frameCount = 0;
    frameInput = frameOutput = frameReduction = 0.0f;
}

void CruzLimiterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (readSettings());
    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples (engine.getLatencySamples());

    if (meterResetRequested.exchange (false))
    {
        for (auto& m : truePeakMeters)
            m.reset();
        truePeakMax = 0.0f;
    }

    const auto numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const auto numSamples = buffer.getNumSamples();
    float peaks[2] {};
    float blockReduction = 0.0f, blockTruePeak = 0.0f;

    // Hosts may send larger blocks than announced, so work in pieces the engine was prepared for.
    for (int start = 0; start < numSamples; start += blockLimit)
    {
        const auto length = juce::jmin (blockLimit, numSamples - start);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numChannels, start, length);
        engine.process (chunk);

        const auto* reduction = engine.getGainReduction();
        const auto* input = engine.getInputLevel();

        for (int i = 0; i < length; ++i)
        {
            float out = 0.0f;
            for (int c = 0; c < numChannels; ++c)
            {
                const auto s = chunk.getSample (c, i);
                const auto level = std::abs (s);
                peaks[c] = juce::jmax (peaks[c], level);
                out = juce::jmax (out, level);
                blockTruePeak = juce::jmax (blockTruePeak, truePeakMeters[(size_t) c].process (s));
            }

            frameInput = juce::jmax (frameInput, input[i]);
            frameOutput = juce::jmax (frameOutput, out);
            frameReduction = juce::jmax (frameReduction, reduction[i]);
            blockReduction = juce::jmax (blockReduction, reduction[i]);

            if (++frameCount >= frameLength)
                pushFrame();
        }
    }

    loudness.process (buffer.getArrayOfReadPointers(), numChannels, numSamples);

    for (int c = 0; c < 2; ++c)
    {
        const auto peak = numChannels == 1 ? peaks[0] : peaks[c];
        if (peak > outputPeaks[(size_t) c].load())
            outputPeaks[(size_t) c] = peak;
    }
    if (blockReduction > reductionPeak.load())
        reductionPeak = blockReduction;
    if (blockTruePeak > truePeakMax.load())
        truePeakMax = blockTruePeak;
}

void CruzLimiterAudioProcessor::pushFrame() noexcept
{
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)
        frames[(size_t) scope.startIndex1] = { juce::Decibels::gainToDecibels (frameInput, -100.0f),
                                               juce::Decibels::gainToDecibels (frameOutput, -100.0f),
                                               frameReduction };
    frameCount = 0;
    frameInput = frameOutput = frameReduction = 0.0f;
}

int CruzLimiterAudioProcessor::readFrames (MeterFrame* dest, int maxFrames)
{
    const auto scope = fifo.read (juce::jmin (maxFrames, fifo.getNumReady()));
    for (int i = 0; i < scope.blockSize1; ++i)
        dest[i] = frames[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i)
        dest[scope.blockSize1 + i] = frames[(size_t) (scope.startIndex2 + i)];
    return scope.blockSize1 + scope.blockSize2;
}

void CruzLimiterAudioProcessor::resetMeters() noexcept
{
    loudness.requestReset();
    meterResetRequested = true;
}

juce::AudioProcessorEditor* CruzLimiterAudioProcessor::createEditor()
{
    return new CruzLimiterAudioProcessorEditor (*this);
}

void CruzLimiterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzLimiterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzLimiterAudioProcessor();
}
