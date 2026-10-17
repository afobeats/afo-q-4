cmake_minimum_required(VERSION 3.22)
project(AfoQ4 VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(AFOQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AFOQ_FORMATS AU)
endif()
# AAX needs Avid's SDK + a developer licence:  cmake -B build -DAAX_SDK_PATH=/path/to/aax-sdk
if(AAX_SDK_PATH)
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    list(APPEND AFOQ_FORMATS AAX)
endif()

juce_add_plugin(AfoQ4
    COMPANY_NAME "Afobeats Audio"
    PRODUCT_NAME "Afo-Q 4"
    PLUGIN_MANUFACTURER_CODE Afob
    PLUGIN_CODE AfQ4
    FORMATS ${AFOQ_FORMATS}
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AfoQ4 PRIVATE
    Source/Params.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/UI/EqDisplay.cpp)

target_compile_definitions(AfoQ4 PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AfoQ4
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# DSP unit test (no JUCE needed):  cmake --build build --target filter_test && ./build/filter_test
add_executable(filter_test Tests/filter_test.cpp)
target_include_directories(filter_test PRIVATE Source)
