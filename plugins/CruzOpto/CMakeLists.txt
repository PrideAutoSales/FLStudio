# Cruz Opto: optical leveling compressor.

set(CRUZ_OPTO_FORMATS VST3)
if(APPLE)
    list(APPEND CRUZ_OPTO_FORMATS AU)
endif()

juce_add_plugin(CruzOpto
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzopto
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crla
    PRODUCT_NAME "Cruz Opto"
    VERSION 0.2.0
    FORMATS ${CRUZ_OPTO_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzOpto
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_include_directories(CruzOpto
    PRIVATE
        Source
        ${CMAKE_CURRENT_SOURCE_DIR}/../CruzCompShared)

target_compile_definitions(CruzOpto
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzOpto
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
