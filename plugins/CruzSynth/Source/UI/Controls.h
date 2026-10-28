#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

namespace cs::ui
{
// Drag descriptions for modulation sources look like "mod:4".
juce::String modDragDescription (int source);
int modSourceFromDescription (const juce::var& description);

//==============================================================================
// A parameter knob. Drop a modulation source on it to route that source here.
// Option-drag adjusts the modulation amount; right-click lists its routings.
class ModKnob : public juce::Slider,
                public juce::DragAndDropTarget,
                private juce::Timer
{
public:
    ModKnob (CruzSynthProcessor& p, const juce::String& paramID, const juce::String& label);
    ~ModKnob() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    int getDestination() const noexcept { return destination; }

    // Draw as a small value box (label and number, drag up or down) instead of a knob.
    void setCompact (bool shouldBeCompact);

private:
    void timerCallback() override;
    void showMenu();
    juce::RangedAudioParameter* slotParam (int slot, const char* name) const;

    CruzSynthProcessor& proc;
    juce::String paramID, label;
    int destination = -1;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    void paintCompact (juce::Graphics&);

    bool compact = false;
    bool dropHover = false, adjustingMod = false;
    int activeSlot = -1;
    float dragStartAmount = 0.0f;
    juce::int64 lastHash = 0;
};

//==============================================================================
// A small pill that can be clicked (to select) and dragged onto knobs.
class ModSourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModSourceChip (int source, juce::String text);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setSelected (bool s) { selected = s; repaint(); }
    std::function<void()> onClick;
    int getSource() const noexcept { return source; }

private:
    int source;
    juce::String text;
    bool selected = false, dragging = false;
};

//==============================================================================
class PowerButton : public juce::ToggleButton
{
public:
    PowerButton (juce::AudioProcessorValueTreeState& s, const juce::String& paramID);

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

class ParamToggle : public juce::ToggleButton
{
public:
    ParamToggle (juce::AudioProcessorValueTreeState& s, const juce::String& paramID, const juce::String& text);

private:
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

class ParamCombo : public juce::ComboBox
{
public:
    ParamCombo (juce::AudioProcessorValueTreeState& s, const juce::String& paramID);

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Lays out knobs in a single row, evenly spaced.
void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items);
} // namespace cs::ui
