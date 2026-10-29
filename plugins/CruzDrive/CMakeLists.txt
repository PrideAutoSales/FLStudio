# Cruz Drive: analog-style saturation with a crush compressor.
# Built from the root CMakeLists, which fetches JUCE.

juce_add_plugin(CruzDrive
    VERSION 0.2.0
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzdrive
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Crds
    PRODUCT_NAME "Cruz Drive"
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzDrive
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

target_compile_definitions(CruzDrive
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzDrive
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline checks of the signal path (not part of the Mac build).
option(CRUZDRIVE_TESTS "Build the Cruz Drive test app" OFF)
if(CRUZDRIVE_TESTS)
    juce_add_console_app(CruzDriveTests PRODUCT_NAME "CruzDriveTests")
    target_sources(CruzDriveTests PRIVATE tests/DriveTests.cpp)
    target_include_directories(CruzDriveTests PRIVATE Source)
    target_compile_definitions(CruzDriveTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(CruzDriveTests
        PRIVATE
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    juce_add_console_app(CruzDriveSnapshot PRODUCT_NAME "CruzDriveSnapshot")
    target_sources(CruzDriveSnapshot PRIVATE tests/Snapshot.cpp)
    # Reuse the plugin's compiled code, including its JUCE modules.
    target_include_directories(CruzDriveSnapshot PRIVATE Source $<TARGET_PROPERTY:CruzDrive,INCLUDE_DIRECTORIES>)
    target_compile_definitions(CruzDriveSnapshot PRIVATE $<TARGET_PROPERTY:CruzDrive,COMPILE_DEFINITIONS>)
    target_link_libraries(CruzDriveSnapshot PRIVATE CruzDrive)
endif()
