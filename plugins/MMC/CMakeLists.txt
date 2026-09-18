cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(MMC VERSION 1.0.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "/Applications/JUCE")
endif()

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found. Configure with -DJUCE_PATH=/path/to/JUCE")
endif()

add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

juce_add_plugin(MMC
    COMPANY_NAME "jumang4423"
    BUNDLE_ID "com.jumang4423.mmc"
    PLUGIN_MANUFACTURER_CODE Jm42
    PLUGIN_CODE MmCh
    FORMATS VST3
    PRODUCT_NAME "MMC"
    DESCRIPTION "2 x 3 tap stereo chorus"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(MMC)

target_sources(MMC PRIVATE
    Source/ChorusCore.hpp
    Source/PluginProcessor.cpp
    Source/PluginProcessor.hpp)

target_compile_definitions(MMC PUBLIC
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MMC PRIVATE
    juce::juce_audio_utils
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

add_executable(core-smoke-test Tests/CoreSmokeTest.cpp)
target_include_directories(core-smoke-test PRIVATE Source)

if(APPLE)
    add_custom_command(TARGET MMC_VST3 POST_BUILD
        COMMAND /usr/bin/codesign --force --deep --sign -
            "$<TARGET_BUNDLE_DIR:MMC_VST3>"
        COMMENT "Applying final ad-hoc signature after VST3 metadata generation")
endif()
