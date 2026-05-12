cmake_minimum_required(VERSION 3.15)
project(ThePress VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------
# JUCE setup
# Clone JUCE alongside this project:
#   git submodule add https://github.com/juce-framework/JUCE.git JUCE
# or set JUCE_PATH to your local JUCE installation:
#   cmake -DJUCE_PATH=/path/to/JUCE ..
# -----------------------------------------------------------------------
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    message(FATAL_ERROR
        "JUCE not found. Either:\n"
        "  1) Add JUCE as a submodule: git submodule add https://github.com/juce-framework/JUCE.git JUCE\n"
        "  2) Pass -DJUCE_PATH=/path/to/JUCE to cmake")
endif()

# Mac builds include AU (Logic Pro, GarageBand) + VST3
# Windows/Linux build VST3 only
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(ThePress
    VERSION                     1.0.0
    COMPANY_NAME                "Zeus"
    PLUGIN_MANUFACTURER_CODE    Zeus
    PLUGIN_CODE                 TrPs
    FORMATS                     ${PLUGIN_FORMATS}
    PRODUCT_NAME                "The Press"
    PLUGIN_NAME                 "The Press"
    DESCRIPTION                 "Apex Limiter + Even Drive + Velvet Clip"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             "Fx" "Dynamics"
    AU_MAIN_TYPE                kAudioUnitType_Effect
    AU_EXPORT_PREFIX            ThePressAU
    HARDENED_RUNTIME_ENABLED    TRUE
    HARDENED_RUNTIME_OPTIONS    "com.apple.security.device.audio-input"
)

target_sources(ThePress
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

juce_generate_juce_header(ThePress)

target_compile_definitions(ThePress
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ThePress
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
