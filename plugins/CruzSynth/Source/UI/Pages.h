#pragma once

#include "Controls.h"
#include "Displays.h"

namespace cs::ui
{
// Base for panels that own a pile of child controls.
class Panel : public juce::Component
{
public:
    explicit Panel (juce::String panelTitle) : title (std::move (panelTitle)) {}
    void paint (juce::Graphics& g) override { drawPanel (g, getLocalBounds(), title); }

protected:
    template <typename T, typename... Args>
    T& make (Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        addAndMakeVisible (ref);
        owned.push_back (std::move (c));
        return ref;
    }

    juce::String title;
    std::vector<std::unique_ptr<juce::Component>> owned;
};

//==============================================================================
class OscPanel : public Panel
{
public:
    OscPanel (CruzSynthProcessor& p, int osc, std::function<void (int)> onEdit);
    void resized() override;
    void refreshTableName();

private:
    CruzSynthProcessor& proc;
    int osc;
    PowerButton* power;
    juce::ComboBox* tableBox;
    juce::TextButton* editButton;
    ParamToggle* toFilter;
    WavetableView* view;
    ParamCombo* warp;
    std::vector<ModKnob*> row1, row2;
};

class SubPanel : public Panel
{
public:
    explicit SubPanel (CruzSynthProcessor& p);
    void resized() override;

private:
    PowerButton* power; ParamCombo* shape; ParamToggle* toFilter;
    ModKnob* octave; ModKnob* level; ModKnob* pan;
};

class NoisePanel : public Panel
{
public:
    explicit NoisePanel (CruzSynthProcessor& p);
    void resized() override;

private:
    PowerButton* power; ParamCombo* type; ParamToggle* toFilter;
    ModKnob* color; ModKnob* level; ModKnob* pan;
};

class FilterPanel : public Panel
{
public:
    explicit FilterPanel (CruzSynthProcessor& p);
    void resized() override;

private:
    PowerButton* power; ParamCombo* type; FilterView* view;
    std::vector<ModKnob*> knobs;
};

class ModulatorPanel : public Panel
{
public:
    explicit ModulatorPanel (CruzSynthProcessor& p);
    void resized() override;

private:
    void select (int index); // 0..2 envelopes, 3..6 LFOs
    CruzSynthProcessor& proc;
    int selected = 0;
    std::vector<ModSourceChip*> chips;
    EnvelopeView* envView;
    LfoView* lfoView;
    std::vector<std::vector<juce::Component*>> envControls, lfoControls;
    std::vector<ModKnob*> lfoRate;
    std::vector<ParamCombo*> lfoDiv;
};

class MacroPanel : public Panel
{
public:
    explicit MacroPanel (CruzSynthProcessor& p);
    void resized() override;

private:
    std::vector<ModKnob*> knobs;
    std::vector<ModSourceChip*> macroChips, otherChips;
};

//==============================================================================
class OscPage : public juce::Component
{
public:
    OscPage (CruzSynthProcessor& p, std::function<void (int)> onEditTable);
    void resized() override;
    void refresh();

private:
    OscPanel oscA, oscB;
    SubPanel sub;
    NoisePanel noise;
    FilterPanel filter;
    ModulatorPanel mods;
    MacroPanel macros;
};

//==============================================================================
class MatrixPage : public juce::Component
{
public:
    explicit MatrixPage (CruzSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        std::unique_ptr<ParamCombo> src, dst;
        std::unique_ptr<ParamToggle> bipolar;
        std::unique_ptr<juce::Slider> amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
        std::unique_ptr<juce::TextButton> clear;
    };
    CruzSynthProcessor& proc;
    std::vector<Row> rows;
};

//==============================================================================
class FxPage : public juce::Component
{
public:
    explicit FxPage (CruzSynthProcessor& p);
    ~FxPage() override;
    void resized() override;
    void refresh() { resized(); repaint(); }

private:
    class FxPanel;
    CruzSynthProcessor& proc;
    std::vector<std::unique_ptr<FxPanel>> panels;
};

//==============================================================================
class GlobalPage : public juce::Component
{
public:
    explicit GlobalPage (CruzSynthProcessor& p);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    CruzSynthProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<juce::Component*> voicing;
    juce::Rectangle<int> voicingArea, helpArea;
};
} // namespace cs::ui
