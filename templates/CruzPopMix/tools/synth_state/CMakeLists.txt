# Saves Cruz Synth factory presets as plugin state, for ../../synth_states.
#   cmake -B build -DJUCE_DIR=/path/to/JUCE-8.0.12 -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#   build/SynthState_artefacts/Release/SynthState "Soft Rhodes" keys.bin "Round Bass" bass.bin
# Each .bin is JUCE's binary XML ('VC2!', length, XML); the XML part is what
# synth_states/*.xml holds.
cmake_minimum_required(VERSION 3.22)
project(StateGen VERSION 0.1.0)

add_subdirectory(${JUCE_DIR} juce)

set(SYNTH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../plugins/CruzSynth/Source)

juce_add_console_app(SynthState PRODUCT_NAME "SynthState")
target_sources(SynthState PRIVATE main.cpp
    ${SYNTH}/Parameters.cpp ${SYNTH}/Wavetable.cpp ${SYNTH}/Engine.cpp ${SYNTH}/Effects.cpp
    ${SYNTH}/PluginProcessor.cpp ${SYNTH}/PluginEditor.cpp ${SYNTH}/UI/Theme.cpp ${SYNTH}/UI/Controls.cpp
    ${SYNTH}/UI/Displays.cpp ${SYNTH}/UI/WavetableEditor.cpp ${SYNTH}/UI/Pages.cpp ${SYNTH}/FactoryPresets.cpp)
target_include_directories(SynthState PRIVATE ${SYNTH})
target_compile_definitions(SynthState PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JucePlugin_Name="Cruz Synth" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1
    JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JUCE_ALSA=0 JUCE_JACK=0)
target_link_libraries(SynthState PRIVATE juce::juce_audio_utils juce::juce_dsp
    juce::juce_recommended_config_flags)
