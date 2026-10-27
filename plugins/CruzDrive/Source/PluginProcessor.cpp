#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        float drive;
        int style;
        float lowCut, tone, highCut;
        bool thump, steep, punish;
        float crush, darkness, mix, output;
    };

    // Starting points that show off each part of the plugin.
    const Preset presets[] {
        { "Default",            3.0f, 0,  10.0f,  0.0f, 22000.0f, false, false, false,  0.0f,  0.0f, 100.0f,  0.0f },
        { "Vocal Warmth",       3.5f, 0,  80.0f,  0.1f, 16000.0f, false, false, false,  0.0f,  0.0f,  60.0f,  0.0f },
        { "Bright Vocal Edge",  4.0f, 1, 120.0f,  0.3f, 22000.0f, false, false, false,  0.0f,  0.0f,  45.0f,  0.0f },
        { "Bass Iron",          5.0f, 2,  30.0f, -0.2f,  9000.0f, true,  true,  false,  0.0f,  0.0f, 100.0f,  0.0f },
        { "Drum Bus Glue",      4.0f, 3,  10.0f,  0.0f, 18000.0f, false, false, false, 25.0f,  0.0f,  50.0f,  0.0f },
        { "Snare Crack",        6.5f, 1, 150.0f,  0.4f, 22000.0f, false, false, false, 20.0f,  0.0f,  70.0f,  0.0f },
        { "Smashed Room",       6.0f, 3, 100.0f,  0.0f, 22000.0f, false, false, false, 85.0f, 45.0f, 100.0f,  0.0f },
        { "Dark Crush",         5.0f, 2,  60.0f, -0.4f, 22000.0f, true,  false, false, 70.0f, 70.0f, 100.0f,  0.0f },
        { "Fuzz Guitar",        7.0f, 4, 120.0f,  0.2f,  7000.0f, false, true,  false,  0.0f,  0.0f, 100.0f,  0.0f },
        { "Punished Synth",     6.0f, 4,  60.0f,  0.0f, 12000.0f, false, true,  true,  10.0f, 20.0f, 100.0f, -3.0f },
        { "Parallel Grit",      8.0f, 3,  90.0f,  0.2f, 14000.0f, false, false, true,  40.0f,  0.0f,  30.0f,  0.0f },
    };

    juce::String formatHz (float hz)
    {
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                             : juce::String (juce::roundToInt (hz)) + " Hz";
    }
}

CruzDriveProcessor::CruzDriveProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "CruzDrive", createParameterLayout())
{
    raw.drive = state.getRawParameterValue (DriveParams::drive);
    raw.style = state.getRawParameterValue (DriveParams::style);
    raw.lowCut = state.getRawParameterValue (DriveParams::lowCut);
    raw.thump = state.getRawParameterValue (DriveParams::thump);
    raw.tone = state.getRawParameterValue (DriveParams::tone);
    raw.highCut = state.getRawParameterValue (DriveParams::highCut);
    raw.steep = state.getRawParameterValue (DriveParams::steep);
    raw.punish = state.getRawParameterValue (DriveParams::punish);
    raw.crush = state.getRawParameterValue (DriveParams::crush);
    raw.darkness = state.getRawParameterValue (DriveParams::darkness);
    raw.autoGain = state.getRawParameterValue (DriveParams::autoGain);
    raw.mix = state.getRawParameterValue (DriveParams::mix);
    raw.output = state.getRawParameterValue (DriveParams::output);
}

juce::AudioProcessorValueTreeState::ParameterLayout CruzDriveProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto percent = AudioParameterFloatAttributes()
                       .withLabel ("%")
                       .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "%"; });

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::drive, 1 }, "Drive",
        NormalisableRange<float> { 0.0f, 10.0f, 0.01f }, 3.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1); })));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { DriveParams::style, 1 }, "Style", cruzdrive::styleNames, 0));

    NormalisableRange<float> lowCutRange { 10.0f, 1000.0f, 0.1f };
    lowCutRange.setSkewForCentre (100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::lowCut, 1 }, "Low Cut", lowCutRange, 10.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
        {
            return v <= cruzdrive::lowCutOffHz ? String ("Off") : formatHz (v);
        })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { DriveParams::thump, 1 }, "Thump", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::tone, 1 }, "Tone",
        NormalisableRange<float> { -1.0f, 1.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const auto amount = roundToInt (std::abs (v) * 100.0f);
            if (amount == 0) return String ("Flat");
            return String (v < 0 ? "Dark " : "Bright ") + String (amount);
        })));

    NormalisableRange<float> highCutRange { 1000.0f, 22000.0f, 1.0f };
    highCutRange.setSkewForCentre (6000.0f);
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::highCut, 1 }, "High Cut", highCutRange, 22000.0f,
        AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction ([] (float v, int)
        {
            return v >= cruzdrive::highCutOffHz ? String ("Off") : formatHz (v);
        })));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { DriveParams::steep, 1 }, "Steep", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { DriveParams::punish, 1 }, "Punish", false));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::crush, 1 }, "Crush",
        NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::darkness, 1 }, "Darkness",
        NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 0.0f, percent));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { DriveParams::autoGain, 1 }, "Auto Gain", true));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::mix, 1 }, "Mix",
        NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f, percent));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { DriveParams::output, 1 }, "Output",
        NormalisableRange<float> { -24.0f, 12.0f, 0.01f }, 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction ([] (float v, int)
        {
            return String (v, 1) + " dB";
        })));

    return layout;
}

cruzdrive::Settings CruzDriveProcessor::readSettings() const
{
    cruzdrive::Settings s;
    s.driveDb = raw.drive->load() * 3.6f;
    s.style = juce::roundToInt (raw.style->load());
    s.lowCutHz = raw.lowCut->load();
    s.thump = raw.thump->load() > 0.5f;
    s.tone = raw.tone->load();
    s.highCutHz = raw.highCut->load();
    s.steep = raw.steep->load() > 0.5f;
    s.punish = raw.punish->load() > 0.5f;
    s.crush = raw.crush->load() * 0.01f;
    s.darkness = raw.darkness->load() * 0.01f;
    s.autoGain = raw.autoGain->load() > 0.5f;
    s.mix = raw.mix->load() * 0.01f;
    s.outputDb = raw.output->load();
    return s;
}

void CruzDriveProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.setSettings (readSettings());
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    setLatencySamples (engine.getLatencySamples());
}

bool CruzDriveProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void CruzDriveProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    engine.setSettings (readSettings());
    engine.process (juce::dsp::AudioBlock<float> (buffer));
}

int CruzDriveProcessor::getNumPrograms()
{
    return (int) std::size (presets);
}

const juce::String CruzDriveProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, getNumPrograms()) ? presets[index].name : "";
}

void CruzDriveProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPrograms()))
        return;

    currentProgram = index;
    const auto& p = presets[index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = state.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set (DriveParams::drive, p.drive);
    set (DriveParams::style, (float) p.style);
    set (DriveParams::lowCut, p.lowCut);
    set (DriveParams::thump, p.thump ? 1.0f : 0.0f);
    set (DriveParams::tone, p.tone);
    set (DriveParams::highCut, p.highCut);
    set (DriveParams::steep, p.steep ? 1.0f : 0.0f);
    set (DriveParams::punish, p.punish ? 1.0f : 0.0f);
    set (DriveParams::crush, p.crush);
    set (DriveParams::darkness, p.darkness);
    set (DriveParams::autoGain, 1.0f);
    set (DriveParams::mix, p.mix);
    set (DriveParams::output, p.output);
}

juce::AudioProcessorEditor* CruzDriveProcessor::createEditor()
{
    return new CruzDriveEditor (*this);
}

void CruzDriveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = state.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzDriveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
            state.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzDriveProcessor();
}
