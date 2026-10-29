# Cruz Synth: a wavetable synthesizer instrument (VST3 + AU).
# Built from the root CMakeLists, which already fetches JUCE.

juce_add_plugin(CruzSynth
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzsynth
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crsy
    PRODUCT_NAME "Cruz Synth"
    VERSION 0.4.0
    FORMATS VST3 AU
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(CruzSynth
    PRIVATE
        Source/Parameters.cpp
        Source/Wavetable.cpp
        Source/Engine.cpp
        Source/Piano.cpp
        Source/Effects.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/UI/Theme.cpp
        Source/UI/Controls.cpp
        Source/UI/Displays.cpp
        Source/UI/WavetableEditor.cpp
        Source/UI/Pages.cpp
        Source/FactoryPresets.cpp)

target_include_directories(CruzSynth PRIVATE Source)

target_compile_definitions(CruzSynth
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
