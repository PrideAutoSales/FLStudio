#include "PluginProcessor.h"
#include "PluginEditor.h"

CruzSidechainAudioProcessor::CruzSidechainAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzSidechain", createParameterLayout())
{
    frames.resize ((size_t) fifo.getTotalSize());
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzSidechainAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto decibels = [] (const char* id, const char* name, float min, float max, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> { min, max, 0.1f }, def,
            AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; }));
    };

    auto millis = [] (const char* id, const char* name, float min, float max, float centre, float def)
    {
        NormalisableRange<float> range { min, max, 0.01f };
        range.setSkewForCentre (centre);
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, range, def,
            AudioParameterFloatAttributes().withLabel ("ms")
                .withStringFromValueFunction ([] (float v, int)
                {
                    if (v < 10.0f) return String (v, 2) + " ms";
                    if (v < 1000.0f) return String (roundToInt (v)) + " ms";
                    return String (v / 1000.0f, 2) + " s";
                }));
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

    auto percent = [] (const char* id, const char* name, float min, float max, float def)
    {
        return std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> { min, max, 1.0f }, def,
            AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; }));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::trigger, 1 }, "Trigger",
                                                        StringArray { "Sidechain", "Self", "Tempo" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::rate, 1 }, "Tempo Rate",
                                                        TempoRates::names(), 2));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::detect, 1 }, "Detect",
                                                        StringArray { "Peak", "RMS" }, 0));

    layout.add (decibels (ParamIDs::threshold, "Threshold", -60.0f, 0.0f, -30.0f));

    NormalisableRange<float> ratioRange { 1.0f, cruzsc::infiniteRatio, 0.01f };
    ratioRange.setSkewForCentre (4.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { ParamIDs::ratio, 1 }, "Ratio", ratioRange, cruzsc::infiniteRatio,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            return v >= cruzsc::infiniteRatio ? String (CharPointer_UTF8 ("\xe2\x88\x9e : 1")) : String (v, 1) + " : 1";
        })));

    layout.add (decibels (ParamIDs::knee, "Knee", 0.0f, 24.0f, 6.0f));
    layout.add (decibels (ParamIDs::range, "Range", 0.0f, 60.0f, 24.0f));
    layout.add (millis (ParamIDs::attack, "Attack", 0.0f, 100.0f, 10.0f, 1.0f));
    layout.add (millis (ParamIDs::hold, "Hold", 0.0f, 500.0f, 60.0f, 15.0f));
    layout.add (millis (ParamIDs::release, "Release", 5.0f, 2000.0f, 200.0f, 160.0f));
    layout.add (percent (ParamIDs::attackBend, "Attack Bend", -100.0f, 100.0f, 50.0f));
    layout.add (percent (ParamIDs::releaseBend, "Release Bend", -100.0f, 100.0f, 30.0f));
    layout.add (millis (ParamIDs::lookahead, "Lookahead", 0.0f, cruzsc::maxLookaheadMs, 2.0f, 0.0f));
    layout.add (decibels (ParamIDs::keyGain, "Trigger Gain", -24.0f, 24.0f, 0.0f));
    layout.add (hertz (ParamIDs::keyHighPass, "Trigger High-Pass", 20.0f, 1000.0f, 20.0f));
    layout.add (hertz (ParamIDs::keyLowPass, "Trigger Low-Pass", 60.0f, 20000.0f, 20000.0f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::splitLows, 1 }, "Duck Lows Only", false));
    layout.add (hertz (ParamIDs::splitFreq, "Split Frequency", 40.0f, 500.0f, 150.0f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::listen, 1 }, "Listen to Trigger", false));
    layout.add (percent (ParamIDs::mix, "Mix", 0.0f, 100.0f, 100.0f));
    layout.add (decibels (ParamIDs::output, "Output", -24.0f, 12.0f, 0.0f));

    return layout;
}

cruzsc::Params CruzSidechainAudioProcessor::readParams() const
{
    auto v = [this] (const char* id) { return state.getRawParameterValue (id)->load(); };

    cruzsc::Params p;
    p.thresholdDb = v (ParamIDs::threshold);
    p.ratio = v (ParamIDs::ratio);
    p.kneeDb = v (ParamIDs::knee);
    p.rangeDb = v (ParamIDs::range);
    p.attackMs = v (ParamIDs::attack);
    p.holdMs = v (ParamIDs::hold);
    p.releaseMs = v (ParamIDs::release);
    p.attackBend = v (ParamIDs::attackBend) / 100.0f;
    p.releaseBend = v (ParamIDs::releaseBend) / 100.0f;
    p.lookaheadMs = v (ParamIDs::lookahead);
    p.keyGainDb = v (ParamIDs::keyGain);
    p.keyHighPassHz = v (ParamIDs::keyHighPass);
    p.keyLowPassHz = v (ParamIDs::keyLowPass);
    p.detect = v (ParamIDs::detect) > 0.5f ? cruzsc::Detect::rms : cruzsc::Detect::peak;
    p.splitLows = v (ParamIDs::splitLows) > 0.5f;
    p.splitHz = v (ParamIDs::splitFreq);
    p.listenKey = v (ParamIDs::listen) > 0.5f;
    p.mix = v (ParamIDs::mix) / 100.0f;
    p.outputGain = juce::Decibels::decibelsToGain (v (ParamIDs::output));
    return p;
}

void CruzSidechainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRateNow = sampleRate;
    keyScratch.setSize (2, juce::jmax (512, samplesPerBlock));
    engine.setParams (readParams());
    engine.prepare ((float) sampleRate, samplesPerBlock);
    ghost.prepare ((float) sampleRate);
    setLatencySamples (engine.getLatencySamples());
    lastBeatIndex = -1;
    fifo.reset();
}

void CruzSidechainAudioProcessor::reset()
{
    engine.reset();
    ghost.reset();
    lastBeatIndex = -1;
}

bool CruzSidechainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (out != layouts.getMainInputChannelSet())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto& side = layouts.getChannelSet (true, 1);
        if (! side.isDisabled() && side != juce::AudioChannelSet::mono() && side != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void CruzSidechainAudioProcessor::makeGhostKicks (int numSamples)
{
    auto* left = keyScratch.getWritePointer (0);
    auto* right = keyScratch.getWritePointer (1);

    bool playing = false;
    double ppq = 0.0, bpm = hostBpm.load();

    if (auto* head = getPlayHead())
    {
        if (auto pos = head->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 1.0)
                bpm = *b;
            if (auto q = pos->getPpqPosition(); q.hasValue())
            {
                ppq = *q;
                playing = pos->getIsPlaying();
            }
        }
    }

    hostBpm.store ((float) bpm);
    hostPlaying.store (playing);

    const auto rateIndex = juce::jlimit (0, (int) std::size (TempoRates::beats) - 1,
                                         (int) state.getRawParameterValue (ParamIDs::rate)->load());
    const double length = TempoRates::beats[rateIndex];
    const double ppqPerSample = bpm / 60.0 / sampleRateNow;

    if (! playing)
        lastBeatIndex = -1;

    for (int i = 0; i < numSamples; ++i)
    {
        if (playing)
        {
            // Small offset so a hit that lands exactly on the grid is not missed to rounding.
            const auto index = (juce::int64) std::floor ((ppq + i * ppqPerSample) / length + 1.0e-9);
            if (index != lastBeatIndex)
            {
                const double into = (ppq + i * ppqPerSample) - (double) index * length;
                if (lastBeatIndex >= 0 || into < ppqPerSample * 2.0)
                    ghost.trigger();
                lastBeatIndex = index;
            }
        }

        left[i] = right[i] = ghost.next();
    }
}

void CruzSidechainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    auto main = getBusBuffer (buffer, true, 0);
    const int mainChannels = juce::jmin (2, main.getNumChannels());

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (mainChannels == 0 || numSamples == 0)
        return;

    engine.setParams (readParams());
    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples (engine.getLatencySamples());

    const auto source = (TriggerSource) juce::jlimit (0, 2, (int) state.getRawParameterValue (ParamIDs::trigger)->load());

    // Keep the play position and tempo fresh in every mode, for the display.
    if (source != TriggerSource::tempo)
        if (auto* head = getPlayHead())
            if (auto pos = head->getPosition())
            {
                if (auto b = pos->getBpm(); b.hasValue() && *b > 1.0)
                    hostBpm.store ((float) *b);
                hostPlaying.store (pos->getIsPlaying());
            }

    // The sidechain bus, when the host gives it to us.
    int sideChannels = 0;
    juce::AudioBuffer<float> side;
    if (getBusCount (true) > 1 && getBus (true, 1)->isEnabled())
    {
        side = getBusBuffer (buffer, true, 1);
        sideChannels = juce::jmin (2, side.getNumChannels());
    }
    sidechainConnected.store (sideChannels > 0);

    if (sideChannels > 0)
    {
        float peak = 0.0f;
        for (int c = 0; c < sideChannels; ++c)
            peak = juce::jmax (peak, side.getMagnitude (c, 0, numSamples));

        silentSamples = peak > 1.0e-4f ? 0 : juce::jmin (silentSamples + numSamples, 1 << 30);
        sidechainSignal.store (silentSamples < (int) sampleRateNow);
    }
    else
    {
        sidechainSignal.store (false);
    }

    auto sink = [this] (const cruzsc::Frame& f)
    {
        const auto scope = fifo.write (1);
        if (scope.blockSize1 > 0)
            frames[(size_t) scope.startIndex1] = f;
        else if (scope.blockSize2 > 0)
            frames[(size_t) scope.startIndex2] = f;
    };

    // Work in chunks no longer than the key scratch buffer.
    for (int start = 0; start < numSamples; start += keyScratch.getNumSamples())
    {
        const int len = juce::jmin (keyScratch.getNumSamples(), numSamples - start);
        float* io[2] = { main.getWritePointer (0, start), main.getWritePointer (mainChannels > 1 ? 1 : 0, start) };
        const float* key[2] = { nullptr, nullptr };
        int keyChannels = 0;

        if (source == TriggerSource::sidechain && sideChannels > 0)
        {
            key[0] = side.getReadPointer (0, start);
            key[1] = side.getReadPointer (sideChannels > 1 ? 1 : 0, start);
            keyChannels = sideChannels;
        }
        else if (source == TriggerSource::self)
        {
            for (int c = 0; c < 2; ++c)
                keyScratch.copyFrom (c, 0, main, juce::jmin (c, mainChannels - 1), start, len);
            key[0] = keyScratch.getReadPointer (0);
            key[1] = keyScratch.getReadPointer (1);
            keyChannels = mainChannels;
        }
        else if (source == TriggerSource::tempo)
        {
            makeGhostKicks (len);
            key[0] = keyScratch.getReadPointer (0);
            key[1] = keyScratch.getReadPointer (1);
            keyChannels = 2;
        }

        engine.process (io, mainChannels, keyChannels > 0 ? key : nullptr, keyChannels, len, sink);
    }

    keyLevelDb.store (engine.getKeyLevelDb());
    grDb.store (engine.getGainReductionDb());
}

int CruzSidechainAudioProcessor::popFrames (cruzsc::Frame* dest, int maxFrames)
{
    const auto scope = fifo.read (juce::jmin (maxFrames, fifo.getNumReady()));

    for (int i = 0; i < scope.blockSize1; ++i)
        dest[i] = frames[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i)
        dest[scope.blockSize1 + i] = frames[(size_t) (scope.startIndex2 + i)];

    return scope.blockSize1 + scope.blockSize2;
}

juce::AudioProcessorEditor* CruzSidechainAudioProcessor::createEditor()
{
    return new CruzSidechainAudioProcessorEditor (*this);
}

void CruzSidechainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzSidechainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzSidechainAudioProcessor();
}
