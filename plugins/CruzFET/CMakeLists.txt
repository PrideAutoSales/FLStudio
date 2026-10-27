# Cruz FET: FET peak compressor.

set(CRUZ_FET_FORMATS VST3)
if(APPLE)
    list(APPEND CRUZ_FET_FORMATS AU)
endif()

juce_add_plugin(CruzFET
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzfet
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crfe
    PRODUCT_NAME "Cruz FET"
    VERSION 0.1.1
    FORMATS ${CRUZ_FET_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzFET
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(CruzFET
    PRIVATE
        Source
        ${CMAKE_CURRENT_SOURCE_DIR}/../CruzCompShared)

target_compile_definitions(CruzFET
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzFET
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
