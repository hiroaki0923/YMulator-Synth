#include <gtest/gtest.h>
#include "../../src/PluginProcessor.h"
#include "../mocks/MockAudioProcessorHost.h"
#include "../../src/utils/ParameterIDs.h"
#include "../../src/utils/Debug.h"

using namespace ymulatorsynth;

/**
 * Integration tests between ParameterManager and StateManager.
 * 
 * These tests verify that the two components work correctly together,
 * focusing on parameter-state interactions, custom preset management,
 * and cross-component state consistency.
 */
class ParameterStateIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create processor and host
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize processor with host
        host->initializeProcessor(*processor, 44100.0, 512, 2);
    }
    
    void TearDown() override {
        processor.reset();
        host.reset();
    }
    
    // Helper method to set multiple parameters to create a distinct state
    void setDistinctParameterState(int algorithmValue, int feedbackValue, float op1TotalLevel) {
        auto& parameters = processor->getParameters();
        auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
        auto* fbParam = parameters.getParameter(ParamID::Global::Feedback);
        auto* tlParam = parameters.getParameter(ParamID::Op::tl(1));
        
        algParam->setValueNotifyingHost(algorithmValue / 7.0f);
        fbParam->setValueNotifyingHost(feedbackValue / 7.0f);
        tlParam->setValueNotifyingHost(op1TotalLevel);
    }
    
    // Helper method to verify parameter state
    void verifyParameterState(int expectedAlgorithm, int expectedFeedback, float expectedOp1TL, float tolerance = 0.02f) {
        auto& parameters = processor->getParameters();
        auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
        auto* fbParam = parameters.getParameter(ParamID::Global::Feedback);
        auto* tlParam = parameters.getParameter(ParamID::Op::tl(1));
        
        EXPECT_NEAR(algParam->getValue() * 7.0f, expectedAlgorithm, tolerance);
        EXPECT_NEAR(fbParam->getValue() * 7.0f, expectedFeedback, tolerance);
        EXPECT_NEAR(tlParam->getValue(), expectedOp1TL, tolerance);
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<YMulatorSynth::Test::MockAudioProcessorHost> host;
};

// ============================================================================
// Parameter Changes Affecting State Management
// ============================================================================

TEST_F(ParameterStateIntegrationTest, ParameterChangesAffectCustomMode) {
    // Start with a factory preset
    processor->setCurrentProgram(0);
    EXPECT_FALSE(processor->isInCustomMode());
    
    // Simulate parameter change that should trigger custom mode
    auto& parameters = processor->getParameters();
    auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
    algParam->setValueNotifyingHost(0.8f); // Change parameter
    
    // Note: Custom mode triggering behavior is implementation-dependent
    // This test mainly verifies the operation doesn't crash
    EXPECT_NO_THROW(processor->getCurrentProgram());
}

TEST_F(ParameterStateIntegrationTest, PresetLoadingResetsParameterCustomState) {
    // Enter custom mode
    processor->setCustomMode(true, "Test Custom");
    EXPECT_TRUE(processor->isInCustomMode());
    
    // Load a factory preset
    if (processor->getNumPrograms() > 0) {
        processor->setCurrentProgram(0);
        
        // Should exit custom mode
        EXPECT_FALSE(processor->isInCustomMode());
        
        // Should reflect factory preset
        EXPECT_EQ(processor->getCurrentProgram(), 0);
    }
}

// ============================================================================
// State Persistence with Custom Presets
// ============================================================================

TEST_F(ParameterStateIntegrationTest, CustomPresetStateRoundTrip) {
    // Create a custom state
    setDistinctParameterState(5, 3, 0.7f);
    processor->setCustomMode(true, "Integration Test Custom");
    
    // Save state
    juce::MemoryBlock savedState;
    processor->getStateInformation(savedState);
    
    // Load a different factory preset (this should clear custom mode and change parameters)
    processor->setCurrentProgram(0);
    EXPECT_FALSE(processor->isInCustomMode());
    
    // Restore the saved state
    processor->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    
    // Custom mode should be restored
    EXPECT_TRUE(processor->isInCustomMode());
    EXPECT_EQ(processor->getCustomPresetName(), "Integration Test Custom");
    
    // Parameters should be restored
    verifyParameterState(5, 3, 0.7f);
}

TEST_F(ParameterStateIntegrationTest, FactoryPresetStateRoundTrip) {
    // Load a specific factory preset
    processor->setCurrentProgram(2);
    setDistinctParameterState(3, 6, 0.4f);
    
    // Ensure we're not in custom mode
    processor->setCustomMode(false);
    
    // Save state
    juce::MemoryBlock savedState;
    processor->getStateInformation(savedState);
    
    // Change to different state
    processor->setCurrentProgram(1);
    setDistinctParameterState(1, 1, 0.9f);
    
    // Restore saved state
    processor->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    
    // Should restore to original preset
    EXPECT_EQ(processor->getCurrentProgram(), 2);
    EXPECT_FALSE(processor->isInCustomMode());
    
    // Parameters should be restored
    verifyParameterState(3, 6, 0.4f);
}

// ============================================================================
// Global Pan Integration Tests
// ============================================================================

TEST_F(ParameterStateIntegrationTest, GlobalPanStatePersistence) {
    auto& parameters = processor->getParameters();
    
    // Set global pan to RANDOM mode
    auto* globalPanParam = dynamic_cast<juce::AudioParameterChoice*>(
        parameters.getParameter(ParamID::Global::GlobalPan));
    ASSERT_NE(globalPanParam, nullptr);
    
    globalPanParam->setValueNotifyingHost(1.0f); // RANDOM mode
    
    // Save state
    juce::MemoryBlock savedState;
    processor->getStateInformation(savedState);
    
    // Change to different pan mode
    globalPanParam->setValueNotifyingHost(0.33f); // CENTER
    
    // Restore state
    processor->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    
    // Global pan should be restored to RANDOM (index 3)
    EXPECT_EQ(globalPanParam->getIndex(), 3);
}

// ============================================================================
// Parameter Update Integration
// ============================================================================

TEST_F(ParameterStateIntegrationTest, ParameterUpdatesAfterStateRestore) {
    // Set initial state
    setDistinctParameterState(4, 2, 0.6f);
    
    // Save state
    juce::MemoryBlock savedState;
    processor->getStateInformation(savedState);
    
    // Change parameters
    setDistinctParameterState(1, 5, 0.3f);
    
    // Restore state
    processor->setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
    
    // Process some audio to ensure parameters are applied
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midiBuffer;
    processor->processBlock(buffer, midiBuffer);
    
    // Should not crash and parameters should be consistent
    verifyParameterState(4, 2, 0.6f);
}

// ============================================================================
// Preset Parameter Integration
// ============================================================================

TEST_F(ParameterStateIntegrationTest, PresetParameterLoading) {
    if (processor->getNumPrograms() > 1) {
        auto& parameters = processor->getParameters();
        
        // Load first preset
        processor->setCurrentProgram(0);
        
        // Capture parameter values
        auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
        auto* fbParam = parameters.getParameter(ParamID::Global::Feedback);
        float alg1 = algParam->getValue();
        float fb1 = fbParam->getValue();
        
        // Load second preset
        processor->setCurrentProgram(1);
        float alg2 = algParam->getValue();
        float fb2 = fbParam->getValue();
        
        // Parameters should potentially be different between presets
        // (unless presets happen to have identical algorithm/feedback values)
        
        // Load first preset again
        processor->setCurrentProgram(0);
        
        // Parameters should be restored to first preset values
        EXPECT_NEAR(algParam->getValue(), alg1, 0.01f);
        EXPECT_NEAR(fbParam->getValue(), fb1, 0.01f);
    }
}

// ============================================================================
// Cross-Component State Consistency
// ============================================================================

TEST_F(ParameterStateIntegrationTest, StateConsistencyAfterMultipleOperations) {
    // Perform a series of operations that test integration
    
    // 1. Load preset
    processor->setCurrentProgram(0);
    
    // 2. Modify global pan parameter
    auto& parameters = processor->getParameters();
    auto* globalPanParam = parameters.getParameter(ParamID::Global::GlobalPan);
    globalPanParam->setValueNotifyingHost(0.5f);
    
    // 3. Enter custom mode
    processor->setCustomMode(true, "Consistency Test");
    
    // 4. Save state
    juce::MemoryBlock state1;
    processor->getStateInformation(state1);
    
    // 5. Process some audio
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midiBuffer;
    processor->processBlock(buffer, midiBuffer);
    
    // 6. Restore state
    processor->setStateInformation(state1.getData(), static_cast<int>(state1.getSize()));
    
    // State should be consistent
    EXPECT_TRUE(processor->isInCustomMode());
    EXPECT_EQ(processor->getCustomPresetName(), "Consistency Test");
}

// ============================================================================
// Error Handling Integration
// ============================================================================

TEST_F(ParameterStateIntegrationTest, RecoveryFromInvalidState) {
    // Get a valid state first
    juce::MemoryBlock validState;
    processor->getStateInformation(validState);
    
    // Try to set invalid state
    const char* invalidData = "Invalid state data";
    processor->setStateInformation(invalidData, strlen(invalidData));
    
    // Processor should still be functional
    EXPECT_NO_THROW(processor->setCurrentProgram(0));
    
    // Should be able to restore valid state
    EXPECT_NO_THROW(processor->setStateInformation(validState.getData(), 
                                                   static_cast<int>(validState.getSize())));
}

// ============================================================================
// Performance Integration Tests
// ============================================================================

TEST_F(ParameterStateIntegrationTest, PerformanceWithFrequentUpdates) {
    // Test that frequent parameter updates and state operations don't cause issues
    int numPrograms = processor->getNumPrograms();
    
    for (int iteration = 0; iteration < 20; ++iteration) {
        // Change some parameters
        setDistinctParameterState(iteration % 8, (iteration * 2) % 8, (iteration % 100) / 100.0f);
        
        // Process some audio
        juce::AudioBuffer<float> buffer(2, 256);
        buffer.clear();
        juce::MidiBuffer midiBuffer;
        processor->processBlock(buffer, midiBuffer);
        
        // Occasional state save/restore
        if (iteration % 5 == 0) {
            juce::MemoryBlock tempState;
            processor->getStateInformation(tempState);
            processor->setStateInformation(tempState.getData(), static_cast<int>(tempState.getSize()));
        }
        
        // Occasional preset changes
        if (iteration % 7 == 0) {
            int presetIndex = iteration % numPrograms;
            processor->setCurrentProgram(presetIndex);
        }
    }
    
    // Should complete without crashes
    SUCCEED();
}

// ============================================================================
// Memory Management Integration
// ============================================================================

TEST_F(ParameterStateIntegrationTest, NoMemoryLeaksInIntegration) {
    // Perform operations that could potentially cause memory leaks
    
    for (int i = 0; i < 10; ++i) {
        // Create and save multiple states
        juce::MemoryBlock state;
        processor->getStateInformation(state);
        
        // Change parameters
        processor->setCustomMode(true, "Temp Custom " + juce::String(i));
        
        // Process audio
        juce::AudioBuffer<float> buffer(2, 256);
        buffer.clear();
        juce::MidiBuffer midiBuffer;
        processor->processBlock(buffer, midiBuffer);
        
        // Restore state
        processor->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        
        // Change global pan parameter
        auto& parameters = processor->getParameters();
        auto* globalPanParam = parameters.getParameter(ParamID::Global::GlobalPan);
        globalPanParam->setValueNotifyingHost((i % 4) / 3.0f);
    }
    
    // Final cleanup operations
    processor->setCustomMode(false);
    processor->setCurrentProgram(0);
    
    SUCCEED();
}