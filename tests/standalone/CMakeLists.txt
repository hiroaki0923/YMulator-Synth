# Standalone Unison Detune Test
cmake_minimum_required(VERSION 3.22)

# Create standalone test executable with same source structure as other tests
add_executable(TestUnisonDetuneStandalone
    TestUnisonDetuneStandalone.cpp
    
    # Include common source files like other tests
    ${CMAKE_SOURCE_DIR}/src/PluginProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/PluginEditor.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/YmfmWrapper.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/UnisonEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/RegisterManager.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/NoteConverter.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/ParameterConverter.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AudioProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/MidiProcessor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ParameterManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/StateManager.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/EnvelopeGenerator.cpp
    ${CMAKE_SOURCE_DIR}/src/core/VoiceManager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/PresetManager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/VOPMParser.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/MainComponent.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OperatorPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/EnvelopeDisplay.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/AlgorithmDisplay.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/RotaryKnob.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PresetUIManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/GlobalControlsPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/UnisonControlsPanel.cpp
    
    # ymfm source files
    ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opm.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opn.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_ssg.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_adpcm.cpp
    ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_misc.cpp
    
    # Mock files
    ${CMAKE_SOURCE_DIR}/tests/mocks/MockBinaryData.cpp
)

# Handle different GTest linking scenarios (same as main tests)
if(TARGET GTest::gtest AND TARGET GTest::gtest_main)
    target_link_libraries(TestUnisonDetuneStandalone
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_core
            juce::juce_graphics
            juce::juce_gui_basics
    )
else()
    # Fallback for older CMake or manual installation
    target_link_libraries(TestUnisonDetuneStandalone
        PRIVATE
            gtest
            gtest_main
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_core
            juce::juce_graphics
            juce::juce_gui_basics
    )
    
    # Add include directories if needed
    if(GTEST_ROOT)
        target_include_directories(TestUnisonDetuneStandalone PRIVATE "${GTEST_ROOT}/include")
    elseif(EXISTS "/opt/homebrew/opt/googletest/include")
        target_include_directories(TestUnisonDetuneStandalone PRIVATE "/opt/homebrew/opt/googletest/include")
        target_link_directories(TestUnisonDetuneStandalone PRIVATE "/opt/homebrew/opt/googletest/lib")
    endif()
endif()

# Include directories (same as other tests)
target_include_directories(TestUnisonDetuneStandalone
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src
        ${CMAKE_BINARY_DIR}/_deps/juce-src/modules
        ${CMAKE_BINARY_DIR}/src/juce_binarydata_YMulator-Synth_Resources/JuceLibraryCode
)

# Compiler settings
target_compile_features(TestUnisonDetuneStandalone PRIVATE cxx_std_17)

# Define test-specific compile definitions (same as other tests)
target_compile_definitions(TestUnisonDetuneStandalone
    PRIVATE
        JUCE_UNIT_TESTS=1
        JUCE_STANDALONE_APPLICATION=0
        JUCE_MAC=1
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JucePlugin_Name="YMulator Synth"
        JucePlugin_Desc="YMulator Synth"
        JucePlugin_Manufacturer="Hiroaki"
        JucePlugin_ManufacturerCode=0x56656e64
        JucePlugin_PluginCode=0x43687053
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JucePlugin_EditorRequiresKeyboardFocus=0
        JucePlugin_Version=1.0.0
        JucePlugin_VersionCode=0x10000
        JucePlugin_VersionString="1.0.0"
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        USING_MOCK_BINARY_DATA=1
)

# Add to test suite
add_test(NAME TestUnisonDetuneStandalone COMMAND TestUnisonDetuneStandalone)

# Set test properties
set_tests_properties(TestUnisonDetuneStandalone PROPERTIES
    TIMEOUT 300  # 5 minutes timeout
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)