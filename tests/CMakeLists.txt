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
    # Define common source files used by all test executables
    set(COMMON_SOURCES
        mocks/MockAudioProcessorHost.cpp
        
        # Main source files needed for testing
        ${CMAKE_SOURCE_DIR}/src/PluginProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/PluginEditor.cpp
        ${CMAKE_SOURCE_DIR}/src/dsp/YmfmWrapper.cpp
        ${CMAKE_SOURCE_DIR}/src/core/MidiProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/core/PanProcessor.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ParameterManager.cpp
        ${CMAKE_SOURCE_DIR}/src/core/StateManager.cpp
        ${CMAKE_SOURCE_DIR}/src/core/MacroMapper.cpp
        ${CMAKE_SOURCE_DIR}/src/core/PatchGenerator.cpp
        ${CMAKE_SOURCE_DIR}/src/core/SnapshotStore.cpp
        ${CMAKE_SOURCE_DIR}/src/core/PatchWorkspace.cpp
        ${CMAKE_SOURCE_DIR}/src/core/MotionEngine.cpp
        ${CMAKE_SOURCE_DIR}/src/core/PatchPreview.cpp
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
        ${CMAKE_SOURCE_DIR}/src/ui/ToneStrip.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/QuickView.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/GeneratorPanel.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/OutputScope.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/MotionPanel.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/MotionStrip.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/LfoNoiseStrip.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/YmLookAndFeel.cpp
        
        # ymfm source files
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opm.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_opn.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_ssg.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_adpcm.cpp
        ${CMAKE_SOURCE_DIR}/third_party/ymfm/src/ymfm_misc.cpp
    )
    
    # ============ Test Group 1: Basic Tests ============
    add_executable(YMulatorSynthAU_BasicTests
        test_main.cpp
        unit/PluginBasicTest.cpp
        unit/SimpleParameterTest.cpp
        unit/ParameterDebugTest.cpp
        unit/MultiInstanceTest.cpp
        unit/MidiCcMappingTest.cpp
        unit/AlgorithmInfoTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 2: Preset Tests ============
    add_executable(YMulatorSynthAU_PresetTests
        test_main.cpp
        unit/PresetManagerTest.cpp
        unit/VOPMParserTest.cpp
        unit/StateManagerTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 3: Parameter Tests ============
    add_executable(YMulatorSynthAU_ParameterTests
        test_main.cpp
        unit/ParameterManagerTest.cpp
        unit/ParameterStateIntegrationTest.cpp
        unit/MacroMapperTest.cpp
        unit/PatchGeneratorTest.cpp
        unit/PatchWorkspaceTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 4: Pan Tests ============
    add_executable(YMulatorSynthAU_PanTests
        test_main.cpp
        unit/GlobalPanTest.cpp
        RandomPanDebugTest.cpp
        RandomPanOutputTest.cpp
        PanModeRegressionTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 5: Integration Tests ============
    add_executable(YMulatorSynthAU_IntegrationTests
        test_main.cpp
        unit/PluginProcessorComprehensiveTest.cpp
        unit/VoiceManagerTest.cpp
        unit/YmfmWrapperTest.cpp
        integration/ComprehensiveIntegrationTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 6: UI Tests ============
    add_executable(YMulatorSynthAU_UITests
        test_main.cpp
        ui/MainComponentTest.cpp
        ui/EnvelopeDisplayTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 7: Quality Tests ============
    add_executable(YMulatorSynthAU_QualityTests
        test_main.cpp
        AudioQualityTest.cpp
        unit/PitchAccuracyTest.cpp
        unit/OperatorSlotOrderTest.cpp
        unit/RegisterGoldenTest.cpp
        unit/RegisterUpdateTest.cpp
        unit/SlotEnableTest.cpp
        unit/LfoWiringTest.cpp
        unit/ParameterReachTest.cpp
        unit/VelocityTest.cpp
        unit/PatchPreviewTest.cpp
        unit/MotionEngineTest.cpp
        unit/WideTest.cpp
        unit/PanMotionTest.cpp
        unit/EchoTest.cpp
        unit/MonoArpTest.cpp
        ${COMMON_SOURCES}
    )
    
    # ============ Test Group 8: Performance Tests ============
    add_executable(YMulatorSynthAU_PerformanceTests
        test_main.cpp
        performance/PerformanceRegressionTest.cpp
        ${COMMON_SOURCES}
    )
    
    
    # List of all test executables
    set(TEST_EXECUTABLES
        YMulatorSynthAU_BasicTests
        YMulatorSynthAU_PresetTests
        YMulatorSynthAU_ParameterTests
        YMulatorSynthAU_PanTests
        YMulatorSynthAU_IntegrationTests
        YMulatorSynthAU_UITests
        YMulatorSynthAU_QualityTests
        YMulatorSynthAU_PerformanceTests
    )
    
    # Unified Test Executable (for backward compatibility)
    set(UNIFIED_TEST_SOURCES
        test_main.cpp
        unit/PluginBasicTest.cpp
        unit/SimpleParameterTest.cpp
        unit/ParameterDebugTest.cpp
        unit/MultiInstanceTest.cpp
        unit/MidiCcMappingTest.cpp
        unit/AlgorithmInfoTest.cpp
        unit/PluginProcessorComprehensiveTest.cpp
        unit/VoiceManagerTest.cpp
        unit/YmfmWrapperTest.cpp
        unit/PresetManagerTest.cpp
        unit/VOPMParserTest.cpp
        unit/GlobalPanTest.cpp
        unit/ParameterManagerTest.cpp
        unit/StateManagerTest.cpp
        unit/ParameterStateIntegrationTest.cpp
        RandomPanDebugTest.cpp
        RandomPanOutputTest.cpp
        PanModeRegressionTest.cpp
        AudioQualityTest.cpp
        unit/PitchAccuracyTest.cpp
        unit/OperatorSlotOrderTest.cpp
        unit/RegisterGoldenTest.cpp
        unit/RegisterUpdateTest.cpp
        unit/SlotEnableTest.cpp
        unit/LfoWiringTest.cpp
        unit/ParameterReachTest.cpp
        unit/VelocityTest.cpp
        unit/PatchPreviewTest.cpp
        unit/MotionEngineTest.cpp
        unit/WideTest.cpp
        unit/PanMotionTest.cpp
        unit/EchoTest.cpp
        unit/MonoArpTest.cpp
        unit/MacroMapperTest.cpp
        unit/PatchGeneratorTest.cpp
        unit/PatchWorkspaceTest.cpp
        integration/ComprehensiveIntegrationTest.cpp
        performance/PerformanceRegressionTest.cpp
        # unit/MidiProcessorTest.cpp  # Temporarily disabled during refactoring
        ${COMMON_SOURCES}
    )
    
    add_executable(YMulatorSynthAU_Tests ${UNIFIED_TEST_SOURCES})
    list(APPEND TEST_EXECUTABLES YMulatorSynthAU_Tests)
    
    # ============ Developer tool: off-screen UI snapshot (not a test) ============
    # Shares the test build configuration so it renders the same code the tests run.
    add_executable(YMulatorSynthAU_UISnapshot
        ${CMAKE_SOURCE_DIR}/tools/ui_snapshot.cpp
        ${COMMON_SOURCES}
    )
    target_compile_definitions(YMulatorSynthAU_UISnapshot PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(YMulatorSynthAU_UISnapshot
        PRIVATE
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_audio_basics
            juce::juce_core
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
    )
    add_executable(YMulatorSynthAU_SongRender
        ${CMAKE_SOURCE_DIR}/tools/song_render.cpp
        ${COMMON_SOURCES}
    )
    target_compile_definitions(YMulatorSynthAU_SongRender PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(YMulatorSynthAU_SongRender
        PRIVATE
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_audio_basics
            juce::juce_core
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
            juce::juce_audio_formats
    )
    set(HOST_TARGETS ${TEST_EXECUTABLES} YMulatorSynthAU_UISnapshot YMulatorSynthAU_SongRender)
    
    # Configure each test executable
    # Embedded resources (presets, algorithm diagrams) come from the same
    # binary-data target the plugin links, so every generated file is included
    foreach(TEST_EXECUTABLE ${HOST_TARGETS})
        target_link_libraries(${TEST_EXECUTABLE} PRIVATE YMulator-Synth_Resources)
    endforeach()
    
    # Configure include directories for all test executables
    foreach(TEST_EXECUTABLE ${HOST_TARGETS})
        target_include_directories(${TEST_EXECUTABLE}
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/third_party/ymfm/src
                ${CMAKE_BINARY_DIR}/_deps/juce-src/modules
                ${CMAKE_BINARY_DIR}/src/juce_binarydata_YMulator-Synth_Resources/JuceLibraryCode
        )
    endforeach()
    
    # Configure linking for all test executables
    foreach(TEST_EXECUTABLE ${TEST_EXECUTABLES})
        # Handle different GTest linking scenarios
        if(TARGET GTest::gtest AND TARGET GTest::gtest_main)
            target_link_libraries(${TEST_EXECUTABLE}
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
            target_link_libraries(${TEST_EXECUTABLE}
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
                target_include_directories(${TEST_EXECUTABLE} PRIVATE "${GTEST_ROOT}/include")
            elseif(EXISTS "/opt/homebrew/opt/googletest/include")
                target_include_directories(${TEST_EXECUTABLE} PRIVATE "/opt/homebrew/opt/googletest/include")
                target_link_directories(${TEST_EXECUTABLE} PRIVATE "/opt/homebrew/opt/googletest/lib")
            endif()
        endif()
    endforeach()
    
    # Define test-specific compile definitions for all test executables
    foreach(TEST_EXECUTABLE ${HOST_TARGETS})
        target_compile_definitions(${TEST_EXECUTABLE}
            PRIVATE
                JUCE_UNIT_TESTS=1
                JUCE_STANDALONE_APPLICATION=0
                $<$<PLATFORM_ID:Darwin>:JUCE_MAC=1>
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
    endforeach()
    
    # Register tests
    include(GoogleTest)
    foreach(TEST_EXECUTABLE ${TEST_EXECUTABLES})
        gtest_discover_tests(${TEST_EXECUTABLE})
    endforeach()
    
    message(STATUS "Configured split test binaries for improved CI performance:")
    message(STATUS "  - YMulatorSynthAU_BasicTests (basic functionality)")
    message(STATUS "  - YMulatorSynthAU_PresetTests (preset management)")
    message(STATUS "  - YMulatorSynthAU_ParameterTests (parameter system)")
    message(STATUS "  - YMulatorSynthAU_PanTests (pan functionality)")
    message(STATUS "  - YMulatorSynthAU_IntegrationTests (component integration)")
    message(STATUS "  - YMulatorSynthAU_QualityTests (audio quality)")
    message(STATUS "  - YMulatorSynthAU_Tests (unified, for backward compatibility)")
else()
    message(STATUS "Google Test not found. Tests will not be built.")
endif()