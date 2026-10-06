cmake_minimum_required(VERSION 3.22)

project(TimeTwista VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: build the C++ runtime into the plugin, so it doesn't depend on the
# Visual C++ Redistributable being installed on the PC that loads it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Downloads JUCE 8.0.15 automatically on the first configure (about 100 MB).
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TimeTwista
    COMPANY_NAME             "DIY Audio"         # same vendor as Echo Space, so they group together in Ableton
    PRODUCT_NAME             "TimeTwista mk1"
    PLUGIN_MANUFACTURER_CODE Diya
    PLUGIN_CODE              Ttw1
    BUNDLE_ID                com.diyaudio.timetwista
    FORMATS                  VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx Delay Reverb
    COPY_PLUGIN_AFTER_BUILD  FALSE)

# Fonts (all SIL Open Font License, see Resources/OFL-*.txt)
juce_add_binary_data(TimeTwistaFonts SOURCES
    Resources/Shrikhand-Regular.ttf
    Resources/SpaceGrotesk-Medium.ttf
    Resources/SpaceGrotesk-Bold.ttf
    Resources/JetBrainsMono-SemiBold.ttf)

target_sources(TimeTwista PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(TimeTwista PRIVATE Source)

target_compile_definitions(TimeTwista PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(TimeTwista
    PRIVATE
        TimeTwistaFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
