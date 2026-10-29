juce_add_plugin(CruzVerb
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzverb
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crrv
    PRODUCT_NAME "Cruz Verb"
    VERSION 0.2.0
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzVerb
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ReverbDisplay.cpp
        Source/ReverbEngine.cpp)

target_compile_definitions(CruzVerb
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzVerb
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
