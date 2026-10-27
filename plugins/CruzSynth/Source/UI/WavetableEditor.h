#pragma once

#include "../PluginProcessor.h"
#include "Theme.h"

namespace cs::ui
{
// Full-page editor for one oscillator's wavetable: draw frames by hand, edit
// harmonics, add or remove frames, morph between them, import and export WAV.
class WavetableEditor : public juce::Component
{
public:
    WavetableEditor (CruzSynthProcessor& p, int osc, std::function<void()> onClose);
    ~WavetableEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Re-reads the table from the processor (e.g. after a preset change).
    void reload();

private:
    class DrawArea;
    class HarmonicArea;
    class FrameStrip;

    float* frame (int f) { return frames.data() + (size_t) f * Wavetable::kSize; }
    int numFrames() const { return (int) (frames.size() / Wavetable::kSize); }
    void setCurrent (int f);
    void commit();
    void edited();
    void importFile();
    void exportFile();

    CruzSynthProcessor& proc;
    int osc;
    std::function<void()> onClose;
    std::vector<float> frames;
    juce::String name;
    int current = 0;

    std::unique_ptr<DrawArea> drawArea;
    std::unique_ptr<HarmonicArea> harmonicArea;
    std::unique_ptr<FrameStrip> strip;
    juce::Slider frameSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::TextButton importButton { "Import WAV..." }, exportButton { "Export WAV..." }, closeButton { "Done" };
    juce::TextButton addButton { "Add frame" }, deleteButton { "Delete frame" }, fadeButton { "Crossfade fill" },
                     spectralButton { "Spectral fill" }, normaliseButton { "Normalize" }, smoothButton { "Smooth" },
                     invertButton { "Invert" }, reverseButton { "Reverse" }, sineButton { "Sine" }, factoryButton { "Load factory..." };
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace cs::ui
