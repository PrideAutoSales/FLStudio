juce_add_plugin(CruzLimiter
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzlimiter
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crlm
    PRODUCT_NAME "Cruz Limiter"
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics Mastering
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzLimiter
    PRIVATE
        Source/LimiterEngine.cpp
        Source/LoudnessMeter.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CruzLimiter
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzLimiter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline checks for the limiter engine: ceiling, true peak, latency and loudness.
option(CRUZ_LIMITER_TESTS "Build the Cruz Limiter engine tests" OFF)

if(CRUZ_LIMITER_TESTS)
    juce_add_console_app(CruzLimiterTests PRODUCT_NAME "CruzLimiterTests")

    target_sources(CruzLimiterTests
        PRIVATE
            Tests/LimiterTests.cpp
            Source/LimiterEngine.cpp
            Source/LoudnessMeter.cpp)

    target_compile_definitions(CruzLimiterTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)

    target_link_libraries(CruzLimiterTests
        PRIVATE
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
