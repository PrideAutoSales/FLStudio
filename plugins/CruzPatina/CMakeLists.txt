# Cruz Patina: vintage and lo-fi colour (noise, wobble, distortion, digital, space, magnetic).
juce_add_plugin(CruzPatina
    VERSION 0.2.0
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzpatina
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crrc
    PRODUCT_NAME "Cruz Patina"
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzPatina
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CruzPatina
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzPatina
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
