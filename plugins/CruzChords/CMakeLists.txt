# Cruz Chords: a chord progression instrument (VST3 + AU). It plays through a
# built-in sound or any synth loaded inside it, and exports MIDI.
# Built from the root CMakeLists, which already fetches JUCE.

juce_add_plugin(CruzChords
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzchords
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crch
    PRODUCT_NAME "Cruz Chords"
    VERSION 0.2.0
    FORMATS VST3 AU
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(CruzChords
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(CruzChords PRIVATE Source)

target_compile_definitions(CruzChords
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        # Load other synths inside Cruz Chords.
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=1)

target_link_libraries(CruzChords
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine checks that run without a plugin host: cmake --build build --target CruzChordsEngineTest
add_executable(CruzChordsEngineTest EXCLUDE_FROM_ALL tests/EngineTest.cpp)
target_include_directories(CruzChordsEngineTest PRIVATE Source)
target_compile_features(CruzChordsEngineTest PRIVATE cxx_std_20)
