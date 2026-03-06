cmake_minimum_required(VERSION 3.24)

project(carbide VERSION 0.1.0 LANGUAGES C CXX)

if(NOT APPLE)
    message(FATAL_ERROR "carbide currently targets macOS only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "" FORCE)

option(CARBIDE_BUILD_TESTS "Build carbide DSP self-tests." ON)

if(NOT DEFINED JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR to your JUCE checkout. Example: cmake -S . -B build -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(carbide
    COMPANY_NAME "carbide"
    BUNDLE_ID "com.carbide.audio.carbide"
    PLUGIN_MANUFACTURER_CODE CBDE
    PLUGIN_CODE CKBD
    FORMATS AU Standalone
    PRODUCT_NAME "carbide"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(carbide
    PRIVATE
        Source/DSP/KickSynthVoice.cpp
        Source/Presets/FactoryPresets.cpp
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
)

target_include_directories(carbide
    PRIVATE
        Source
)

target_compile_definitions(carbide
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(carbide
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(CARBIDE_BUILD_TESTS)
    juce_add_console_app(carbide_dsp_tests
        PRODUCT_NAME "carbide_dsp_tests"
    )

    target_sources(carbide_dsp_tests
        PRIVATE
            Source/DSP/KickSynthVoice.cpp
            Source/Presets/FactoryPresets.cpp
            Source/Tests/DspSelfTests.cpp
    )

    target_include_directories(carbide_dsp_tests
        PRIVATE
            Source
    )

    target_link_libraries(carbide_dsp_tests
        PRIVATE
            juce::juce_dsp
            juce::juce_core
            juce::juce_events
            juce::juce_audio_basics
    )
endif()
