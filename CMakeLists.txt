cmake_minimum_required(VERSION 3.22)

project(Cavernous VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Cavernous
    COMPANY_NAME "Cavernous"
    BUNDLE_ID com.cavernous.audio.plugin
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Cavn
    PLUGIN_CODE Cav1
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Cavernous")

juce_generate_juce_header(Cavernous)

juce_add_binary_data(CavernousData SOURCES Resources/faceplate.png)

target_sources(Cavernous PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Cavernous PRIVATE Source)

target_compile_definitions(Cavernous PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Cavernous PRIVATE
    CavernousData
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

juce_add_console_app(CavernousDspTest PRODUCT_NAME CavernousDspTest)

target_sources(CavernousDspTest PRIVATE tests/DspTest.cpp)

target_include_directories(CavernousDspTest PRIVATE Source)

target_compile_definitions(CavernousDspTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

target_link_libraries(CavernousDspTest PRIVATE
    juce::juce_audio_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
