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
    // Everything is laid out at the design size inside this component, and the
    // whole thing is scaled to the window, so every size looks the same.
    struct Body : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        std::function<void()> onLayout;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
        void resized() override { if (onLayout) onLayout(); }
    };
    static constexpr int kBaseWidth = 1120, kBaseHeight = 790;
    static float loadSavedScale();
    static void saveScale (float scale);

    void paintBody (juce::Graphics&);
    void layoutBody();
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
    Body body;
    juce::TooltipWindow tooltips { this, 600 };

    // Same tab order as Serum, so the workflow feels familiar.
    // A PIANO tab sits next to OSC for the modelled piano.
    static constexpr int kNumTabs = 5;
    juce::TextButton tabs[kNumTabs] { juce::TextButton ("OSC"), juce::TextButton ("PIANO"), juce::TextButton ("FX"),
                                      juce::TextButton ("MATRIX"), juce::TextButton ("GLOBAL") };
    juce::TextButton presetButton, prevButton { "<" }, nextButton { ">" }, saveButton { "Save" };
    cs::ui::ModKnob masterKnob;
    cs::ui::ScopeView scope;

    cs::ui::OscPage oscPage;
    cs::ui::PianoPage pianoPage;
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
