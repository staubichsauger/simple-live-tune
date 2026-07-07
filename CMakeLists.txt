cmake_minimum_required(VERSION 3.20)

project(SimpleLiveTune VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIMPLELIVETUNE_BUILD_PLUGIN "Build the AU/VST3/Standalone plugin (fetches JUCE)" ON)

add_library(simplelivetune_dsp
    src/dsp/Downsampler.cpp
    src/dsp/PitchDetector.cpp
    src/dsp/PitchShifter.cpp
    src/dsp/RatioSmoother.cpp
    src/dsp/ScaleMapper.cpp
    src/engine/TuneEngine.cpp
)

target_include_directories(simplelivetune_dsp PUBLIC src)
set_target_properties(simplelivetune_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(simplelivetune_dsp PRIVATE /W4)
else()
    target_compile_options(simplelivetune_dsp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(offline_tune tools/offline_tune.cpp)
target_link_libraries(offline_tune PRIVATE simplelivetune_dsp)

enable_testing()

add_executable(test_pitch_detector tests/test_pitch_detector.cpp)
target_link_libraries(test_pitch_detector PRIVATE simplelivetune_dsp)
add_test(NAME pitch_detector COMMAND test_pitch_detector)

add_executable(test_pitch_shifter tests/test_pitch_shifter.cpp)
target_link_libraries(test_pitch_shifter PRIVATE simplelivetune_dsp)
add_test(NAME pitch_shifter COMMAND test_pitch_shifter)

add_executable(test_ratio_smoother tests/test_ratio_smoother.cpp)
target_link_libraries(test_ratio_smoother PRIVATE simplelivetune_dsp)
add_test(NAME ratio_smoother COMMAND test_ratio_smoother)

add_executable(test_engine tests/test_engine.cpp)
target_link_libraries(test_engine PRIVATE simplelivetune_dsp)
add_test(NAME engine COMMAND test_engine)

if(SIMPLELIVETUNE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.14
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(SimpleLiveTune
        COMPANY_NAME "staubichsauger"
        BUNDLE_ID "com.staubichsauger.simplelivetune"
        PLUGIN_MANUFACTURER_CODE Stau
        PLUGIN_CODE Sltn
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "Simple Live Tune"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
    )

    target_sources(SimpleLiveTune PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
    )

    target_compile_definitions(SimpleLiveTune PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    juce_generate_juce_header(SimpleLiveTune)

    target_link_libraries(SimpleLiveTune
        PRIVATE
            simplelivetune_dsp
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )
endif()
