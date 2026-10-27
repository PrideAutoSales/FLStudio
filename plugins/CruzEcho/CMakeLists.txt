# Cruz Echo: a character delay. Built from the root CMakeLists, which fetches JUCE.

juce_add_plugin(CruzEcho
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzecho
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crdy
    PRODUCT_NAME "Cruz Echo"
    VERSION 0.1.0
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzEcho
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CruzEcho
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzEcho
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine checks that run without a plugin host: cmake --build build --target CruzEchoEngineTest
add_executable(CruzEchoEngineTest EXCLUDE_FROM_ALL tests/EngineTest.cpp)
target_include_directories(CruzEchoEngineTest PRIVATE Source)
