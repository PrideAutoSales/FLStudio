# Cruz Tape: a two-track tape machine. JUCE is fetched by the root CMakeLists.

juce_add_plugin(CruzTape
    VERSION 0.1.0
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruztape
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crtp
    PRODUCT_NAME "Cruz Tape"
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzTape
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CruzTape
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzTape
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
