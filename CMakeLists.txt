cmake_minimum_required(VERSION 3.22)
project(LunaFX VERSION 1.2.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE -----------------------------------------------------------------
# Option A: point to a local JUCE checkout:  cmake -B build -DJUCE_PATH=/path/to/JUCE
# Option B: nothing -> JUCE is downloaded automatically (needs internet on first configure)
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# don't copy into system folders on CI (the installer does that)
if(DEFINED ENV{CI})
    set(LUNAFX_COPY FALSE)
else()
    set(LUNAFX_COPY TRUE)
endif()

juce_add_plugin(LunaFX
    PRODUCT_NAME "Luna FX"
    COMPANY_NAME "LunaLabs"
    PLUGIN_MANUFACTURER_CODE Lnlb
    PLUGIN_CODE Lnx1
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD ${LUNAFX_COPY})

target_sources(LunaFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(LunaFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(LunaFX
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
        # juce_recommended_warning_flags omitted: JUCE8 adds /WX which rejects Font() deprecation


# --- optional: DSP unit tests (no JUCE needed) ------------------------------
add_executable(accentfx_dsp_test EXCLUDE_FROM_ALL tests/test_engine.cpp)
add_executable(accentfx_dsp_smoke EXCLUDE_FROM_ALL tests/test_engine_smoke.cpp)
add_executable(accentfx_xy_test EXCLUDE_FROM_ALL tests/test_xy_weights.cpp)
