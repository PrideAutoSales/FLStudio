#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace cruzchords;

namespace
{
constexpr auto volumeID = "volume";

// Holds a loaded synth's own window.
class SynthWindow final : public juce::DocumentWindow
{
public:
    SynthWindow (const juce::String& name, std::function<void()> onClose)
        : juce::DocumentWindow (name, juce::Colour (0xff15131c), juce::DocumentWindow::closeButton),
          close (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        // Stay above FL Studio's own windows.
        setAlwaysOnTop (true);
        setResizable (false, false);
    }

    void closeButtonPressed() override
    {
        if (close)
            close();
    }

private:
    std::function<void()> close;
};
} // namespace

CruzChordsAudioProcessor::CruzChordsAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "Params",
              { std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { volumeID, 1 }, "Volume",
                                                             juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                                                             juce::AudioParameterFloatAttributes().withLabel ("dB")) })
{
    volume = params.getRawParameterValue (volumeID);

    juce::addDefaultFormatsToManager (formatManager);

    for (int i = 0; i < 16; ++i)
        builtIn.addVoice (new ChordVoice (builtInChoice));
    builtIn.addSound (new ChordSound());

    live = std::make_unique<Live>();
    spareLive = std::make_unique<Live>();

    // Start with something to play.
    int first = 0;
    for (int i = 0; i < (int) presets().size(); ++i)
        if (juce::String (presets()[(size_t) i].genre) == "Deep House")
        {
            first = i;
            break;
        }
    loadPreset (first);
}

CruzChordsAudioProcessor::~CruzChordsAudioProcessor()
{
    alive.reset();
    closeSynthWindow();
    const juce::ScopedLock lt (hostLifetime);
    hosted.reset();
}

//==============================================================================
void CruzChordsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    currentBlock = samplesPerBlock;

    builtIn.setCurrentPlaybackSampleRate (sampleRate);
    generated.ensureSize (8192);
    hostMidi.ensureSize (8192);

    {
        const juce::ScopedLock lt (hostLifetime);
        if (hosted != nullptr)
        {
            hosted->setRateAndBufferSizeDetails (sampleRate, samplesPerBlock);
            hosted->prepareToPlay (sampleRate, samplesPerBlock);
            hostBuffer.setSize (hostBuffer.getNumChannels(), juce::jmax (samplesPerBlock, 512), false, true, false);
        }
    }

    wasPlaying = wasHostPlaying = false;
    expectedPpq = -1;
    std::fill (std::begin (sounding), std::end (sounding), false);
    std::fill (std::begin (heldKeyCounts), std::end (heldKeyCounts), 0);
    lastGain = juce::Decibels::decibelsToGain (volume->load());
}

void CruzChordsAudioProcessor::releaseResources()
{
    const juce::ScopedLock lt (hostLifetime);
    if (hosted != nullptr)
        hosted->releaseResources();
}

bool CruzChordsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CruzChordsAudioProcessor::addNote (juce::MidiBuffer& buffer, int sample, int note, float velocity, bool on)
{
    if (note < 0 || note > 127)
        return;

    if (on)
    {
        buffer.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, (int) std::lround (velocity * 127.0f))),
                         sample);
        sounding[note] = true;
    }
    else
    {
        buffer.addEvent (juce::MidiMessage::noteOff (1, note), sample);
        sounding[note] = false;
    }
}

void CruzChordsAudioProcessor::allNotesOff (juce::MidiBuffer& buffer, int sample)
{
    player.stop ([&] (int, int note, float, bool) { addNote (buffer, sample, note, 0.0f, false); }, sample);
    for (int n = 0; n < 128; ++n)
        if (sounding[n])
            addNote (buffer, sample, n, 0.0f, false);
    std::fill (std::begin (heldKeyCounts), std::end (heldKeyCounts), 0);
}

void CruzChordsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    generated.clear();

    if (panicRequested.exchange (false))
        allNotesOff (generated, 0);

    const juce::SpinLock::ScopedTryLockType liveScope (liveLock);
    Live* L = liveScope.isLocked() ? live.get() : nullptr;
    const float velocityScale = L != nullptr ? L->velocity : 0.8f;

    // Notes from the window.
    {
        const auto scope = auditionFifo.read (auditionFifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i)
            addNote (generated, 0, auditionEvents[(size_t) (scope.startIndex1 + i)].note, velocityScale,
                     auditionEvents[(size_t) (scope.startIndex1 + i)].on);
        for (int i = 0; i < scope.blockSize2; ++i)
            addNote (generated, 0, auditionEvents[(size_t) (scope.startIndex2 + i)].note, velocityScale,
                     auditionEvents[(size_t) (scope.startIndex2 + i)].on);
    }

    // Notes coming in (piano roll or a keyboard). In the scale, one key plays a chord.
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pos = juce::jlimit (0, juce::jmax (0, n - 1), meta.samplePosition);

        if (msg.isNoteOn())
        {
            const int note = msg.getNoteNumber();
            const int pc = note % 12;
            if (L != nullptr && L->keysPlayChords && L->keyCounts[pc] > 0)
            {
                for (int i = 0; i < heldKeyCounts[note]; ++i)
                    addNote (generated, pos, heldKeyNotes[note][i], 0.0f, false);
                heldKeyCounts[note] = 0;

                for (int i = 0; i < L->keyCounts[pc]; ++i)
                {
                    const int out = note + L->keyOffsets[pc][i];
                    if (out >= 0 && out < 128)
                    {
                        addNote (generated, pos, out, msg.getFloatVelocity(), true);
                        heldKeyNotes[note][heldKeyCounts[note]++] = out;
                    }
                }
                continue;
            }
            addNote (generated, pos, note, msg.getFloatVelocity(), true);
        }
        else if (msg.isNoteOff())
        {
            const int note = msg.getNoteNumber();
            if (heldKeyCounts[note] > 0)
            {
                for (int i = 0; i < heldKeyCounts[note]; ++i)
                    addNote (generated, pos, heldKeyNotes[note][i], 0.0f, false);
                heldKeyCounts[note] = 0;
            }
            else
                addNote (generated, pos, note, 0.0f, false);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            allNotesOff (generated, pos);
            generated.addEvent (msg, pos);
        }
        else
            generated.addEvent (msg, pos);
    }

    // Where the song is.
    double bpm = 120.0;
    bool hostIsPlaying = false;
    std::optional<double> hostPpq;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm(); b.hasValue() && *b > 1.0)
                bpm = *b;
            hostIsPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition(); q.hasValue())
                hostPpq = *q;
        }
    hostBpm = bpm;
    hostPlaying = hostIsPlaying;

    // The progression.
    // The Play button, or FL Studio's own transport (space bar) when "Play with FL" is on.
    const bool isPlaying = playing.load() || (followHost.load() && hostIsPlaying);
    progressionSounding = isPlaying;
    if (isPlaying)
    {
        if (L != nullptr)
        {
            const double bps = bpm / 60.0 / currentRate;
            double start;
            bool jumped;
            if (hostIsPlaying && hostPpq.has_value())
            {
                start = *hostPpq;
                jumped = ! wasPlaying || ! wasHostPlaying || std::abs (start - expectedPpq) > 0.05;
            }
            else
            {
                jumped = ! wasPlaying || wasHostPlaying;
                if (jumped)
                    freePpq = 0.0;
                start = freePpq;
            }

            player.process (L->sequence, start, bps, n, jumped,
                            [&] (int s, int note, float v, bool on) { addNote (generated, s, note, v * velocityScale, on); });

            expectedPpq = start + bps * n;
            freePpq = start + bps * n;
            playingSlot = player.currentSlot();
            wasPlaying = true;
            wasHostPlaying = hostIsPlaying;
        }
    }
    else if (wasPlaying)
    {
        player.stop ([&] (int s, int note, float, bool) { addNote (generated, s, note, 0.0f, false); }, 0);
        playingSlot = -1;
        wasPlaying = false;
    }

    // Sound: the loaded synth, or the built-in one.
    {
        const juce::SpinLock::ScopedTryLockType hostScope (hostLock);
        if (hostScope.isLocked())
        {
            if (hosted != nullptr)
            {
                hostMidi.clear();
                hostMidi.addEvents (generated, 0, n, 0);
                hosted->setPlayHead (getPlayHead());

                const int ch = hostBuffer.getNumChannels();
                if (hostBuffer.getNumSamples() < n)
                    hostBuffer.setSize (ch, n, false, false, true);

                juce::AudioBuffer<float> view (hostBuffer.getArrayOfWritePointers(), ch, n);
                view.clear();
                hosted->processBlock (view, hostMidi);

                const int outs = juce::jmax (1, hosted->getTotalNumOutputChannels());
                for (int c = 0; c < buffer.getNumChannels(); ++c)
                    buffer.copyFrom (c, 0, view, juce::jmin (c, outs - 1, ch - 1), 0, n);
            }
            else
                builtIn.renderNextBlock (buffer, generated, 0, n);
        }
    }

    const float gain = juce::Decibels::decibelsToGain (volume->load());
    buffer.applyGainRamp (0, n, lastGain, gain);
    lastGain = gain;

    // Keep the built-in sound from clipping when chords stack up.
    if (! hasHosted.load())
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* d = buffer.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                d[i] = std::tanh (d[i]);
        }

    // The same notes go out as MIDI, for FL Studio's MIDI Out routing.
    midi.swapWith (generated);
}

//==============================================================================
RenderOptions CruzChordsAudioProcessor::renderOptions() const
{
    RenderOptions opt;
    opt.keyPc = m.key;
    opt.voicing.spread = m.spread;
    opt.voicing.smooth = m.smooth;
    opt.voicing.bassNote = m.bass;
    opt.voicing.octave = m.octave;
    opt.voicing.invert = m.invert;
    opt.rhythm = m.rhythm;
    return opt;
}

void CruzChordsAudioProcessor::rebuildLive()
{
    auto& next = *spareLive;
    next.sequence = render (m.chords, renderOptions());
    next.keysPlayChords = m.keysPlayChords;
    followHost = m.followHost;
    next.velocity = m.velocity;

    VoicingOptions v = renderOptions().voicing;
    for (int pc = 0; pc < 12; ++pc)
    {
        const auto offsets = keyChordOffsets (m.key, m.scale, m.size, v, pc);
        next.keyCounts[pc] = (int) juce::jmin (offsets.size(), (size_t) Sequence::maxNotes);
        for (int i = 0; i < next.keyCounts[pc]; ++i)
            next.keyOffsets[pc][i] = offsets[(size_t) i];
    }

    const juce::SpinLock::ScopedLockType lock (liveLock);
    std::swap (live, spareLive);
}

void CruzChordsAudioProcessor::modelChanged()
{
    m.chords.resize (juce::jmin (m.chords.size(), (size_t) Sequence::maxSlots));
    rebuildLive();
    builtInChoice = m.sound;

    if (auto xml = modelToTree().createXml())
    {
        const juce::ScopedLock sl (savedStateLock);
        savedModelXml = xml->toString();
    }
    sendChangeMessage();
}

void CruzChordsAudioProcessor::shuffle()
{
    ShuffleOptions o;
    o.scale = m.scale;
    o.feel = m.feel;
    o.adventure = m.adventure;
    o.totalBeats = 4.0 * juce::jlimit (1, 32, m.shuffleBars);
    o.chordBeats = m.chordEvery;
    o.seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
    m.chords = shuffleProgression (m.chords, o);
    m.presetIndex = -1;
    modelChanged();
}

void CruzChordsAudioProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets().size())
        return;

    const auto& p = presets()[(size_t) index];
    m.chords = parseProgression (p.progression);
    m.scale = p.scale;
    m.rhythm = p.rhythm;
    m.size = p.size;
    m.presetIndex = index;
    modelChanged();
}

Chord CruzChordsAudioProcessor::padChord (int degree) const
{
    Chord c;
    c.offset = scaleDegreeOffset (m.scale, degree);
    c.type = diatonicChordType (m.scale, degree, m.size);
    return c;
}

static std::vector<int> withBass (std::vector<int> notes, int rootPc, bool bass, int octave)
{
    if (bass)
    {
        const int b = bassNoteFor (rootPc) + 12 * octave;
        if (std::find (notes.begin(), notes.end(), b) == notes.end())
            notes.insert (notes.begin(), b);
    }
    return notes;
}

std::vector<int> CruzChordsAudioProcessor::padNotes (int degree) const
{
    const auto c = padChord (degree);
    auto notes = voiceChord (m.key + c.offset, c.type, 0, 0, m.spread);
    applyGlobalVoicing (notes, renderOptions().voicing);
    return withBass (notes, m.key + c.offset, m.bass, m.octave);
}

std::vector<int> CruzChordsAudioProcessor::slotNotes (int slot) const
{
    if (slot < 0 || slot >= (int) m.chords.size())
        return {};
    const auto opt = renderOptions();
    const auto voiced = voiceProgression (m.chords, m.key, opt.voicing);
    return withBass (voiced[(size_t) slot], m.key + m.chords[(size_t) slot].offset, m.bass, m.octave);
}

void CruzChordsAudioProcessor::audition (const std::vector<int>& notes, bool on)
{
    const auto scope = auditionFifo.write ((int) notes.size());
    size_t k = 0;
    for (int i = 0; i < scope.blockSize1; ++i)
        auditionEvents[(size_t) (scope.startIndex1 + i)] = { notes[k++], on };
    for (int i = 0; i < scope.blockSize2; ++i)
        auditionEvents[(size_t) (scope.startIndex2 + i)] = { notes[k++], on };
}

//==============================================================================
juce::Array<juce::File> CruzChordsAudioProcessor::findPluginFiles()
{
    juce::Array<juce::File> dirs;
   #if JUCE_MAC
    dirs.add (juce::File ("/Library/Audio/Plug-Ins/VST3"));
    dirs.add (juce::File ("~/Library/Audio/Plug-Ins/VST3"));
   #elif JUCE_WINDOWS
    dirs.add (juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory).getChildFile ("Common Files/VST3"));
   #else
    dirs.add (juce::File ("~/.vst3"));
    dirs.add (juce::File ("/usr/lib/vst3"));
    dirs.add (juce::File ("/usr/local/lib/vst3"));
   #endif

    juce::Array<juce::File> found;
    juce::StringArray names;
    auto addFrom = [&] (const juce::File& dir, auto& self, int depth) -> void
    {
        for (const auto& f : dir.findChildFiles (juce::File::findFilesAndDirectories, false, "*"))
        {
            if (f.hasFileExtension ("vst3"))
            {
                if (! f.getFileNameWithoutExtension().containsIgnoreCase ("Cruz Chords") && ! names.contains (f.getFileName()))
                {
                    names.add (f.getFileName());
                    found.add (f);
                }
            }
            else if (f.isDirectory() && depth < 1)
                self (f, self, depth + 1); // vendor folders
        }
    };
    for (const auto& d : dirs)
        if (d.isDirectory())
            addFrom (d, addFrom, 0);

    std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareIgnoreCase (b.getFileNameWithoutExtension()) < 0; });
    return found;
}

void CruzChordsAudioProcessor::loadPluginFile (const juce::File& file)
{
    lastLoadError.clear();
    juce::OwnedArray<juce::PluginDescription> types;
    for (auto* format : formatManager.getFormats())
        if (format->fileMightContainThisPluginType (file.getFullPathName()))
            format->findAllTypesForFile (types, file.getFullPathName());

    if (types.isEmpty())
    {
        lastLoadError = "Could not open " + file.getFileNameWithoutExtension() + ".";
        sendChangeMessage();
        return;
    }

    auto* pick = types.getFirst();
    for (auto* t : types)
        if (t->isInstrument)
        {
            pick = t;
            break;
        }

    if (! pick->isInstrument)
        lastLoadError = pick->name + " looks like an effect, not a synth, so it may stay silent.";

    loadPluginDescription (*pick, {});
}

void CruzChordsAudioProcessor::loadPluginDescription (const juce::PluginDescription& desc, const juce::MemoryBlock& state)
{
    std::weak_ptr<bool> weak = alive;
    formatManager.createPluginInstanceAsync (desc, currentRate, currentBlock,
                                             [this, weak, desc, state] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                                                        const juce::String& error)
                                             {
                                                 if (weak.expired())
                                                     return;
                                                 if (instance == nullptr)
                                                 {
                                                     lastLoadError = error.isNotEmpty() ? error : "That plugin could not be opened.";
                                                     sendChangeMessage();
                                                     return;
                                                 }
                                                 hostedDescription = desc;
                                                 installSynth (std::move (instance), state);
                                             });
}

void CruzChordsAudioProcessor::installSynth (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::MemoryBlock& state)
{
    // Stereo out and no audio in, when the synth allows it.
    auto layout = instance->getBusesLayout();
    for (auto& in : layout.inputBuses)
        in = juce::AudioChannelSet::disabled();
    if (! layout.outputBuses.isEmpty())
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    if (instance->checkBusesLayoutSupported (layout))
        instance->setBusesLayout (layout);

    instance->setRateAndBufferSizeDetails (currentRate, currentBlock);
    if (state.getSize() > 0)
        instance->setStateInformation (state.getData(), (int) state.getSize());
    instance->prepareToPlay (currentRate, currentBlock);

    const int channels = juce::jmax (2, instance->getTotalNumInputChannels(), instance->getTotalNumOutputChannels());
    juce::AudioBuffer<float> buffer (channels, juce::jmax (currentBlock, 512));

    closeSynthWindow();
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock lt (hostLifetime);
        const juce::SpinLock::ScopedLockType hl (hostLock);
        old = std::move (hosted);
        hosted = std::move (instance);
        hostBuffer = std::move (buffer);
        hasHosted = true;
    }
    builtIn.allNotesOff (0, false);
    setLatencySamples (hosted->getLatencySamples());
    old.reset();
    sendChangeMessage();
}

void CruzChordsAudioProcessor::removeSynth()
{
    closeSynthWindow();
    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock lt (hostLifetime);
        const juce::SpinLock::ScopedLockType hl (hostLock);
        old = std::move (hosted);
        hasHosted = false;
    }
    setLatencySamples (0);
    old.reset();
}

void CruzChordsAudioProcessor::useBuiltInSound (int sound)
{
    lastLoadError.clear();
    removeSynth();
    m.sound = juce::jlimit (0, 1, sound);
    modelChanged();
}

bool CruzChordsAudioProcessor::hasHostedSynth() const
{
    return hasHosted.load();
}

juce::String CruzChordsAudioProcessor::getSoundName() const
{
    const juce::ScopedLock lt (hostLifetime);
    if (hosted != nullptr)
        return hosted->getName();
    return m.sound == 0 ? "Built-in keys" : "Built-in pad";
}

void CruzChordsAudioProcessor::showSynthWindow()
{
    if (synthWindow != nullptr)
    {
        synthWindow->toFront (true);
        return;
    }

    const juce::ScopedLock lt (hostLifetime);
    if (hosted == nullptr)
        return;

    auto* editor = hosted->createEditorIfNeeded();
    if (editor == nullptr)
    {
        lastLoadError = hosted->getName() + " has no window of its own.";
        sendChangeMessage();
        return;
    }

    std::weak_ptr<bool> weak = alive;
    synthWindow = std::make_unique<SynthWindow> (hosted->getName(), [this, weak]
    {
        juce::MessageManager::callAsync ([this, weak]
        {
            if (! weak.expired())
                closeSynthWindow();
        });
    });
    synthWindow->setContentOwned (editor, true);
    synthWindow->centreWithSize (synthWindow->getWidth(), synthWindow->getHeight());
    synthWindow->setVisible (true);
    synthWindow->toFront (true);
}

void CruzChordsAudioProcessor::closeSynthWindow()
{
    if (synthWindow != nullptr)
    {
        synthWindow->clearContentComponent();
        synthWindow.reset();
    }
}

//==============================================================================
juce::String CruzChordsAudioProcessor::suggestedMidiName() const
{
    juce::String name;
    if (m.presetIndex >= 0 && m.presetIndex < (int) presets().size())
        name << presets()[(size_t) m.presetIndex].name << " - ";
    name << noteName (m.key, useFlats()) << " " << scales()[(size_t) m.scale].name;
    return juce::File::createLegalFileName (name);
}

bool CruzChordsAudioProcessor::writeMidiFile (const juce::File& file) const
{
    auto seq = std::make_unique<Sequence> (render (m.chords, renderOptions()));
    constexpr double ticks = 960.0;

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "Cruz Chords"), 0.0);

    int notes[Sequence::maxNotes];
    for (int i = 0; i < seq->numHits; ++i)
    {
        const auto& h = seq->hits[i];
        const int count = seq->notesFor (h, notes);
        const auto vel = (juce::uint8) juce::jlimit (1, 127, (int) std::lround (h.velocity * m.velocity * 127.0f));
        for (int k = 0; k < count; ++k)
        {
            track.addEvent (juce::MidiMessage::noteOn (1, notes[k], vel), h.start * ticks);
            track.addEvent (juce::MidiMessage::noteOff (1, notes[k]), h.end * ticks);
        }
    }
    track.updateMatchedPairs();

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote ((int) ticks);
    midiFile.addTrack (track);

    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream out (file);
    return out.openedOk() && midiFile.writeTo (out);
}

//==============================================================================
juce::ValueTree CruzChordsAudioProcessor::modelToTree() const
{
    juce::ValueTree t ("Model");
    t.setProperty ("key", m.key, nullptr);
    t.setProperty ("scale", m.scale, nullptr);
    t.setProperty ("size", m.size, nullptr);
    t.setProperty ("rhythm", m.rhythm, nullptr);
    t.setProperty ("spread", m.spread, nullptr);
    t.setProperty ("smooth", m.smooth, nullptr);
    t.setProperty ("bass", m.bass, nullptr);
    t.setProperty ("keysPlayChords", m.keysPlayChords, nullptr);
    t.setProperty ("followHost", m.followHost, nullptr);
    t.setProperty ("octave", m.octave, nullptr);
    t.setProperty ("invert", m.invert, nullptr);
    t.setProperty ("feel", m.feel, nullptr);
    t.setProperty ("adventure", m.adventure, nullptr);
    t.setProperty ("shuffleBars", m.shuffleBars, nullptr);
    t.setProperty ("chordEvery", m.chordEvery, nullptr);
    t.setProperty ("velocity", m.velocity, nullptr);
    t.setProperty ("sound", m.sound, nullptr);
    t.setProperty ("preset", m.presetIndex, nullptr);

    for (const auto& c : m.chords)
    {
        juce::ValueTree ct ("Chord");
        ct.setProperty ("offset", c.offset, nullptr);
        ct.setProperty ("type", c.type, nullptr);
        ct.setProperty ("inversion", c.inversion, nullptr);
        ct.setProperty ("octave", c.octave, nullptr);
        ct.setProperty ("beats", c.beats, nullptr);
        ct.setProperty ("locked", c.locked, nullptr);
        t.appendChild (ct, nullptr);
    }
    return t;
}

void CruzChordsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("CruzChords");
    root.setProperty ("version", 1, nullptr);

    {
        const juce::ScopedLock sl (savedStateLock);
        if (auto xml = juce::parseXML (savedModelXml))
            root.appendChild (juce::ValueTree::fromXml (*xml), nullptr);
    }
    root.appendChild (params.copyState(), nullptr);

    {
        const juce::ScopedLock lt (hostLifetime);
        if (hosted != nullptr)
        {
            juce::ValueTree s ("Synth");
            if (auto xml = hostedDescription.createXml())
                s.setProperty ("description", xml->toString(), nullptr);
            juce::MemoryBlock synthState;
            hosted->getStateInformation (synthState);
            s.setProperty ("state", synthState.toBase64Encoding(), nullptr);
            root.appendChild (s, nullptr);
        }
    }

    if (auto xml = root.createXml())
        copyXmlToBinary (*xml, destData);
}

void CruzChordsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.hasType ("CruzChords"))
        return;

    if (auto p = tree.getChildWithName (params.state.getType()); p.isValid())
        params.replaceState (p);

    if (juce::MessageManager::existsAndIsCurrentThread())
        applyState (tree);
    else
    {
        std::weak_ptr<bool> weak = alive;
        juce::MessageManager::callAsync ([this, weak, tree]
        {
            if (! weak.expired())
                applyState (tree);
        });
    }
}

void CruzChordsAudioProcessor::applyState (const juce::ValueTree& tree)
{
    if (auto t = tree.getChildWithName ("Model"); t.isValid())
    {
        const int numScales = (int) scales().size();
        m.key = juce::jlimit (0, 11, (int) t.getProperty ("key", m.key));
        m.scale = juce::jlimit (0, numScales - 1, (int) t.getProperty ("scale", m.scale));
        m.size = juce::jlimit (0, (int) numChordSizes - 1, (int) t.getProperty ("size", m.size));
        m.rhythm = juce::jlimit (0, (int) rhythms().size() - 1, (int) t.getProperty ("rhythm", m.rhythm));
        m.spread = juce::jlimit (0, (int) numSpreads - 1, (int) t.getProperty ("spread", m.spread));
        m.smooth = (bool) t.getProperty ("smooth", m.smooth);
        m.bass = (bool) t.getProperty ("bass", m.bass);
        m.keysPlayChords = (bool) t.getProperty ("keysPlayChords", m.keysPlayChords);
        m.followHost = (bool) t.getProperty ("followHost", m.followHost);
        m.octave = juce::jlimit (-3, 3, (int) t.getProperty ("octave", 0));
        m.invert = juce::jlimit (-4, 4, (int) t.getProperty ("invert", 0));
        m.feel = juce::jlimit (0, (int) cruzchords::numFeels - 1, (int) t.getProperty ("feel", m.feel));
        m.adventure = juce::jlimit (0.0f, 1.0f, (float) t.getProperty ("adventure", m.adventure));
        m.shuffleBars = juce::jlimit (1, 32, (int) t.getProperty ("shuffleBars", m.shuffleBars));
        m.chordEvery = juce::jlimit (1.0, 8.0, (double) t.getProperty ("chordEvery", m.chordEvery));
        m.velocity = juce::jlimit (0.05f, 1.0f, (float) t.getProperty ("velocity", m.velocity));
        m.sound = juce::jlimit (0, 1, (int) t.getProperty ("sound", m.sound));
        m.presetIndex = juce::jlimit (-1, (int) presets().size() - 1, (int) t.getProperty ("preset", m.presetIndex));

        m.chords.clear();
        for (const auto& ct : t)
            if (ct.hasType ("Chord"))
            {
                Chord c;
                c.offset = juce::jlimit (0, 11, (int) ct.getProperty ("offset", 0));
                c.type = juce::jlimit (0, (int) numChordTypes - 1, (int) ct.getProperty ("type", 0));
                c.inversion = juce::jlimit (0, 5, (int) ct.getProperty ("inversion", 0));
                c.octave = juce::jlimit (-2, 2, (int) ct.getProperty ("octave", 0));
                c.beats = juce::jlimit (0.25, 128.0, (double) ct.getProperty ("beats", 4.0));
                c.locked = (bool) ct.getProperty ("locked", false);
                m.chords.push_back (c);
            }
        modelChanged();
    }

    if (auto s = tree.getChildWithName ("Synth"); s.isValid())
    {
        juce::PluginDescription desc;
        auto descXml = juce::parseXML (s.getProperty ("description").toString());
        juce::MemoryBlock synthState;
        synthState.fromBase64Encoding (s.getProperty ("state").toString());
        if (descXml != nullptr && desc.loadFromXml (*descXml))
            loadPluginDescription (desc, synthState);
    }
    else if (hasHostedSynth())
    {
        removeSynth();
        sendChangeMessage();
    }
}

//==============================================================================
juce::AudioProcessorEditor* CruzChordsAudioProcessor::createEditor()
{
    return new CruzChordsAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CruzChordsAudioProcessor();
}
