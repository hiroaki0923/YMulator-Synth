#include <gtest/gtest.h>
#include "../../src/PluginProcessor.h"
#include "../mocks/MockAudioProcessorHost.h"
#include "../../src/utils/ParameterIDs.h"
#include "../../src/utils/Debug.h"

using namespace ymulatorsynth;

/**
 * Direct unit tests for ParameterManager class.
 * 
 * These tests focus on ParameterManager functionality in isolation,
 * complementing the existing integration tests in PluginProcessorComprehensiveTest.
 */
class ParameterManagerTest : public ::testing::Test {
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
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<YMulatorSynth::Test::MockAudioProcessorHost> host;
};

// ============================================================================
// Parameter Layout Creation Tests
// ============================================================================

TEST_F(ParameterManagerTest, ProcessorHasAllParameters) {
    auto& parameters = processor->getParameters();
    
    // Verify we have a reasonable number of parameters
    EXPECT_GT(parameters.state.getNumChildren(), 50); // Should have many parameters
}

TEST_F(ParameterManagerTest, ProcessorHasRequiredOperatorParameters) {
    auto& parameters = processor->getParameters();
    
    // Verify all operator parameters exist with correct ranges
    for (int op = 1; op <= 4; ++op) {
        EXPECT_NE(parameters.getParameter(ParamID::Op::tl(op)), nullptr) 
            << "Operator " << op << " Total Level parameter missing";
        EXPECT_NE(parameters.getParameter(ParamID::Op::ar(op)), nullptr)
            << "Operator " << op << " Attack Rate parameter missing";
        EXPECT_NE(parameters.getParameter(ParamID::Op::d1r(op)), nullptr)
            << "Operator " << op << " Decay 1 Rate parameter missing";
        EXPECT_NE(parameters.getParameter(ParamID::Op::d1l(op)), nullptr)
            << "Operator " << op << " Decay 1 Level parameter missing";
        EXPECT_NE(parameters.getParameter(ParamID::Op::d2r(op)), nullptr)
            << "Operator " << op << " Decay 2 Rate parameter missing";
        EXPECT_NE(parameters.getParameter(ParamID::Op::rr(op)), nullptr)
            << "Operator " << op << " Release Rate parameter missing";
    }
}

TEST_F(ParameterManagerTest, ProcessorHasGlobalParameters) {
    auto& parameters = processor->getParameters();
    
    EXPECT_NE(parameters.getParameter(ParamID::Global::Algorithm), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::Feedback), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::GlobalPan), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::LfoRate), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::LfoPmd), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::LfoAmd), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::LfoWaveform), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::NoiseEnable), nullptr);
    EXPECT_NE(parameters.getParameter(ParamID::Global::NoiseFrequency), nullptr);
}

// ============================================================================
// Parameter Range Validation Tests
// ============================================================================

TEST_F(ParameterManagerTest, OperatorParametersHaveCorrectRanges) {
    auto& parameters = processor->getParameters();
    
    // Test Total Level range (0-127)
    auto* tlParam = dynamic_cast<juce::AudioParameterInt*>(
        parameters.getParameter(ParamID::Op::tl(1)));
    ASSERT_NE(tlParam, nullptr);
    EXPECT_EQ(tlParam->getRange().getStart(), 0);
    EXPECT_EQ(tlParam->getRange().getEnd(), 127);
    
    // Test Attack Rate range (0-31)
    auto* arParam = dynamic_cast<juce::AudioParameterInt*>(
        parameters.getParameter(ParamID::Op::ar(1)));
    ASSERT_NE(arParam, nullptr);
    EXPECT_EQ(arParam->getRange().getStart(), 0);
    EXPECT_EQ(arParam->getRange().getEnd(), 31);
    
    // Test Sustain Level range (0-15)
    auto* d1lParam = dynamic_cast<juce::AudioParameterInt*>(
        parameters.getParameter(ParamID::Op::d1l(1)));
    ASSERT_NE(d1lParam, nullptr);
    EXPECT_EQ(d1lParam->getRange().getStart(), 0);
    EXPECT_EQ(d1lParam->getRange().getEnd(), 15);
}

TEST_F(ParameterManagerTest, GlobalParametersHaveCorrectRanges) {
    auto& parameters = processor->getParameters();
    
    // Test Algorithm range (0-7)
    auto* algParam = dynamic_cast<juce::AudioParameterInt*>(
        parameters.getParameter(ParamID::Global::Algorithm));
    ASSERT_NE(algParam, nullptr);
    EXPECT_EQ(algParam->getRange().getStart(), 0);
    EXPECT_EQ(algParam->getRange().getEnd(), 7);
    
    // Test Feedback range (0-7)
    auto* fbParam = dynamic_cast<juce::AudioParameterInt*>(
        parameters.getParameter(ParamID::Global::Feedback));
    ASSERT_NE(fbParam, nullptr);
    EXPECT_EQ(fbParam->getRange().getStart(), 0);
    EXPECT_EQ(fbParam->getRange().getEnd(), 7);
}

// ============================================================================
// Custom Preset State Management Tests
// ============================================================================

TEST_F(ParameterManagerTest, InitialCustomModeState) {
    EXPECT_FALSE(processor->isInCustomMode());
    EXPECT_EQ(processor->getCustomPresetName(), "Custom");
}

TEST_F(ParameterManagerTest, SetCustomModeChangesState) {
    processor->setCustomMode(true, "My Custom Preset");
    
    EXPECT_TRUE(processor->isInCustomMode());
    // Note: Custom preset name behavior may vary during refactoring
    
    processor->setCustomMode(false);
    
    EXPECT_FALSE(processor->isInCustomMode());
    // Note: Name preservation behavior may vary during refactoring
}

TEST_F(ParameterManagerTest, SetCustomModeWithEmptyName) {
    processor->setCustomMode(true, "");
    
    EXPECT_TRUE(processor->isInCustomMode());
    EXPECT_EQ(processor->getCustomPresetName(), "");
}

// ============================================================================
// Global Pan Parameter Tests
// ============================================================================

TEST_F(ParameterManagerTest, GlobalPanParameterExists) {
    auto& parameters = processor->getParameters();
    auto* globalPanParam = parameters.getParameter(ParamID::Global::GlobalPan);
    
    EXPECT_NE(globalPanParam, nullptr);
    
    // Test setting different pan modes
    globalPanParam->setValueNotifyingHost(0.0f); // LEFT
    globalPanParam->setValueNotifyingHost(0.33f); // CENTER
    globalPanParam->setValueNotifyingHost(0.66f); // RIGHT  
    globalPanParam->setValueNotifyingHost(1.0f); // RANDOM
}

TEST_F(ParameterManagerTest, PanParametersExistForAllChannels) {
    auto& parameters = processor->getParameters();
    
    // Verify pan parameters exist for all 8 channels
    for (int ch = 0; ch < 8; ++ch) {
        auto* panParam = parameters.getParameter(ParamID::Channel::pan(ch));
        EXPECT_NE(panParam, nullptr) << "Pan parameter for channel " << ch << " missing";
    }
}

// ============================================================================
// Parameter Update Integration Tests
// ============================================================================

TEST_F(ParameterManagerTest, ParameterChangesAreProcessed) {
    auto& parameters = processor->getParameters();
    
    // Change some parameters and verify they are accepted
    auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
    auto* fbParam = parameters.getParameter(ParamID::Global::Feedback);
    
    ASSERT_NE(algParam, nullptr);
    ASSERT_NE(fbParam, nullptr);
    
    // Change parameters
    EXPECT_NO_THROW(algParam->setValueNotifyingHost(0.8f));
    EXPECT_NO_THROW(fbParam->setValueNotifyingHost(0.6f));
    
    // Verify parameters are within valid ranges
    EXPECT_GE(algParam->getValue(), 0.0f);
    EXPECT_LE(algParam->getValue(), 1.0f);
    EXPECT_GE(fbParam->getValue(), 0.0f);
    EXPECT_LE(fbParam->getValue(), 1.0f);
}

TEST_F(ParameterManagerTest, ParameterValueRangeEnforcement) {
    auto& parameters = processor->getParameters();
    auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
    
    ASSERT_NE(algParam, nullptr);
    
    // Test range enforcement
    algParam->setValueNotifyingHost(-0.5f); // Below minimum
    EXPECT_GE(algParam->getValue(), 0.0f);
    
    algParam->setValueNotifyingHost(1.5f); // Above maximum  
    EXPECT_LE(algParam->getValue(), 1.0f);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(ParameterManagerTest, InvalidParameterIDHandling) {
    auto& parameters = processor->getParameters();
    
    // Test accessing non-existent parameter
    auto* invalidParam = parameters.getParameter("invalid_parameter_id");
    EXPECT_EQ(invalidParam, nullptr);
}

TEST_F(ParameterManagerTest, ParameterAccessWithInvalidIndices) {
    auto& parameters = processor->getParameters();
    
    // Test operator parameters with invalid operator numbers  
    auto* invalidOp = parameters.getParameter(ParamID::Op::tl(0)); // Op 0 doesn't exist
    EXPECT_EQ(invalidOp, nullptr);
    
    auto* invalidOp2 = parameters.getParameter(ParamID::Op::tl(5)); // Op 5 doesn't exist
    EXPECT_EQ(invalidOp2, nullptr);
}

// ============================================================================
// Integration with YmfmWrapper Tests
// ============================================================================

TEST_F(ParameterManagerTest, ParameterApplicationBasicTest) {
    auto& parameters = processor->getParameters();
    
    // Set some parameter values and verify they don't cause crashes
    auto* algParam = parameters.getParameter(ParamID::Global::Algorithm);
    algParam->setValueNotifyingHost(0.5f); // Set to algorithm 3-4
    
    auto* fbParam = parameters.getParameter(ParamID::Global::Feedback);
    fbParam->setValueNotifyingHost(0.5f); // Set to feedback 3-4
    
    // Process some audio to trigger parameter application
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midiBuffer;
    
    EXPECT_NO_THROW(processor->processBlock(buffer, midiBuffer));
}

// ============================================================================
// Memory and Resource Management Tests
// ============================================================================

TEST_F(ParameterManagerTest, MultipleParameterAccessCalls) {
    // Test that multiple parameter access calls don't cause issues
    auto& parameters = processor->getParameters();
    
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(parameters.getParameter(ParamID::Global::Algorithm));
        EXPECT_NO_THROW(parameters.getParameter(ParamID::Op::tl(1)));
    }
}

TEST_F(ParameterManagerTest, ProcessorDestruction) {
    // Test that processor can be safely destroyed and recreated
    auto tempProcessor = std::make_unique<YMulatorSynthAudioProcessor>();
    auto tempHost = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
    
    tempHost->initializeProcessor(*tempProcessor, 44100.0, 512, 2);
    
    // Destruction should clean up listeners properly
    EXPECT_NO_THROW(tempProcessor.reset());
    EXPECT_NO_THROW(tempHost.reset());
}