# Test configuration
# Try to find Google Test
find_package(GTest QUIET)

# If not found, try with pkg-config
if(NOT GTest_FOUND)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTest QUIET gtest)
    endif()
endif()

# If still not found, try homebrew paths on macOS
if(NOT GTest_FOUND AND APPLE)
    set(HOMEBREW_PREFIX "/opt/homebrew" "/usr/local")
    foreach(prefix ${HOMEBREW_PREFIX})
        if(EXISTS "${prefix}/opt/googletest")
            set(GTEST_ROOT "${prefix}/opt/googletest")
            find_package(GTest PATHS ${GTEST_ROOT} NO_DEFAULT_PATH QUIET)
            if(GTest_FOUND)
                break()
            endif()
        endif()
    endforeach()
endif()

if(GTest_FOUND)
    # Check if JUCE binary data has been generated
    set(JUCE_BINARY_DATA_FILE "${CMAKE_BINARY_DIR}/src/juce_binarydata_YMulator-Synth_Resources/JuceLibraryCode/BinaryData1.cpp")
    
    # Define test sources
    set(TEST_SOURCES
        test_main.cpp
        mocks/MockAudioProcessorHost.cpp
        unit/PluginBasicTest.cpp
        unit/SimpleParameterTest.cpp
        unit/ParameterDebugTest.cpp
        unit/PluginProcessorComprehensiveTest.cpp
        unit/VoiceManagerTest.cpp
        unit/YmfmWrapperTest.cpp
        unit/PresetManagerTest.cpp
        unit/VOPMParserTest.cpp
        unit/GlobalPanTest.cpp
        RandomPanDebugTest.cpp
        RandomPanOutputTest.cpp
        PanModeRegressionTest.cpp
        AudioQualityTest.cpp
        # unit/MidiProcessorTest.cpp  # Temporarily disabled during refactoring
        
        # Main source files needed for testing
        ${CMAKE_SOURCE_DIR}/src/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/PluginEditor.cpp
        ${CMAKE_SOURCE_DIR}/src/dsp/YmfmWrapper.cpp
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
        
        # ymfm source files
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opm.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opn.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_ssg.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_adpcm.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_misc.cpp
        
        # Additional test files will be added here
    )
    
    # Test executable
    add_executable(YMulatorSynthAU_Tests ${TEST_SOURCES})
    
    # Add JUCE binary data if it exists, otherwise use mock
    if(EXISTS "${JUCE_BINARY_DATA_FILE}")
        target_sources(YMulatorSynthAU_Tests PRIVATE "${JUCE_BINARY_DATA_FILE}")
        message(STATUS "Added JUCE binary data file: ${JUCE_BINARY_DATA_FILE}")
    else()
        message(STATUS "JUCE binary data not available - using MockBinaryData")
        target_sources(YMulatorSynthAU_Tests PRIVATE "mocks/MockBinaryData.cpp")
        target_compile_definitions(YMulatorSynthAU_Tests PRIVATE USING_MOCK_BINARY_DATA=1)
    endif()
    
    target_include_directories(YMulatorSynthAU_Tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/third_party/ymfm/src
            ${CMAKE_BINARY_DIR}/_deps/juce-src/modules
            ${CMAKE_BINARY_DIR}/src/juce_binarydata_YMulator-Synth_Resources/JuceLibraryCode
    )
    
    # Handle different GTest linking scenarios
    if(TARGET GTest::gtest AND TARGET GTest::gtest_main)
        target_link_libraries(YMulatorSynthAU_Tests
            PRIVATE
                GTest::gtest
                GTest::gtest_main
                juce::juce_audio_processors
                juce::juce_audio_utils
                juce::juce_audio_basics
                juce::juce_core
                juce::juce_graphics
                juce::juce_gui_basics
        )
    else()
        # Fallback for older CMake or manual installation
        target_link_libraries(YMulatorSynthAU_Tests
            PRIVATE
                gtest
                gtest_main
                juce::juce_audio_processors
                juce::juce_audio_utils
                juce::juce_audio_basics
                juce::juce_core
                juce::juce_graphics
                juce::juce_gui_basics
        )
        
        # Add include directories if needed
        if(GTEST_ROOT)
            target_include_directories(YMulatorSynthAU_Tests PRIVATE "${GTEST_ROOT}/include")
        elseif(EXISTS "/opt/homebrew/opt/googletest/include")
            target_include_directories(YMulatorSynthAU_Tests PRIVATE "/opt/homebrew/opt/googletest/include")
            target_link_directories(YMulatorSynthAU_Tests PRIVATE "/opt/homebrew/opt/googletest/lib")
        endif()
    endif()
    
    # Define test-specific compile definitions
    target_compile_definitions(YMulatorSynthAU_Tests
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
    )
    
    # Register tests
    include(GoogleTest)
    gtest_discover_tests(YMulatorSynthAU_Tests)
else()
    message(STATUS "Google Test not found. Tests will not be built.")
endif()