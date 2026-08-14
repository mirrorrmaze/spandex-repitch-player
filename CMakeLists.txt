cmake_minimum_required(VERSION 3.22)

project(SPANDEX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MP3 export uses libmp3lame, built from source via vcpkg (see tools/vcpkg).
# Configure with:
#   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
#     -DCMAKE_TOOLCHAIN_FILE=tools/vcpkg/scripts/buildsystems/vcpkg.cmake ^
#     -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
find_package(mp3lame CONFIG REQUIRED)

add_subdirectory(libs/JUCE)

add_library(rubberband STATIC
    libs/rubberband/single/RubberBandSingle.cpp
)
target_include_directories(rubberband PUBLIC
    libs/rubberband
)
target_compile_features(rubberband PUBLIC cxx_std_17)
target_compile_definitions(rubberband PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _USE_MATH_DEFINES
)

juce_add_plugin(SPANDEX
    PRODUCT_NAME "SPANDEX"
    COMPANY_NAME "SPANDEX"
    VERSION ${PROJECT_VERSION}
    FORMATS Standalone VST3
    PLUGIN_MANUFACTURER_CODE Spdx
    PLUGIN_CODE Spx1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument
    COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(SPANDEX)

target_sources(SPANDEX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MainComponent.cpp
    Source/SelfTest.cpp
    Source/Audio/AudioFileLoader.cpp
    Source/Audio/StretchAudioSource.cpp
    Source/Audio/AudioEngine.cpp
    Source/Audio/Paulstretch.cpp
    Source/Audio/EffectsChain.cpp
    Source/Audio/SmudgeProcessor.cpp
    Source/Audio/LossyProcessor.cpp
    Source/Audio/SpectralClipperProcessor.cpp
    Source/Audio/GranularDelay.cpp
    Source/Audio/FreqShifter.cpp
    Source/UI/WaveformComponent.cpp
    Source/UI/TransportControls.cpp
    Source/UI/LoopControls.cpp
    Source/UI/PitchFader.cpp
    Source/UI/SpeedControl.cpp
    Source/UI/WarpModeSelector.cpp
    Source/UI/ExportPanel.cpp
    Source/UI/FxPanel.cpp
    Source/UI/FxRoutingBar.cpp
    Source/UI/EqCurveEditor.cpp
    Source/UI/EqPanel.cpp
    Source/UI/HeaderTabs.cpp
    Source/Export/ExportEngine.cpp
)

target_compile_definitions(SPANDEX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    # No VST2 build (FORMATS is Standalone/VST3 only, no VST2 has ever been
    # released) - this disables a JUCE build-time check that only matters
    # for VST2<->VST3 parameter-automation migration.
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(SPANDEX
    PRIVATE
        rubberband
        mp3lame::mp3lame
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
