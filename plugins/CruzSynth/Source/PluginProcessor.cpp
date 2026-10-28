#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "FactoryPresets.h"

using namespace cs;

namespace
{
    int factoryTableIndex (const juce::String& name)
    {
        return juce::jmax (0, tables::factoryNames().indexOf (name));
    }
}

CruzSynthProcessor::CruzSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CruzSynth", createParameterLayout()),
      engine (apvts, live, lfoTables),
      fx (apvts, live)
{
    for (int o = 0; o < kNumOscs; ++o)
        uiTables[(size_t) o] = audioTables[(size_t) o] = tables::makeFactory (0);
    engine.setTables (audioTables[0].get(), audioTables[1].get());

    for (int l = 0; l < kNumLfos; ++l)
        setLfoPoints (l, defaultLfoPoints());

    // Start on a plain saw, like most synths' init patch.
    setParam (cs::id::osc (0, "pos"), 2.0f / 3.0f);
    setParam (cs::id::osc (1, "pos"), 2.0f / 3.0f);
}

CruzSynthProcessor::~CruzSynthProcessor() = default;

bool CruzSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CruzSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    fx.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
}

void CruzSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Pick up any wavetable the editor swapped in.
    {
        const juce::SpinLock::ScopedTryLockType lock (tableLock);
        if (lock.isLocked())
        {
            for (size_t o = 0; o < (size_t) kNumOscs; ++o)
                if (pendingFlags[o])
                {
                    std::swap (audioTables[o], pendingTables[o]);
                    pendingFlags[o] = false;
                }
        }
    }
    engine.setTables (audioTables[0].get(), audioTables[1].get());

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    Engine::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) t.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) t.ppq = *ppq;
            t.playing = pos->getIsPlaying();
        }

    // Always render into a stereo scratch so a mono output still works.
    buffer.clear();
    juce::AudioBuffer<float> work;
    const bool stereo = buffer.getNumChannels() >= 2;
    juce::AudioBuffer<float>* target = &buffer;
    juce::HeapBlock<float> spare;
    if (! stereo)
    {
        spare.calloc ((size_t) buffer.getNumSamples());
        float* chans[2] = { buffer.getWritePointer (0), spare.get() };
        work.setDataToReferTo (chans, 2, buffer.getNumSamples());
        target = &work;
    }

    engine.render (*target, midi, t);
    fx.process (*target, t.bpm);

    if (! stereo)
        juce::FloatVectorOperations::add (buffer.getWritePointer (0), spare.get(), buffer.getNumSamples());

    // Scope and meter
    const int n = buffer.getNumSamples();
    const float* L = target->getReadPointer (0);
    const float* R = target->getReadPointer (1);
    int w = scopeWrite.load (std::memory_order_relaxed);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = 0.5f * (L[i] + R[i]);
        scope[(size_t) w] = m;
        w = (w + 1) % kScopeSize;
        peak = juce::jmax (peak, std::abs (L[i]), std::abs (R[i]));
    }
    scopeWrite.store (w, std::memory_order_relaxed);
    outputPeak.store (juce::jmax (peak, outputPeak.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    midi.clear();
}

void CruzSynthProcessor::copyScope (float* dest, int num) const
{
    num = juce::jmin (num, kScopeSize);
    int r = scopeWrite.load (std::memory_order_relaxed) - num;
    if (r < 0) r += kScopeSize;
    for (int i = 0; i < num; ++i)
        dest[i] = scope[(size_t) ((r + i) % kScopeSize)];
}

juce::AudioProcessorEditor* CruzSynthProcessor::createEditor()
{
    return new CruzSynthEditor (*this);
}

//==============================================================================
void CruzSynthProcessor::setTable (int osc, WavetablePtr table)
{
    if (table == nullptr)
        return;
    uiTables[(size_t) osc] = table;
    {
        const juce::SpinLock::ScopedLockType lock (tableLock);
        pendingTables[(size_t) osc] = table;
        pendingFlags[(size_t) osc] = true;
    }
    sendChangeMessageToEditor();
}

juce::String CruzSynthProcessor::defaultLfoPoints()
{
    return "0,0,0;0.5,1,0;1,0,0";
}

void CruzSynthProcessor::setLfoPoints (int lfo, const juce::String& points)
{
    lfoPoints[(size_t) lfo] = points;
    renderLfoTable (lfo);
    sendChangeMessageToEditor();
}

void CruzSynthProcessor::renderLfoTable (int lfo)
{
    struct P { float x, y, c; };
    std::vector<P> pts;
    for (auto& tok : juce::StringArray::fromTokens (lfoPoints[(size_t) lfo], ";", ""))
    {
        auto v = juce::StringArray::fromTokens (tok, ",", "");
        if (v.size() >= 2)
            pts.push_back ({ juce::jlimit (0.0f, 1.0f, v[0].getFloatValue()), juce::jlimit (0.0f, 1.0f, v[1].getFloatValue()),
                             v.size() > 2 ? juce::jlimit (-1.0f, 1.0f, v[2].getFloatValue()) : 0.0f });
    }
    std::sort (pts.begin(), pts.end(), [] (const P& a, const P& b) { return a.x < b.x; });
    if (pts.empty())
        pts = { { 0, 0, 0 }, { 1, 0, 0 } };
    if (pts.front().x > 0.0f) pts.insert (pts.begin(), { 0.0f, pts.front().y, 0.0f });
    if (pts.back().x < 1.0f)  pts.push_back ({ 1.0f, pts.back().y, 0.0f });

    auto& table = lfoTables[(size_t) lfo];
    size_t seg = 0;
    for (int i = 0; i < kLfoTableSize; ++i)
    {
        const float x = (float) i / (float) (kLfoTableSize - 1);
        while (seg + 2 < pts.size() && x > pts[seg + 1].x)
            ++seg;
        const auto& a = pts[seg];
        const auto& b = pts[juce::jmin (seg + 1, pts.size() - 1)];
        const float span = b.x - a.x;
        const float t = span > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (x - a.x) / span) : 1.0f;
        table[(size_t) i].store (a.y + (b.y - a.y) * dsp::bend (t, a.c * 6.0f), std::memory_order_relaxed);
    }
}

//==============================================================================
int CruzSynthProcessor::addModulation (int source, int destination, float amount)
{
    if (source <= 0 || destination <= 0)
        return -1;

    auto idx = [&] (int s, const char* n) { return apvts.getParameter (cs::id::slot (s, n)); };
    int free = -1;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = juce::roundToInt (apvts.getRawParameterValue (cs::id::slot (s, "src"))->load());
        const int dst = juce::roundToInt (apvts.getRawParameterValue (cs::id::slot (s, "dst"))->load());
        if (src == source && dst == destination)
            return s; // already routed
        if (free < 0 && (src == 0 || dst == 0))
            free = s;
    }
    if (free < 0)
        return -1;

    auto set = [&] (const char* n, float v)
    {
        auto* p = idx (free, n);
        p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    set ("src", (float) source);
    set ("dst", (float) destination);
    set ("amount", amount);
    set ("bipolar", 0.0f);
    return free;
}

void CruzSynthProcessor::clearModulationSlot (int slot)
{
    for (auto* n : { "src", "dst", "amount", "bipolar" })
    {
        auto* p = apvts.getParameter (cs::id::slot (slot, n));
        p->setValueNotifyingHost (p->getDefaultValue());
    }
}

juce::Array<int> CruzSynthProcessor::slotsForDestination (int destination) const
{
    juce::Array<int> r;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = juce::roundToInt (apvts.getRawParameterValue (cs::id::slot (s, "src"))->load());
        const int dst = juce::roundToInt (apvts.getRawParameterValue (cs::id::slot (s, "dst"))->load());
        if (src > 0 && dst == destination)
            r.add (s);
    }
    return r;
}

//==============================================================================
void CruzSynthProcessor::setParam (const juce::String& pid, float value)
{
    if (auto* p = apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    else
        jassertfalse;
}

void CruzSynthProcessor::resetParametersToDefault()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
}

juce::StringArray CruzSynthProcessor::getFactoryPresetNames() const
{
    juce::StringArray names { "Init" };
    for (int i = 0; i < numFactoryPresets(); ++i)
        names.add (factoryPresets()[i].name);
    return names;
}

juce::String CruzSynthProcessor::getFactoryPresetCategory (int index) const
{
    return juce::isPositiveAndBelow (index - 1, numFactoryPresets()) ? juce::String (factoryPresets()[index - 1].category)
                                                                      : juce::String();
}

int CruzSynthProcessor::getNumPrograms() { return numFactoryPresets() + 1; }

const juce::String CruzSynthProcessor::getProgramName (int index) { return getFactoryPresetNames()[index]; }

void CruzSynthProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        loadFactoryPreset (index);
}

void CruzSynthProcessor::loadFactoryPreset (int index)
{
    index = juce::jlimit (0, numFactoryPresets(), index);

    resetParametersToDefault();
    for (int o = 0; o < kNumOscs; ++o)
        setTable (o, tables::makeFactory (0));
    for (int l = 0; l < kNumLfos; ++l)
        setLfoPoints (l, defaultLfoPoints());
    fx.setOrder (FxRack::defaultOrder());

    if (index == 0)
    {
        setParam ("oscA_pos", 2.0f / 3.0f);
        setParam ("oscB_pos", 2.0f / 3.0f);
    }
    else
    {
        applyRecipe (factoryPresets()[index - 1].recipe);
    }

    currentProgram = index;
    presetName = getFactoryPresetNames()[index];
    sendChangeMessageToEditor();
}

void CruzSynthProcessor::applyRecipe (const juce::String& recipe)
{
    static const char* sources[] = { "None", "Env1", "Env2", "Env3", "Lfo1", "Lfo2", "Lfo3", "Lfo4", "Velocity",
                                     "NoteTrack", "ModWheel", "Aftertouch", "PitchBend", "Macro1", "Macro2",
                                     "Macro3", "Macro4", "NoteRandom" };
    static const char* dests[] = { "None", "OscACoarse", "OscAFine", "OscAPos", "OscADetune", "OscABlend",
                                   "OscAWarp", "OscAPhase", "OscAPan", "OscALevel", "OscBCoarse", "OscBFine",
                                   "OscBPos", "OscBDetune", "OscBBlend", "OscBWarp", "OscBPhase", "OscBPan",
                                   "OscBLevel", "SubLevel", "SubPan", "NoiseLevel", "NoisePan", "NoiseColor",
                                   "FltCutoff", "FltRes", "FltDrive", "FltMix", "Lfo1Rate", "Lfo2Rate",
                                   "Lfo3Rate", "Lfo4Rate", "MasterVol",
                                   "FxHypDetune", "FxHypMix", "FxHypWide", "FxDistDrive", "FxDistFreq", "FxDistMix",
                                   "FxFlaFeedback", "FxFlaMix", "FxPhaFreq", "FxPhaFeedback", "FxPhaMix",
                                   "FxChoDepth", "FxChoMix", "FxDlyFeedback", "FxDlyMix", "FxMbDepth",
                                   "FxEqLowGain", "FxEqHighGain", "FxFilCutoff", "FxFilRes", "FxFilDrive", "FxFilMix",
                                   "FxRevSize", "FxRevDecay", "FxRevMix" };
    static_assert (std::size (sources) == (size_t) kNumModSources);
    static_assert (std::size (dests) == (size_t) DstCount);

    auto indexIn = [] (const auto& list, const juce::String& name)
    {
        for (size_t i = 0; i < std::size (list); ++i)
            if (name == list[i])
                return (int) i;
        jassertfalse;
        return 0;
    };

    int nextSlot = 0;
    for (auto& token : juce::StringArray::fromTokens (recipe, " ", ""))
    {
        const auto key = token.upToFirstOccurrenceOf ("=", false, false);
        const auto value = token.fromFirstOccurrenceOf ("=", false, false);

        if (key == "A" || key == "B")
        {
            setTable (key == "A" ? 0 : 1, tables::makeFactory (factoryTableIndex (value.replaceCharacter ('_', ' '))));
        }
        else if (key == "m")
        {
            // Source>Dest:amount[:b]
            auto parts = juce::StringArray::fromTokens (value.fromFirstOccurrenceOf (">", false, false), ":", "");
            if (nextSlot >= kNumModSlots || parts.size() < 2)
                continue;
            setParam (cs::id::slot (nextSlot, "src"), (float) indexIn (sources, value.upToFirstOccurrenceOf (">", false, false)));
            setParam (cs::id::slot (nextSlot, "dst"), (float) indexIn (dests, parts[0]));
            setParam (cs::id::slot (nextSlot, "amount"), parts[1].getFloatValue());
            setParam (cs::id::slot (nextSlot, "bipolar"), parts.size() > 2 && parts[2] == "b" ? 1.0f : 0.0f);
            ++nextSlot;
        }
        else if (key.length() == 2 && key[0] == 'l')
        {
            setLfoPoints (key.getTrailingIntValue() - 1, value);
        }
        else if (key == "fx")
        {
            juce::Array<int> list;
            for (auto& item : juce::StringArray::fromTokens (value, ",", ""))
                list.add (item.getIntValue());
            fx.setOrder (FxRack::orderFromList (list));
        }
        else
        {
            setParam (key, value.getFloatValue());
        }
    }
}

//==============================================================================
juce::File CruzSynthProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Cruz Synth Presets");
}

bool CruzSynthProcessor::saveUserPreset (const juce::File& file)
{
    juce::MemoryBlock mb;
    getStateInformation (mb);
    if (auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize()))
    {
        presetName = file.getFileNameWithoutExtension();
        xml->setAttribute ("presetName", presetName);
        return xml->writeTo (file);
    }
    return false;
}

bool CruzSynthProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;
    juce::MemoryBlock mb;
    copyXmlToBinary (*xml, mb);
    setStateInformation (mb.getData(), (int) mb.getSize());
    presetName = file.getFileNameWithoutExtension();
    sendChangeMessageToEditor();
    return true;
}

void CruzSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("Extra"), nullptr);

    juce::ValueTree extra ("Extra");
    for (int o = 0; o < kNumOscs; ++o)
    {
        const auto& t = uiTables[(size_t) o];
        juce::ValueTree node ("Table");
        node.setProperty ("osc", o, nullptr);
        node.setProperty ("name", t->getName(), nullptr);
        node.setProperty ("factory", t->getFactoryIndex(), nullptr);
        if (t->getFactoryIndex() < 0)
        {
            const auto& raw = t->getRawFrames();
            juce::MemoryBlock mb (raw.data(), raw.size() * sizeof (float));
            node.setProperty ("data", mb.toBase64Encoding(), nullptr);
        }
        extra.appendChild (node, nullptr);
    }
    for (int l = 0; l < kNumLfos; ++l)
        extra.setProperty ("lfo" + juce::String (l + 1), lfoPoints[(size_t) l], nullptr);
    extra.setProperty ("fxOrder", FxRack::orderToString (fx.getOrder()), nullptr);
    extra.setProperty ("presetName", presetName, nullptr);
    state.appendChild (extra, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    auto extra = state.getChildWithName ("Extra");
    state.removeChild (extra, nullptr);
    apvts.replaceState (state);

    if (extra.isValid())
    {
        for (auto node : extra)
        {
            if (! node.hasType ("Table"))
                continue;
            const int o = juce::jlimit (0, kNumOscs - 1, (int) node.getProperty ("osc"));
            const int factory = node.getProperty ("factory", 0);
            if (factory >= 0)
                setTable (o, tables::makeFactory (factory));
            else
            {
                juce::MemoryBlock mb;
                if (mb.fromBase64Encoding (node.getProperty ("data").toString()) && mb.getSize() >= Wavetable::kSize * sizeof (float))
                {
                    std::vector<float> raw (mb.getSize() / sizeof (float));
                    std::memcpy (raw.data(), mb.getData(), raw.size() * sizeof (float));
                    setTable (o, std::make_shared<const Wavetable> (node.getProperty ("name").toString(), std::move (raw)));
                }
            }
        }
        for (int l = 0; l < kNumLfos; ++l)
            setLfoPoints (l, extra.getProperty ("lfo" + juce::String (l + 1), defaultLfoPoints()).toString());
        fx.setOrder (FxRack::orderFromString (extra.getProperty ("fxOrder").toString()));
        presetName = extra.getProperty ("presetName", "Init").toString();
    }
    sendChangeMessageToEditor();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzSynthProcessor();
}
