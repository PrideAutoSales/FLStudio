#pragma once

#include "PluginProcessor.h"
#include "UI/Pages.h"
#include "UI/WavetableEditor.h"
#include "EditorRedrawGuard.h"

class CruzSynthEditor : public juce::AudioProcessorEditor,
                        public juce::DragAndDropContainer,
                        private juce::ChangeListener,
                        private juce::Timer
{
public:
    explicit CruzSynthEditor (CruzSynthProcessor&);
    ~CruzSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Draw on the main thread instead of in the background. Some hosts (FL
    // Studio on macOS) otherwise show an empty window on the first open.
    bool wantsLayerBackedView() const override { return false; }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void showPage (int index);
    void openTableEditor (int osc);
    void closeTableEditor();
    void showPresetMenu();
    void stepPreset (int delta);
    void savePreset();
    void loadPreset();

    CruzSynthProcessor& proc;
    cs::ui::CruzLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    // Same tab order as Serum, so the workflow feels familiar.
    juce::TextButton tabs[4] { juce::TextButton ("OSC"), juce::TextButton ("FX"), juce::TextButton ("MATRIX"), juce::TextButton ("GLOBAL") };
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    cs::ui::ModKnob masterKnob;
    cs::ui::ScopeView scope;

    cs::ui::OscPage oscPage;
    cs::ui::MatrixPage matrixPage;
    cs::ui::FxPage fxPage;
    cs::ui::GlobalPage globalPage;
    std::unique_ptr<cs::ui::WavetableEditor> tableEditor;
    juce::MidiKeyboardComponent keyboard;

    std::unique_ptr<juce::FileChooser> chooser;
    int page = 0;
    int lastVoices = -1;

    cruz::EditorRedrawGuard redrawGuard { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzSynthEditor)
};
