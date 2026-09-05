cmake_minimum_required(VERSION 3.22)

project(NFVocalHarmonizer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(NF_BUILD_TESTS "Build DSP and state tests" ON)
option(NF_ENABLE_AAX "Build AAX when the official Avid SDK is configured" OFF)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout")
set(NF_JUCE_PATH "${JUCE_DIR}" CACHE PATH "Alias for JUCE_DIR used by other NF plugins")
set(JUCE_AAX_SDK_PATH "" CACHE PATH "Path to the official Avid AAX SDK")

if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS universal architectures")
endif()

if(NF_JUCE_PATH)
    add_subdirectory("${NF_JUCE_PATH}" JUCE)
elseif(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(NF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NF_FORMATS AU)
endif()

if(NF_ENABLE_AAX)
    if(NOT JUCE_AAX_SDK_PATH)
        message(FATAL_ERROR "NF_ENABLE_AAX=ON requires JUCE_AAX_SDK_PATH=/absolute/path/to/AAX_SDK")
    endif()
    juce_set_aax_sdk_path("${JUCE_AAX_SDK_PATH}")
    list(APPEND NF_FORMATS AAX)
endif()

juce_add_plugin(NFVocalHarmonizer
    COMPANY_NAME "NF Audio Tools"
    BUNDLE_ID "com.nfaudiotools.vocalharmonizer"
    PLUGIN_MANUFACTURER_CODE NfAt
    PLUGIN_CODE NfVh
    FORMATS ${NF_FORMATS}
    PRODUCT_NAME "NF Vocal Harmonizer"
    PLUGIN_NAME "NF Vocal Harmonizer"
    DESCRIPTION "Intelligent real-time vocal harmonizer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Pitch Shift
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_Effect)

juce_generate_juce_header(NFVocalHarmonizer)

juce_add_binary_data(NFVocalHarmonizerAssets SOURCES
    assets/svg/chassis.svg
    assets/svg/logo_nf_audio_tools.svg
    assets/svg/knob_harmony.svg
    assets/svg/knob_formant.svg
    assets/svg/knob_humanize.svg
    assets/svg/knob_width.svg
    assets/svg/knob_mix.svg
    assets/svg/button_analyze.svg
    assets/svg/button_harmonize.svg
    assets/svg/button_power.svg
    assets/svg/interval_voice.svg
    assets/svg/interval_selected.svg
    assets/svg/interval_neutral.svg
    assets/svg/meter_input.svg
    assets/svg/meter_output.svg
    assets/svg/preset_strip.svg
    Manuals/pdf/NF_Vocal_Harmonizer_User_Manual_English.pdf
    Manuals/pdf/NF_Vocal_Harmonizer_Manual_Portugues.pdf)

target_sources(NFVocalHarmonizer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/PresetManager.cpp
    Source/PresetManager.h
    Source/dsp/YinPitchDetector.cpp
    Source/dsp/YinPitchDetector.h
    Source/dsp/MusicalScale.cpp
    Source/dsp/MusicalScale.h
    Source/dsp/GranularPitchShifter.cpp
    Source/dsp/GranularPitchShifter.h
    Source/dsp/HarmonyEngine.cpp
    Source/dsp/HarmonyEngine.h
    Source/dsp/NoteEditModel.cpp
    Source/dsp/NoteEditModel.h
    Source/dsp/NoteCapture.cpp
    Source/dsp/NoteCapture.h
    Source/dsp/HarmonyNoteTypes.h
    Source/ui/NFLookAndFeel.cpp
    Source/ui/NFLookAndFeel.h
    Source/ui/IntervalRail.cpp
    Source/ui/IntervalRail.h
    Source/ui/PitchTrace.cpp
    Source/ui/PitchTrace.h
    Source/ui/HarmonyNoteEditor.cpp
    Source/ui/HarmonyNoteEditor.h
    Source/ui/StereoMeter.cpp
    Source/ui/StereoMeter.h)

target_compile_definitions(NFVocalHarmonizer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(NFVocalHarmonizer PRIVATE
    NFVocalHarmonizerAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

if(NF_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(NFVocalHarmonizerTests PRODUCT_NAME "NF Vocal Harmonizer Tests")
    juce_generate_juce_header(NFVocalHarmonizerTests)
    target_sources(NFVocalHarmonizerTests PRIVATE
        Tests/DspTests.cpp
        Source/dsp/YinPitchDetector.cpp
        Source/dsp/MusicalScale.cpp
        Source/dsp/GranularPitchShifter.cpp
        Source/dsp/HarmonyEngine.cpp
        Source/dsp/NoteEditModel.cpp
        Source/dsp/NoteCapture.cpp
        Source/PresetManager.cpp)
    target_include_directories(NFVocalHarmonizerTests PRIVATE Source)
    target_compile_definitions(NFVocalHarmonizerTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(NFVocalHarmonizerTests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    add_test(NAME NFVocalHarmonizerDSP COMMAND $<TARGET_FILE:NFVocalHarmonizerTests>)

    juce_add_console_app(NFVocalHarmonizerVocalAudit PRODUCT_NAME "NF Vocal Harmonizer Vocal Audit")
    juce_generate_juce_header(NFVocalHarmonizerVocalAudit)
    target_sources(NFVocalHarmonizerVocalAudit PRIVATE
        tools/VocalOfflineAudit.cpp
        Source/dsp/YinPitchDetector.cpp
        Source/dsp/MusicalScale.cpp
        Source/dsp/GranularPitchShifter.cpp
        Source/dsp/HarmonyEngine.cpp
        Source/dsp/NoteEditModel.cpp
        Source/dsp/NoteCapture.cpp)
    target_include_directories(NFVocalHarmonizerVocalAudit PRIVATE Source)
    target_compile_definitions(NFVocalHarmonizerVocalAudit PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(NFVocalHarmonizerVocalAudit PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
