// Writes the state of named Cruz Synth factory presets: SynthState "Preset" out.bin ...
#include "PluginProcessor.h"
#include <iostream>
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<juce::AudioProcessor> p (createPluginFilter());
    p->setRateAndBufferSizeDetails (48000.0, 512);
    for (int i = 1; i + 1 < argc; i += 2)
    {
        auto name = juce::String (argv[i]);
        int idx = -1;
        for (int k = 0; k < p->getNumPrograms(); ++k)
            if (p->getProgramName (k) == name) idx = k;
        if (idx < 0) { std::cerr << "missing " << name << "\n"; return 1; }
        p->setCurrentProgram (idx);
        juce::MemoryBlock mb;
        p->getStateInformation (mb);
        juce::File (argv[i + 1]).replaceWithData (mb.getData(), mb.getSize());
        std::cout << name << " idx " << idx << " bytes " << mb.getSize() << "\n";
    }
    return 0;
}
