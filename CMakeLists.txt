cmake_minimum_required(VERSION 3.22)

# Must be set before project() to take effect.
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(CruzEQ VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.12
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CruzEQ
    COMPANY_NAME "Cruz Audio"
    BUNDLE_ID com.cruzaudio.cruzeq
    PLUGIN_MANUFACTURER_CODE Cruz
    PLUGIN_CODE Creq
    PRODUCT_NAME "Cruz EQ"
    FORMATS VST3 AU
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(CruzEQ
    PRIVATE
        Source/Parameters.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/BandPanel.cpp
        Source/ui/EqDisplay.cpp)

target_compile_definitions(CruzEQ
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CruzEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Accuracy, stability and speed tests for the EQ engine (no JUCE needed).
enable_testing()
add_executable(CruzEQTests Tests/DspTests.cpp)
target_include_directories(CruzEQTests PRIVATE Source)
target_link_libraries(CruzEQTests PRIVATE juce::juce_recommended_config_flags)
add_test(NAME dsp COMMAND CruzEQTests)

# Every other plugin lives in its own folder under plugins/ and is picked up
# automatically, so adding a plugin never needs a change to this file.
add_subdirectory(plugins)
