#pragma once

#include "PluginProcessor.h"
#include "UI/Pages.h"
#include "UI/WavetableEditor.h"

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

    juce::TextButton tabs[4] { juce::TextButton ("OSC"), juce::TextButton ("MATRIX"), juce::TextButton ("FX"), juce::TextButton ("GLOBAL") };
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CruzSynthEditor)
};
