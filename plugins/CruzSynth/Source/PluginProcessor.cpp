#include "PluginProcessor.h"
#include "PluginEditor.h"

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
      fx (apvts)
{
    for (int o = 0; o < kNumOscs; ++o)
        uiTables[(size_t) o] = audioTables[(size_t) o] = tables::makeFactory (0);
    engine.setTables (audioTables[0].get(), audioTables[1].get());

    for (int l = 0; l < kNumLfos; ++l)
        setLfoPoints (l, defaultLfoPoints());

    // Start on a plain saw, like most synths' init patch.
    setParam (id::osc (0, "pos"), 2.0f / 3.0f);
    setParam (id::osc (1, "pos"), 2.0f / 3.0f);
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

    auto idx = [&] (int s, const char* n) { return apvts.getParameter (id::slot (s, n)); };
    int free = -1;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = juce::roundToInt (apvts.getRawParameterValue (id::slot (s, "src"))->load());
        const int dst = juce::roundToInt (apvts.getRawParameterValue (id::slot (s, "dst"))->load());
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
        auto* p = apvts.getParameter (id::slot (slot, n));
        p->setValueNotifyingHost (p->getDefaultValue());
    }
}

juce::Array<int> CruzSynthProcessor::slotsForDestination (int destination) const
{
    juce::Array<int> r;
    for (int s = 0; s < kNumModSlots; ++s)
    {
        const int src = juce::roundToInt (apvts.getRawParameterValue (id::slot (s, "src"))->load());
        const int dst = juce::roundToInt (apvts.getRawParameterValue (id::slot (s, "dst"))->load());
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
    return { "Init", "Super Lead", "Reese Bass", "Wobble Bass", "Glass Pluck", "Vowel Pad",
             "Sync Lead", "Breathy Keys", "Punch Sub", "Growl", "Digital Arp", "Pulse Strings" };
}

int CruzSynthProcessor::getNumPrograms() { return getFactoryPresetNames().size(); }

const juce::String CruzSynthProcessor::getProgramName (int index) { return getFactoryPresetNames()[index]; }

void CruzSynthProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        loadFactoryPreset (index);
}

void CruzSynthProcessor::loadFactoryPreset (int index)
{
    resetParametersToDefault();
    for (int o = 0; o < kNumOscs; ++o)
        setTable (o, tables::makeFactory (0));
    for (int l = 0; l < kNumLfos; ++l)
        setLfoPoints (l, defaultLfoPoints());
    fx.setOrder ({ FxDistortion, FxChorus, FxPhaser, FxDelay, FxReverb, FxCompressor, FxEq });

    int nextSlot = 0;
    auto set = [this] (const juce::String& pid, float v) { setParam (pid, v); };
    auto table = [this] (int o, const char* name) { setTable (o, tables::makeFactory (factoryTableIndex (name))); };
    auto mod = [&] (ModSrc s, ModDst d, float amount, bool bipolar = false)
    {
        set (id::slot (nextSlot, "src"), (float) s);
        set (id::slot (nextSlot, "dst"), (float) d);
        set (id::slot (nextSlot, "amount"), amount);
        set (id::slot (nextSlot, "bipolar"), bipolar ? 1.0f : 0.0f);
        ++nextSlot;
    };
    const float saw = 2.0f / 3.0f;

    switch (index)
    {
        case 1: // Super Lead
            set ("oscA_pos", saw); set ("oscA_unison", 7); set ("oscA_detune", 0.35f); set ("oscA_blend", 0.8f);
            set ("oscB_on", 1); table (1, "Hollow Lead"); set ("oscB_octave", 1); set ("oscB_level", 0.4f);
            set ("oscB_unison", 3); set ("oscB_detune", 0.2f); set ("oscB_pos", 0.5f);
            set ("flt_on", 1); set ("flt_type", FltLp24); set ("flt_cutoff", 6000.0f); set ("flt_res", 0.2f);
            set ("env1_decay", 0.5f); set ("env1_sustain", 0.8f); set ("env1_release", 0.3f);
            set ("fx_dly_on", 1); set ("fx_dly_mix", 0.15f);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.2f);
            mod (ModSrc::Macro1, DstFltCutoff, 0.4f);
            mod (ModSrc::ModWheel, DstOscADetune, 0.4f);
            set ("master_vol", 0.5f);
            break;

        case 2: // Reese Bass
            set ("oscA_pos", saw); set ("oscA_octave", -1); set ("oscA_unison", 2); set ("oscA_detune", 0.3f);
            set ("oscB_on", 1); set ("oscB_pos", saw); set ("oscB_octave", -1); set ("oscB_unison", 2);
            set ("oscB_detune", 0.45f); set ("oscB_fine", 12.0f);
            set ("sub_on", 1); set ("sub_octave", -2); set ("sub_level", 0.6f);
            set ("flt_on", 1); set ("flt_type", FltLadder); set ("flt_cutoff", 600.0f); set ("flt_res", 0.3f); set ("flt_drive", 0.3f);
            set ("lfo1_sync", 0); set ("lfo1_rate", 0.3f); set ("lfo1_mode", LfoFree);
            mod (ModSrc::Lfo1, DstFltCutoff, 0.15f);
            set ("mono", 1); set ("glide", 0.05f);
            set ("fx_dist_on", 1); set ("fx_dist_drive", 0.3f);
            mod (ModSrc::Macro1, DstFltCutoff, 0.5f);
            break;

        case 3: // Wobble Bass
            table (0, "FM Growl"); set ("oscA_octave", -1); set ("oscA_unison", 3); set ("oscA_detune", 0.15f);
            set ("sub_on", 1); set ("sub_octave", -2);
            set ("flt_on", 1); set ("flt_type", FltLp24); set ("flt_cutoff", 250.0f); set ("flt_res", 0.4f); set ("flt_drive", 0.4f);
            set ("lfo1_div", 10); set ("lfo1_mode", LfoFree);
            mod (ModSrc::Lfo1, DstFltCutoff, 0.55f);
            mod (ModSrc::Lfo1, DstOscAPos, 0.4f);
            set ("fx_dist_on", 1); set ("fx_dist_drive", 0.45f); set ("fx_dist_mix", 0.6f);
            set ("fx_cmp_on", 1); set ("fx_cmp_multi", 1); set ("fx_cmp_upward", 0.4f); set ("fx_cmp_thresh", -24.0f);
            set ("mono", 1);
            mod (ModSrc::Macro1, DstOscAPos, 0.5f);
            break;

        case 4: // Glass Pluck
            table (0, "Glass Bell"); set ("oscA_pos", 0.3f);
            set ("oscB_on", 1); table (1, "Sine"); set ("oscB_octave", 1); set ("oscB_level", 0.3f);
            set ("env1_attack", 0.001f); set ("env1_decay", 0.45f); set ("env1_sustain", 0.0f); set ("env1_release", 0.4f);
            set ("flt_on", 1); set ("flt_type", FltLp12); set ("flt_cutoff", 1500.0f); set ("flt_res", 0.25f);
            set ("env2_decay", 0.25f); set ("env2_sustain", 0.0f);
            mod (ModSrc::Env2, DstFltCutoff, 0.45f);
            mod (ModSrc::Velocity, DstOscAPos, 0.3f);
            set ("fx_dly_on", 1); set ("fx_dly_mix", 0.25f);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.3f);
            set ("poly", 12);
            break;

        case 5: // Vowel Pad
            table (0, "Vowel Morph"); set ("oscA_unison", 5); set ("oscA_detune", 0.2f);
            set ("lfo2_sync", 0); set ("lfo2_rate", 0.15f); set ("lfo2_mode", LfoFree);
            mod (ModSrc::Lfo2, DstOscAPos, 0.6f);
            set ("env1_attack", 1.2f); set ("env1_decay", 1.0f); set ("env1_sustain", 0.8f); set ("env1_release", 2.5f);
            set ("fx_cho_on", 1);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.4f); set ("fx_rev_size", 0.85f);
            set ("poly", 12);
            break;

        case 6: // Sync Lead
            set ("oscA_pos", saw); set ("oscA_warp", WarpSync); set ("oscA_unison", 3); set ("oscA_detune", 0.15f);
            set ("env2_decay", 0.8f); set ("env2_sustain", 0.2f);
            mod (ModSrc::Env2, DstOscAWarp, 0.7f);
            set ("mono", 1); set ("legato", 1); set ("glide", 0.08f);
            set ("fx_dly_on", 1); set ("fx_dly_mix", 0.2f);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.15f);
            break;

        case 7: // Breathy Keys
            table (0, "Organ Bars"); set ("oscA_pos", 0.3f);
            set ("noise_on", 1); set ("noise_type", 1); set ("noise_level", 0.15f);
            set ("env1_attack", 0.01f); set ("env1_decay", 1.2f); set ("env1_sustain", 0.5f); set ("env1_release", 0.5f);
            set ("fx_pha_on", 1);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.25f);
            break;

        case 8: // Punch Sub
            table (0, "Sine"); set ("oscA_octave", -1);
            set ("env1_attack", 0.001f); set ("env1_decay", 0.8f); set ("env1_sustain", 0.7f); set ("env1_release", 0.12f);
            set ("env2_attack", 0.0f); set ("env2_decay", 0.08f); set ("env2_sustain", 0.0f);
            mod (ModSrc::Env2, DstOscACoarse, 0.15f);
            set ("mono", 1); set ("glide", 0.03f);
            set ("fx_dist_on", 1); set ("fx_dist_mode", 1); set ("fx_dist_drive", 0.15f);
            break;

        case 9: // Growl
            table (0, "Fold Sweep"); set ("oscA_octave", -1); set ("oscA_unison", 2); set ("oscA_detune", 0.1f);
            set ("lfo1_div", 13); set ("lfo1_mode", LfoFree);
            mod (ModSrc::Lfo1, DstOscAPos, 0.7f);
            set ("flt_on", 1); set ("flt_type", FltVowel); set ("flt_cutoff", 1000.0f); set ("flt_res", 0.5f);
            mod (ModSrc::Lfo1, DstFltCutoff, 0.3f);
            set ("fx_dist_on", 1); set ("fx_dist_mode", 3); set ("fx_dist_drive", 0.3f);
            set ("fx_cmp_on", 1); set ("fx_cmp_multi", 1); set ("fx_cmp_upward", 0.5f);
            set ("mono", 1);
            break;

        case 10: // Digital Arp
            table (0, "Digital Steps");
            set ("env1_attack", 0.001f); set ("env1_decay", 0.25f); set ("env1_sustain", 0.0f); set ("env1_release", 0.2f);
            set ("lfo1_shape", LfoSampleHold); set ("lfo1_div", 13); set ("lfo1_mode", LfoFree);
            mod (ModSrc::Lfo1, DstOscAPos, 0.8f);
            set ("fx_dly_on", 1); set ("fx_dly_div", 10); set ("fx_dly_mix", 0.3f);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.2f);
            break;

        case 11: // Pulse Strings
            table (0, "Pulse Width"); set ("oscA_unison", 5); set ("oscA_detune", 0.25f);
            set ("lfo1_sync", 0); set ("lfo1_rate", 0.4f); set ("lfo1_mode", LfoFree);
            mod (ModSrc::Lfo1, DstOscAPos, 0.5f);
            set ("env1_attack", 0.4f); set ("env1_sustain", 0.9f); set ("env1_release", 1.2f);
            set ("flt_on", 1); set ("flt_type", FltLp12); set ("flt_cutoff", 4000.0f);
            set ("fx_cho_on", 1);
            set ("fx_rev_on", 1); set ("fx_rev_mix", 0.35f);
            set ("poly", 12); set ("master_vol", 0.5f);
            break;

        default: // Init
            set ("oscA_pos", saw);
            set ("oscB_pos", saw);
            break;
    }

    currentProgram = index;
    presetName = getFactoryPresetNames()[index];
    sendChangeMessageToEditor();
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
