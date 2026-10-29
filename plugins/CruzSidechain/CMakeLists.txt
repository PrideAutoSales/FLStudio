# Cruz Sidechain: a sidechain compressor with drawable response curves,
# trigger and ducked-track waveforms, and a low-end clash heat map.

set(CRUZ_SIDECHAIN_FORMATS VST3)
if(APPLE)
    list(APPEND CRUZ_SIDECHAIN_FORMATS AU)
endif()

juce_add_plugin(CruzSidechain
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzsidechain
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crsc
    PRODUCT_NAME "Cruz Sidechain"
    VERSION 0.1.1
    FORMATS ${CRUZ_SIDECHAIN_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzSidechain
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CruzSidechain
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzSidechain
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Engine checks that run without a plugin host: cmake --build build --target CruzSidechainEngineTest
add_executable(CruzSidechainEngineTest EXCLUDE_FROM_ALL tests/EngineTest.cpp)
target_include_directories(CruzSidechainEngineTest PRIVATE Source)
