// Renders the Cruz Drive editor to a PNG so the layout can be checked without a DAW.
// Usage: CruzDriveSnapshot <out.png> [presetIndex]

#include "PluginProcessor.h"

int main (int argc, char** argv)
{
    if (argc < 2)
        return 1;

    juce::ScopedJuceInitialiser_GUI gui;
    std::unique_ptr<juce::AudioProcessor> processor (createPluginFilter());
    if (argc > 2)
        processor->setCurrentProgram (juce::String (argv[2]).getIntValue());

    // Feed some audio so the meters and curve dot have something to show.
    processor->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    for (int b = 0; b < 40; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                buffer.setSample (c, i, 0.5f * std::sin ((float) (b * 512 + i) * 0.03f));
        processor->processBlock (buffer, midi);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor->createEditor());

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.5f);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat().writeImageToStream (image, stream);
    editor.reset();
    return 0;
}
