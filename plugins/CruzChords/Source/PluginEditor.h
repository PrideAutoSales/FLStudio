#pragma once

#include "EditorRedrawGuard.h"
#include "PluginProcessor.h"

namespace cruzchordsui
{
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;
};

// One chord of the key. Click to hear it, click "+ Add" to put it in the progression.
class ChordPad final : public juce::Component
{
public:
    void setChord (const juce::String& name, const juce::String& numeral);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    std::function<void (bool)> onPress;
    std::function<void()> onAdd;

private:
    bool inAddBar (juce::Point<int> p) const { return p.y > getHeight() - 28; }

    juce::String name, numeral;
    bool down = false, hoverAdd = false;
};

// The progression: one block per chord, as wide as it is long.
class ProgressionStrip final : public juce::Component
{
public:
    explicit ProgressionStrip (CruzChordsAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setSelected (int);
    void setPlaying (int);

    std::function<void (int)> onSelect;
    std::function<void (int, bool)> onAudition;
    std::function<void (int)> onToggleLock;

private:
    // One chord can wrap onto the next row, so it may have more than one piece.
    struct Segment
    {
        int slot;
        juce::Rectangle<float> bounds;
        bool first;
    };
    std::vector<Segment> segments() const;

    CruzChordsAudioProcessor& processor;
    int selected = -1, playing = -1, held = -1;
};

// Drag from here onto FL Studio's playlist or a channel to drop the progression in as MIDI.
class MidiDragZone final : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

    std::function<juce::File()> makeFile;

private:
    bool dragging = false;
};
} // namespace cruzchordsui

class CruzChordsAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::ChangeListener,
                                             private juce::Timer
{
public:
    explicit CruzChordsAudioProcessorEditor (CruzChordsAudioProcessor&);
    ~CruzChordsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void syncFromModel();
    void fillSoundBox();
    void fillPresetBox();
    void soundChosen();
    void editSelected (const std::function<void (cruzchords::Chord&)>&);
    void selectSlot (int);
    void stepPreset (int direction);
    juce::File midiFileIn (const juce::File& folder) const;

    CruzChordsAudioProcessor& processor;
    CruzChordsAudioProcessor::Model& model;
    cruzchordsui::LookAndFeel lookAndFeel;

    juce::ComboBox soundBox;
    juce::TextButton openSynthButton { "Open synth" };
    juce::Slider volumeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    juce::Label statusLabel;

    juce::Label keyLabel, scaleLabel, genreLabel, presetLabel;
    juce::ComboBox keyBox, scaleBox, genreBox, presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" }, surpriseButton { "Surprise me" };

    juce::TextButton sizeButtons[3];
    juce::OwnedArray<cruzchordsui::ChordPad> pads;
    std::vector<int> padHeld[7];
    std::vector<int> stripHeld;

    cruzchordsui::ProgressionStrip strip;
    juce::Label rootLabel, typeLabel, invLabel, octLabel, lengthLabel;
    juce::ComboBox rootBox, typeBox, lengthBox;
    juce::TextButton invDown { "-" }, invUp { "+" }, octDown { "-" }, octUp { "+" };
    juce::Label invValue, octValue;
    juce::TextButton lockButton { "Lock" };
    juce::TextButton moveLeft { "Move left" }, moveRight { "Move right" }, deleteButton { "Delete" },
        duplicateButton { "Duplicate" }, clearButton { "Clear all" };

    juce::Label rhythmLabel, spreadLabel, velocityLabel;
    juce::ComboBox rhythmBox, spreadBox;
    juce::ToggleButton smoothToggle { "Smooth voice leading" }, bassToggle { "Bass note" },
        keysToggle { "Keys play chords" };
    juce::Slider velocitySlider;

    // Shuffle and whole-progression controls.
    juce::TextButton shuffleButton { "Shuffle" };
    juce::Label traditionalLabel, adventurousLabel, barsLabel, everyLabel, allLabel, allOctLabel, allInvLabel;
    juce::Slider adventureSlider;
    juce::ComboBox barsBox, everyBox;
    juce::TextButton allOctDown { "-" }, allOctUp { "+" }, allInvDown { "-" }, allInvUp { "+" };
    juce::Label allOctValue, allInvValue;

    juce::TextButton playButton { "Play" };
    juce::ToggleButton followToggle { "Play with FL (space bar)" };
    juce::Label tempoLabel;
    cruzchordsui::MidiDragZone dragZone;
    juce::TextButton saveButton { "Save MIDI file" };
    juce::Label hintLabel;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> pluginFiles;
    int selectedSlot = -1;
    juce::String stripInfo;
    juce::Rectangle<int> padsTitle, stripTitle, feelTitle;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzChordsAudioProcessorEditor)
};
