#include "PluginEditor.h"

using namespace cruzchords;

namespace
{
namespace Colours
{
    const juce::Colour bg { 0xff15131c };
    const juce::Colour panel { 0xff201c2a };
    const juce::Colour raised { 0xff2c2639 };
    const juce::Colour edge { 0xff3d3550 };
    const juce::Colour accent { 0xffff7a59 };  // coral
    const juce::Colour accent2 { 0xff4fd1c5 }; // teal
    const juce::Colour text { 0xffece8f4 };
    const juce::Colour dim { 0xff9a93ad };
}

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

bool same (double a, double b) { return std::abs (a - b) < 1.0e-6; }

juce::String lengthText (double beats)
{
    if (same (beats, 1.0))
        return "1 beat";
    if (beats < 4.0)
        return juce::String (beats, same (beats, std::floor (beats)) ? 0 : 1) + " beats";
    const double bars = beats / 4.0;
    return juce::String (bars, same (bars, std::floor (bars)) ? 0 : 1) + (same (bars, 1.0) ? " bar" : " bars");
}

const double lengthChoices[] = { 1, 2, 3, 4, 6, 8, 12, 16 };

void setupLabel (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setColour (juce::Label::textColourId, Colours::dim);
    l.setJustificationType (juce::Justification::centredLeft);
}
} // namespace

//==============================================================================
namespace cruzchordsui
{
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colours::bg);
    setColour (juce::ComboBox::backgroundColourId, Colours::raised);
    setColour (juce::ComboBox::outlineColourId, Colours::edge);
    setColour (juce::ComboBox::textColourId, Colours::text);
    setColour (juce::ComboBox::arrowColourId, Colours::accent);
    setColour (juce::ComboBox::focusedOutlineColourId, Colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    setColour (juce::PopupMenu::textColourId, Colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, Colours::bg);
    setColour (juce::PopupMenu::headerTextColourId, Colours::accent2);
    setColour (juce::TextButton::buttonColourId, Colours::raised);
    setColour (juce::TextButton::buttonOnColourId, Colours::accent);
    setColour (juce::TextButton::textColourOffId, Colours::text);
    setColour (juce::TextButton::textColourOnId, Colours::bg);
    setColour (juce::ToggleButton::textColourId, Colours::text);
    setColour (juce::ToggleButton::tickColourId, Colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, Colours::dim);
    setColour (juce::Slider::trackColourId, Colours::accent);
    setColour (juce::Slider::backgroundColourId, Colours::raised);
    setColour (juce::Slider::thumbColourId, Colours::text);
    setColour (juce::Slider::textBoxTextColourId, Colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Colours::text);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    auto c = b.getToggleState() ? Colours::accent : Colours::raised;
    if (! b.isEnabled())
        c = c.withMultipliedAlpha (0.4f);
    else if (down)
        c = c.brighter (0.25f);
    else if (highlighted)
        c = c.brighter (0.1f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Colours::edge);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return font (juce::jmin (15.0f, h * 0.5f), true); }
juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return font (14.0f); }
juce::Font LookAndFeel::getPopupMenuFont() { return font (14.0f); }
juce::Font LookAndFeel::getLabelFont (juce::Label&) { return font (13.0f); }

//==============================================================================
void ChordPad::setChord (const juce::String& n, const juce::String& r)
{
    if (n != name || r != numeral)
    {
        name = n;
        numeral = r;
        repaint();
    }
}

void ChordPad::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    g.setColour (down ? Colours::accent : Colours::raised);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Colours::edge);
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    auto addBar = r.removeFromBottom (26.0f);
    g.setColour (down ? Colours::bg : Colours::text);
    g.setFont (font (22.0f, true));
    g.drawFittedText (name, r.removeFromTop (r.getHeight() * 0.62f).toNearestInt(), juce::Justification::centredBottom, 1);
    g.setColour (down ? Colours::bg.withAlpha (0.8f) : Colours::dim);
    g.setFont (font (14.0f));
    g.drawFittedText (numeral, r.toNearestInt(), juce::Justification::centredTop, 1);

    g.setColour (hoverAdd ? Colours::accent2 : Colours::edge);
    g.fillRoundedRectangle (addBar.reduced (6.0f, 3.0f), 5.0f);
    g.setColour (hoverAdd ? Colours::bg : Colours::text);
    g.setFont (font (13.0f, true));
    g.drawText ("+ Add", addBar, juce::Justification::centred);
}

void ChordPad::mouseDown (const juce::MouseEvent& e)
{
    if (inAddBar (e.getPosition()))
    {
        if (onAdd)
            onAdd();
        return;
    }
    down = true;
    repaint();
    if (onPress)
        onPress (true);
}

void ChordPad::mouseUp (const juce::MouseEvent&)
{
    if (down)
    {
        down = false;
        repaint();
        if (onPress)
            onPress (false);
    }
}

void ChordPad::mouseMove (const juce::MouseEvent& e)
{
    const bool h = inAddBar (e.getPosition());
    if (h != hoverAdd)
    {
        hoverAdd = h;
        repaint();
    }
}

void ChordPad::mouseExit (const juce::MouseEvent&)
{
    hoverAdd = false;
    repaint();
}

//==============================================================================
ProgressionStrip::ProgressionStrip (CruzChordsAudioProcessor& p) : processor (p) {}

std::vector<juce::Rectangle<float>> ProgressionStrip::slotRects() const
{
    const auto& chords = processor.model().chords;
    std::vector<juce::Rectangle<float>> rects;
    double total = 0;
    for (const auto& c : chords)
        total += c.beats;
    if (total <= 0)
        return rects;

    auto area = getLocalBounds().toFloat();
    float x = area.getX();
    for (const auto& c : chords)
    {
        const float w = (float) (c.beats / total) * area.getWidth();
        rects.push_back ({ x, area.getY(), w, area.getHeight() });
        x += w;
    }
    return rects;
}

void ProgressionStrip::paint (juce::Graphics& g)
{
    const auto& m = processor.model();
    const auto rects = slotRects();

    if (rects.empty())
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (Colours::edge);
        g.drawRoundedRectangle (r, 10.0f, 1.5f);
        g.setColour (Colours::dim);
        g.setFont (font (15.0f));
        g.drawFittedText ("Empty. Pick a progression above, or press + Add on any chord.", r.toNearestInt(),
                          juce::Justification::centred, 2);
        return;
    }

    const bool flats = processor.useFlats();
    for (size_t i = 0; i < rects.size(); ++i)
    {
        auto r = rects[i].reduced (3.0f);
        const auto& c = m.chords[i];
        const bool isPlaying = (int) i == playing;
        const bool isSelected = (int) i == selected;
        const bool isHeld = (int) i == held;

        g.setColour (isHeld ? Colours::accent : (isPlaying ? Colours::accent2.withAlpha (0.35f) : Colours::raised));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (isSelected ? Colours::accent : Colours::edge);
        g.drawRoundedRectangle (r, 10.0f, isSelected ? 2.5f : 1.0f);

        auto inner = r.reduced (6.0f, 8.0f);
        const auto ink = isHeld ? Colours::bg : Colours::text;
        g.setColour (ink);
        g.setFont (font (r.getWidth() < 70 ? 15.0f : 21.0f, true));
        g.drawFittedText (chordName (c, m.key, flats), inner.removeFromTop (inner.getHeight() * 0.45f).toNearestInt(),
                          juce::Justification::centredBottom, 1);
        g.setColour (isHeld ? Colours::bg.withAlpha (0.8f) : Colours::dim);
        g.setFont (font (13.0f));
        g.drawFittedText (juce::String::fromUTF8 (romanNumeral (c).c_str()), inner.removeFromTop (20).toNearestInt(),
                          juce::Justification::centred, 1);
        g.drawFittedText (lengthText (c.beats), inner.toNearestInt(), juce::Justification::centredTop, 1);
    }
}

void ProgressionStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto rects = slotRects();
    int hit = -1;
    for (size_t i = 0; i < rects.size(); ++i)
        if (rects[i].contains (e.position))
            hit = (int) i;

    if (onSelect)
        onSelect (hit);
    if (hit >= 0)
    {
        held = hit;
        if (onAudition)
            onAudition (hit, true);
    }
    repaint();
}

void ProgressionStrip::mouseUp (const juce::MouseEvent&)
{
    if (held >= 0 && onAudition)
        onAudition (held, false);
    held = -1;
    repaint();
}

void ProgressionStrip::setSelected (int s)
{
    if (s != selected)
    {
        selected = s;
        repaint();
    }
}

void ProgressionStrip::setPlaying (int s)
{
    if (s != playing)
    {
        playing = s;
        repaint();
    }
}

//==============================================================================
void MidiDragZone::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool hot = isMouseOver() || dragging;
    g.setColour (hot ? Colours::accent2.withAlpha (0.15f) : Colours::panel);
    g.fillRoundedRectangle (r, 10.0f);

    juce::Path border;
    border.addRoundedRectangle (r, 10.0f);
    const float dashes[] = { 6.0f, 4.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, border, dashes, 2);
    g.setColour (Colours::accent2);
    g.fillPath (dashed);

    g.setColour (Colours::text);
    g.setFont (font (15.0f, true));
    g.drawText ("Drag MIDI into FL Studio", r.removeFromTop (r.getHeight() * 0.58f), juce::Justification::centredBottom);
    g.setColour (Colours::dim);
    g.setFont (font (12.0f));
    g.drawText ("drop it on the playlist or a channel", r, juce::Justification::centredTop);
}

void MidiDragZone::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 6 || ! makeFile)
        return;

    const auto file = makeFile();
    if (! file.existsAsFile())
        return;

    dragging = true;
    repaint();
    juce::Component::SafePointer<MidiDragZone> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this, [safe]
    {
        if (safe != nullptr)
        {
            safe->dragging = false;
            safe->repaint();
        }
    });
}

void MidiDragZone::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}
} // namespace cruzchordsui

//==============================================================================
CruzChordsAudioProcessorEditor::CruzChordsAudioProcessorEditor (CruzChordsAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), model (p.model()), strip (p)
{
    setLookAndFeel (&lookAndFeel);

    // Sound
    addAndMakeVisible (soundBox);
    soundBox.onChange = [this] { soundChosen(); };
    addAndMakeVisible (openSynthButton);
    openSynthButton.onClick = [this] { processor.showSynthWindow(); };
    volumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    volumeSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    volumeSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (volumeSlider);
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.getParams(), "volume", volumeSlider);
    statusLabel.setColour (juce::Label::textColourId, Colours::accent);
    statusLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (statusLabel);

    // Key and presets
    setupLabel (keyLabel, "Key");
    setupLabel (scaleLabel, "Scale");
    setupLabel (genreLabel, "Genre");
    setupLabel (presetLabel, "Progression");
    for (auto* l : { &keyLabel, &scaleLabel, &genreLabel, &presetLabel })
        addAndMakeVisible (*l);

    for (auto* b : { &keyBox, &scaleBox, &genreBox, &presetBox })
        addAndMakeVisible (*b);
    for (const auto& s : scales())
        scaleBox.addItem (s.name, scaleBox.getNumItems() + 1);
    for (const auto& genre : presetGenres())
        genreBox.addItem (genre, genreBox.getNumItems() + 1);

    keyBox.onChange = [this]
    {
        model.key = keyBox.getSelectedItemIndex();
        processor.modelChanged();
    };
    scaleBox.onChange = [this]
    {
        model.scale = scaleBox.getSelectedItemIndex();
        processor.modelChanged();
    };
    genreBox.onChange = [this] { fillPresetBox(); };
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0 && presetBox.getSelectedId() - 1 != model.presetIndex)
        {
            selectSlot (-1);
            processor.loadPreset (presetBox.getSelectedId() - 1);
        }
    };

    for (auto* b : { &prevButton, &nextButton, &surpriseButton })
        addAndMakeVisible (*b);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };
    surpriseButton.onClick = [this] { stepPreset (0); };

    // Chord pads
    const char* sizeNames[] = { "Triads", "7ths", "9ths" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = sizeButtons[i];
        b.setButtonText (sizeNames[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i]
        {
            model.size = i;
            processor.modelChanged();
        };
        addAndMakeVisible (b);
    }

    for (int d = 0; d < 7; ++d)
    {
        auto* pad = pads.add (new cruzchordsui::ChordPad());
        pad->onPress = [this, d] (bool down)
        {
            if (down)
            {
                padHeld[d] = processor.padNotes (d);
                processor.audition (padHeld[d], true);

                // With a chord selected in the progression, a pad replaces it.
                if (selectedSlot >= 0)
                    editSelected ([this, d] (Chord& c)
                    {
                        const auto pc = processor.padChord (d);
                        c.offset = pc.offset;
                        c.type = pc.type;
                        c.inversion = 0;
                    });
            }
            else
            {
                processor.audition (padHeld[d], false);
                padHeld[d].clear();
            }
        };
        pad->onAdd = [this, d]
        {
            if ((int) model.chords.size() >= Sequence::maxSlots)
            {
                statusLabel.setText ("The progression is full (16 chords).", juce::dontSendNotification);
                return;
            }
            auto c = processor.padChord (d);
            c.beats = model.chords.empty() ? 4.0 : model.chords.back().beats;
            model.chords.push_back (c);
            selectedSlot = -1;
            processor.modelChanged();
        };
        addAndMakeVisible (pad);
    }

    // Progression
    addAndMakeVisible (strip);
    strip.onSelect = [this] (int i) { selectSlot (i); };
    strip.onAudition = [this] (int i, bool on)
    {
        if (on)
        {
            stripHeld = processor.slotNotes (i);
            processor.audition (stripHeld, true);
        }
        else
        {
            processor.audition (stripHeld, false);
            stripHeld.clear();
        }
    };

    setupLabel (rootLabel, "Root");
    setupLabel (typeLabel, "Type");
    setupLabel (invLabel, "Inversion");
    setupLabel (octLabel, "Octave");
    setupLabel (lengthLabel, "Length");
    for (auto* l : { &rootLabel, &typeLabel, &invLabel, &octLabel, &lengthLabel })
        addAndMakeVisible (*l);
    for (auto* l : { &invValue, &octValue })
    {
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*l);
    }

    for (int pc = 0; pc < 12; ++pc)
        rootBox.addItem (noteName (pc, false), pc + 1);
    for (int t = 0; t < numChordTypes; ++t)
        typeBox.addItem (chordTypeInfo (t).menuName, t + 1);
    for (double b : lengthChoices)
        lengthBox.addItem (lengthText (b), lengthBox.getNumItems() + 1);
    for (auto* b : { &rootBox, &typeBox, &lengthBox })
        addAndMakeVisible (*b);

    rootBox.onChange = [this]
    {
        const int pc = rootBox.getSelectedId() - 1;
        if (pc >= 0)
            editSelected ([this, pc] (Chord& c) { c.offset = ((pc - model.key) % 12 + 12) % 12; });
    };
    typeBox.onChange = [this]
    {
        const int t = typeBox.getSelectedId() - 1;
        if (t >= 0)
            editSelected ([t] (Chord& c)
            {
                c.type = t;
                c.inversion = 0;
            });
    };
    lengthBox.onChange = [this]
    {
        const int i = lengthBox.getSelectedItemIndex();
        if (i >= 0)
            editSelected ([i] (Chord& c) { c.beats = lengthChoices[i]; });
    };

    auto stepButton = [this] (juce::TextButton& b, std::function<void (Chord&)> f)
    {
        addAndMakeVisible (b);
        b.onClick = [this, f] { editSelected (f); };
    };
    stepButton (invDown, [] (Chord& c) { c.inversion = (c.inversion + chordTypeInfo (c.type).count - 1) % chordTypeInfo (c.type).count; });
    stepButton (invUp, [] (Chord& c) { c.inversion = (c.inversion + 1) % chordTypeInfo (c.type).count; });
    stepButton (octDown, [] (Chord& c) { c.octave = juce::jmax (-2, c.octave - 1); });
    stepButton (octUp, [] (Chord& c) { c.octave = juce::jmin (2, c.octave + 1); });

    for (auto* b : { &moveLeft, &moveRight, &deleteButton, &duplicateButton, &clearButton })
        addAndMakeVisible (*b);
    moveLeft.onClick = [this]
    {
        if (selectedSlot > 0)
        {
            std::swap (model.chords[(size_t) selectedSlot], model.chords[(size_t) selectedSlot - 1]);
            --selectedSlot;
            processor.modelChanged();
        }
    };
    moveRight.onClick = [this]
    {
        if (selectedSlot >= 0 && selectedSlot + 1 < (int) model.chords.size())
        {
            std::swap (model.chords[(size_t) selectedSlot], model.chords[(size_t) selectedSlot + 1]);
            ++selectedSlot;
            processor.modelChanged();
        }
    };
    deleteButton.onClick = [this]
    {
        if (selectedSlot >= 0 && selectedSlot < (int) model.chords.size())
        {
            model.chords.erase (model.chords.begin() + selectedSlot);
            selectedSlot = juce::jmin (selectedSlot, (int) model.chords.size() - 1);
            processor.modelChanged();
        }
    };
    duplicateButton.onClick = [this]
    {
        if (selectedSlot >= 0 && (int) model.chords.size() < Sequence::maxSlots)
        {
            model.chords.insert (model.chords.begin() + selectedSlot + 1, model.chords[(size_t) selectedSlot]);
            ++selectedSlot;
            processor.modelChanged();
        }
    };
    clearButton.onClick = [this]
    {
        model.chords.clear();
        model.presetIndex = -1;
        selectedSlot = -1;
        processor.modelChanged();
    };

    // Feel
    setupLabel (rhythmLabel, "Rhythm");
    setupLabel (spreadLabel, "Voicing");
    setupLabel (velocityLabel, "Velocity");
    for (auto* l : { &rhythmLabel, &spreadLabel, &velocityLabel })
        addAndMakeVisible (*l);
    for (const auto& r : rhythms())
        rhythmBox.addItem (r.name, rhythmBox.getNumItems() + 1);
    spreadBox.addItem ("Close", 1);
    spreadBox.addItem ("Open", 2);
    spreadBox.addItem ("Wide", 3);
    addAndMakeVisible (rhythmBox);
    addAndMakeVisible (spreadBox);
    rhythmBox.onChange = [this]
    {
        model.rhythm = rhythmBox.getSelectedItemIndex();
        processor.modelChanged();
    };
    spreadBox.onChange = [this]
    {
        model.spread = spreadBox.getSelectedItemIndex();
        processor.modelChanged();
    };

    for (auto* t : { &smoothToggle, &bassToggle, &keysToggle })
        addAndMakeVisible (*t);
    smoothToggle.onClick = [this]
    {
        model.smooth = smoothToggle.getToggleState();
        processor.modelChanged();
    };
    bassToggle.onClick = [this]
    {
        model.bass = bassToggle.getToggleState();
        processor.modelChanged();
    };
    keysToggle.onClick = [this]
    {
        model.keysPlayChords = keysToggle.getToggleState();
        processor.modelChanged();
    };
    keysToggle.setTooltip ("Notes in the scale, from the piano roll or a keyboard, play the chord built on that note.");
    smoothToggle.setTooltip ("Picks inversions so each chord moves as little as possible from the one before.");

    velocitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
    velocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
    velocitySlider.setRange (5, 127, 1);
    velocitySlider.onValueChange = [this]
    {
        model.velocity = (float) velocitySlider.getValue() / 127.0f;
        processor.modelChanged();
    };
    addAndMakeVisible (velocitySlider);

    // Footer
    playButton.setClickingTogglesState (false);
    playButton.onClick = [this] { processor.playing = ! processor.playing.load(); };
    addAndMakeVisible (playButton);
    tempoLabel.setColour (juce::Label::textColourId, Colours::dim);
    addAndMakeVisible (tempoLabel);

    dragZone.makeFile = [this]
    {
        return midiFileIn (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Cruz Chords"));
    };
    addAndMakeVisible (dragZone);

    saveButton.onClick = [this]
    {
        const auto folder = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Cruz Chords");
        auto file = folder.getChildFile (processor.suggestedMidiName() + ".mid");
        if (file.exists())
            file = file.getNonexistentSibling (true);
        if (! folder.createDirectory() || ! processor.writeMidiFile (file))
        {
            statusLabel.setText ("Could not save the MIDI file.", juce::dontSendNotification);
            return;
        }
        statusLabel.setText ("Saved to Music > Cruz Chords", juce::dontSendNotification);
        file.revealToUser();
    };
    addAndMakeVisible (saveButton);

    hintLabel.setColour (juce::Label::textColourId, Colours::dim);
    hintLabel.setFont (font (12.0f));
    hintLabel.setText ("Play loops the progression. While FL Studio's song plays, it follows the song's position and tempo.",
                       juce::dontSendNotification);
    addAndMakeVisible (hintLabel);

    fillSoundBox();
    syncFromModel();
    processor.addChangeListener (this);
    startTimerHz (30);

    setSize (1080, 610);
}

CruzChordsAudioProcessorEditor::~CruzChordsAudioProcessorEditor()
{
    processor.removeChangeListener (this);
    for (int d = 0; d < 7; ++d)
        if (! padHeld[d].empty())
            processor.audition (padHeld[d], false);
    setLookAndFeel (nullptr);
}

//==============================================================================
void CruzChordsAudioProcessorEditor::fillSoundBox()
{
    soundBox.clear (juce::dontSendNotification);
    soundBox.addItem ("Built-in keys", 1);
    soundBox.addItem ("Built-in pad", 2);
    soundBox.addSeparator();
    soundBox.addItem ("Find a synth file...", 3);

    pluginFiles = CruzChordsAudioProcessor::findPluginFiles();
    if (! pluginFiles.isEmpty())
    {
        soundBox.addSectionHeading ("Your plugins (VST3)");
        for (int i = 0; i < pluginFiles.size(); ++i)
            soundBox.addItem (pluginFiles[i].getFileNameWithoutExtension(), 100 + i);
    }
}

void CruzChordsAudioProcessorEditor::soundChosen()
{
    const int choice = soundBox.getSelectedId();
    statusLabel.setText ({}, juce::dontSendNotification);

    if (choice == 1 || choice == 2)
        processor.useBuiltInSound (choice - 1);
    else if (choice == 3)
    {
        auto start = juce::File ("/Library/Audio/Plug-Ins/VST3");
        if (! start.isDirectory())
            start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        chooser = std::make_unique<juce::FileChooser> ("Pick a synth (.vst3 or .component)", start, "*.vst3;*.component");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<CruzChordsAudioProcessorEditor> (this)] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr)
                                      return;
                                  const auto f = fc.getResult();
                                  if (f != juce::File())
                                  {
                                      safe->statusLabel.setText ("Loading " + f.getFileNameWithoutExtension() + "...", juce::dontSendNotification);
                                      safe->processor.loadPluginFile (f);
                                  }
                                  safe->syncFromModel();
                              });
    }
    else if (choice >= 100 && choice - 100 < pluginFiles.size())
    {
        statusLabel.setText ("Loading " + pluginFiles[choice - 100].getFileNameWithoutExtension() + "...", juce::dontSendNotification);
        processor.loadPluginFile (pluginFiles[choice - 100]);
    }
    syncFromModel();
}

void CruzChordsAudioProcessorEditor::fillPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    const auto genre = genreBox.getText();
    for (int i = 0; i < (int) presets().size(); ++i)
        if (genre == presets()[(size_t) i].genre)
            presetBox.addItem (presets()[(size_t) i].name, i + 1);
    if (model.presetIndex >= 0)
        presetBox.setSelectedId (model.presetIndex + 1, juce::dontSendNotification);
}

void CruzChordsAudioProcessorEditor::stepPreset (int direction)
{
    std::vector<int> inGenre;
    for (int i = 0; i < (int) presets().size(); ++i)
        if (genreBox.getText() == presets()[(size_t) i].genre)
            inGenre.push_back (i);
    if (inGenre.empty())
        return;

    const auto it = std::find (inGenre.begin(), inGenre.end(), model.presetIndex);
    int pos = it == inGenre.end() ? -1 : (int) (it - inGenre.begin());
    const int count = (int) inGenre.size();

    if (direction == 0)
    {
        // Surprise: a different progression from this genre, in a random key.
        int next = juce::Random::getSystemRandom().nextInt (count);
        if (count > 1 && next == pos)
            next = (next + 1) % count;
        pos = next;
        model.key = juce::Random::getSystemRandom().nextInt (12);
    }
    else
        pos = pos < 0 ? 0 : (pos + direction + count) % count;

    selectSlot (-1);
    processor.loadPreset (inGenre[(size_t) pos]);
}

void CruzChordsAudioProcessorEditor::selectSlot (int s)
{
    selectedSlot = s;
    syncFromModel();
}

void CruzChordsAudioProcessorEditor::editSelected (const std::function<void (Chord&)>& f)
{
    if (selectedSlot < 0 || selectedSlot >= (int) model.chords.size())
        return;
    f (model.chords[(size_t) selectedSlot]);
    processor.modelChanged();
}

juce::File CruzChordsAudioProcessorEditor::midiFileIn (const juce::File& folder) const
{
    folder.createDirectory();
    const auto file = folder.getChildFile (processor.suggestedMidiName() + ".mid");
    processor.writeMidiFile (file);
    return file;
}

void CruzChordsAudioProcessorEditor::syncFromModel()
{
    const bool flats = processor.useFlats();

    keyBox.clear (juce::dontSendNotification);
    for (int pc = 0; pc < 12; ++pc)
        keyBox.addItem (noteName (pc, flats), pc + 1);
    keyBox.setSelectedId (model.key + 1, juce::dontSendNotification);
    scaleBox.setSelectedId (model.scale + 1, juce::dontSendNotification);

    if (model.presetIndex >= 0)
    {
        const juce::String genre = presets()[(size_t) model.presetIndex].genre;
        if (genreBox.getText() != genre)
        {
            for (int i = 0; i < genreBox.getNumItems(); ++i)
                if (genreBox.getItemText (i) == genre)
                    genreBox.setSelectedItemIndex (i, juce::dontSendNotification);
        }
    }
    else if (genreBox.getSelectedId() == 0)
        genreBox.setSelectedItemIndex (0, juce::dontSendNotification);
    fillPresetBox();
    if (model.presetIndex < 0)
        presetBox.setText ("Your own", juce::dontSendNotification);

    for (int i = 0; i < 3; ++i)
        sizeButtons[i].setToggleState (model.size == i, juce::dontSendNotification);

    for (int d = 0; d < 7; ++d)
    {
        const auto c = processor.padChord (d);
        pads[d]->setChord (chordName (c, model.key, flats), juce::String::fromUTF8 (romanNumeral (c).c_str()));
    }

    // Selected chord
    if (selectedSlot >= (int) model.chords.size())
        selectedSlot = -1;
    strip.setSelected (selectedSlot);
    strip.repaint();

    const bool hasSel = selectedSlot >= 0;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &rootBox, &typeBox, &lengthBox, &invDown, &invUp, &octDown, &octUp,
                                                                         &moveLeft, &moveRight, &deleteButton, &duplicateButton })
        c->setEnabled (hasSel);
    clearButton.setEnabled (! model.chords.empty());

    if (hasSel)
    {
        const auto& c = model.chords[(size_t) selectedSlot];
        rootBox.setSelectedId ((model.key + c.offset) % 12 + 1, juce::dontSendNotification);
        typeBox.setSelectedId (c.type + 1, juce::dontSendNotification);
        int li = -1;
        for (int i = 0; i < (int) std::size (lengthChoices); ++i)
            if (same (lengthChoices[i], c.beats))
                li = i;
        if (li >= 0)
            lengthBox.setSelectedItemIndex (li, juce::dontSendNotification);
        else
            lengthBox.setText (lengthText (c.beats), juce::dontSendNotification);

        // Smooth voice leading picks inversions for every chord after the first.
        const bool autoInv = model.smooth && selectedSlot > 0;
        invDown.setEnabled (! autoInv);
        invUp.setEnabled (! autoInv);
        invValue.setText (autoInv ? "Auto" : juce::String (c.inversion), juce::dontSendNotification);
        octValue.setText ((c.octave > 0 ? "+" : "") + juce::String (c.octave), juce::dontSendNotification);
    }
    else
    {
        rootBox.setSelectedId (0, juce::dontSendNotification);
        typeBox.setSelectedId (0, juce::dontSendNotification);
        lengthBox.setSelectedId (0, juce::dontSendNotification);
        rootBox.setTextWhenNothingSelected ("Click a chord");
        invValue.setText ({}, juce::dontSendNotification);
        octValue.setText ({}, juce::dontSendNotification);
    }

    rhythmBox.setSelectedItemIndex (model.rhythm, juce::dontSendNotification);
    spreadBox.setSelectedItemIndex (model.spread, juce::dontSendNotification);
    smoothToggle.setToggleState (model.smooth, juce::dontSendNotification);
    bassToggle.setToggleState (model.bass, juce::dontSendNotification);
    keysToggle.setToggleState (model.keysPlayChords, juce::dontSendNotification);
    velocitySlider.setValue (std::round (model.velocity * 127.0f), juce::dontSendNotification);

    // Sound
    if (processor.hasHostedSynth())
    {
        soundBox.setText (processor.getSoundName(), juce::dontSendNotification);
        openSynthButton.setEnabled (true);
    }
    else
    {
        soundBox.setSelectedId (model.sound + 1, juce::dontSendNotification);
        openSynthButton.setEnabled (false);
    }
    if (processor.getLastLoadError().isNotEmpty())
        statusLabel.setText (processor.getLastLoadError(), juce::dontSendNotification);
    else if (statusLabel.getText().startsWith ("Loading"))
        statusLabel.setText ({}, juce::dontSendNotification);
}

void CruzChordsAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    syncFromModel();
}

void CruzChordsAudioProcessorEditor::timerCallback()
{
    const bool on = processor.playing.load();
    strip.setPlaying (on ? processor.getPlayingSlot() : -1);
    if (playButton.getToggleState() != on)
    {
        playButton.setToggleState (on, juce::dontSendNotification);
        playButton.setButtonText (on ? "Stop" : "Play");
    }

    const auto text = juce::String (processor.getHostBpm(), 1) + " BPM"
                      + (processor.isHostPlaying() ? "  -  following the song" : "");
    if (tempoLabel.getText() != text)
        tempoLabel.setText (text, juce::dontSendNotification);
}

//==============================================================================
void CruzChordsAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::bg);

    auto header = getLocalBounds().removeFromTop (60);
    g.setColour (Colours::panel);
    g.fillRect (header);
    g.setColour (Colours::accent);
    g.setFont (font (24.0f, true));
    g.drawText ("CRUZ", header.withTrimmedLeft (22).withWidth (80), juce::Justification::centredLeft);
    g.setColour (Colours::text);
    g.drawText ("CHORDS", header.withTrimmedLeft (90).withWidth (140), juce::Justification::centredLeft);

    auto section = [&] (juce::Rectangle<int> r, const juce::String& title)
    {
        g.setColour (Colours::text);
        g.setFont (font (15.0f, true));
        g.drawText (title, r, juce::Justification::centredLeft);
    };
    section (padsTitle, "Chords in your key");
    section (stripTitle, "Your progression");
    section (feelTitle, "Feel");

    g.setColour (Colours::dim);
    g.setFont (font (12.0f));
    g.drawText ("Click a chord to hear it. With a chord selected below, clicking a pad swaps it in.",
                padsTitle.withTrimmedLeft (170), juce::Justification::centredLeft);
    g.drawText ("Click a chord to hear and edit it.", stripTitle.withTrimmedLeft (150), juce::Justification::centredLeft);
}

void CruzChordsAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // Header: sound picker on the right.
    {
        auto h = area.removeFromTop (60).reduced (16, 14);
        volumeSlider.setBounds (h.removeFromRight (190));
        h.removeFromRight (10);
        openSynthButton.setBounds (h.removeFromRight (110));
        h.removeFromRight (8);
        soundBox.setBounds (h.removeFromRight (250));
        h.removeFromRight (8);
        statusLabel.setBounds (h.withTrimmedLeft (220));
    }

    area.reduce (20, 0);
    area.removeFromTop (12);

    // Key, scale and presets.
    {
        auto row = area.removeFromTop (32);
        keyLabel.setBounds (row.removeFromLeft (32));
        keyBox.setBounds (row.removeFromLeft (70));
        row.removeFromLeft (16);
        scaleLabel.setBounds (row.removeFromLeft (42));
        scaleBox.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (28);
        genreLabel.setBounds (row.removeFromLeft (48));
        genreBox.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (16);
        presetLabel.setBounds (row.removeFromLeft (82));
        presetBox.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (6);
        prevButton.setBounds (row.removeFromLeft (32));
        row.removeFromLeft (4);
        nextButton.setBounds (row.removeFromLeft (32));
        row.removeFromLeft (12);
        surpriseButton.setBounds (row.removeFromLeft (120));
    }

    area.removeFromTop (14);
    {
        auto row = area.removeFromTop (28);
        padsTitle = row;
        auto right = row.removeFromRight (3 * 70 + 8);
        for (auto& b : sizeButtons)
        {
            b.setBounds (right.removeFromLeft (70).reduced (2, 0));
        }
    }
    area.removeFromTop (4);
    {
        auto row = area.removeFromTop (104);
        const int w = row.getWidth() / 7;
        for (auto* pad : pads)
            pad->setBounds (row.removeFromLeft (w));
    }

    area.removeFromTop (14);
    stripTitle = area.removeFromTop (28);
    {
        auto r = stripTitle;
        moveRight.setBounds (r.removeFromRight (96).reduced (0, 1));
        r.removeFromRight (6);
        moveLeft.setBounds (r.removeFromRight (96).reduced (0, 1));
    }
    area.removeFromTop (4);
    strip.setBounds (area.removeFromTop (100));
    area.removeFromTop (8);
    {
        auto row = area.removeFromTop (30);
        rootLabel.setBounds (row.removeFromLeft (36));
        rootBox.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (12);
        typeLabel.setBounds (row.removeFromLeft (36));
        typeBox.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (12);
        lengthLabel.setBounds (row.removeFromLeft (48));
        lengthBox.setBounds (row.removeFromLeft (92));
        row.removeFromLeft (12);
        invLabel.setBounds (row.removeFromLeft (64));
        invDown.setBounds (row.removeFromLeft (26));
        invValue.setBounds (row.removeFromLeft (40));
        invUp.setBounds (row.removeFromLeft (26));
        row.removeFromLeft (10);
        octLabel.setBounds (row.removeFromLeft (48));
        octDown.setBounds (row.removeFromLeft (26));
        octValue.setBounds (row.removeFromLeft (32));
        octUp.setBounds (row.removeFromLeft (26));

        clearButton.setBounds (row.removeFromRight (78));
        row.removeFromRight (6);
        deleteButton.setBounds (row.removeFromRight (64));
        row.removeFromRight (6);
        duplicateButton.setBounds (row.removeFromRight (82));
    }
    area.removeFromTop (8);
    feelTitle = area.removeFromTop (26);
    {
        auto row = area.removeFromTop (30);
        rhythmLabel.setBounds (row.removeFromLeft (54));
        rhythmBox.setBounds (row.removeFromLeft (190));
        row.removeFromLeft (16);
        spreadLabel.setBounds (row.removeFromLeft (56));
        spreadBox.setBounds (row.removeFromLeft (90));
        row.removeFromLeft (16);
        smoothToggle.setBounds (row.removeFromLeft (172));
        bassToggle.setBounds (row.removeFromLeft (100));
        keysToggle.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (8);
        velocityLabel.setBounds (row.removeFromLeft (58));
        velocitySlider.setBounds (row);
    }

    area.removeFromTop (18);
    {
        auto row = area.removeFromTop (60);
        playButton.setBounds (row.removeFromLeft (130));
        row.removeFromLeft (14);
        tempoLabel.setBounds (row.removeFromLeft (230));
        saveButton.setBounds (row.removeFromRight (150).reduced (0, 10));
        row.removeFromRight (14);
        dragZone.setBounds (row.removeFromRight (300));
    }
    area.removeFromTop (6);
    hintLabel.setBounds (area.removeFromTop (20));
}
