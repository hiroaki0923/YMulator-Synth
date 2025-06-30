#include <gtest/gtest.h>
#include "../mocks/MockAudioProcessorHost.h"
#include "PluginProcessor.h"
#include "utils/ParameterIDs.h"

using namespace YMulatorSynth;

class SimpleParameterTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize processor with minimal settings
        host->initializeProcessor(*processor, 44100.0, 512, 2);
    }
    
    void TearDown() override {
        if (processor) {
            processor->resetProcessBlockStaticState();
            ymulatorsynth::ParameterManager::resetStaticState();
        }
        processor.reset();
        host.reset();
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<YMulatorSynth::Test::MockAudioProcessorHost> host;
};

// Test parameter discovery and basic access
TEST_F(SimpleParameterTest, ParameterDiscoveryTest) {
    // Test that the processor can be initialized without crashing
    EXPECT_NE(processor, nullptr);
    
    // Try to access a parameter through the mock host
    const auto algorithmId = ParamID::Global::Algorithm;
    
    // This should not crash even if parameter doesn't exist
    float initialValue = host->getParameterValue(*processor, algorithmId);
    
    // Value should be in valid range or 0 if parameter not found
    EXPECT_GE(initialValue, 0.0f);
    EXPECT_LE(initialValue, 1.0f);
}

// Test mock host parameter setting with proper quantization understanding
TEST_F(SimpleParameterTest, MockHostParameterTest) {
    // Test if our mock host can find and set parameters correctly
    const auto algorithmId = ParamID::Global::Algorithm;
    
    // Get initial value through host
    float initialValue = host->getParameterValue(*processor, algorithmId);
    EXPECT_GE(initialValue, 0.0f);
    EXPECT_LE(initialValue, 1.0f);
    
    // Algorithm parameter has 8 discrete values (0-7) in YM2151 hardware
    // Setting 0.75f should quantize to either 5/7 ≈ 0.714 or 6/7 ≈ 0.857
    host->setParameterValue(*processor, algorithmId, 0.75f);
    
    // Get the actual quantized value
    float actualValue = host->getParameterValue(*processor, algorithmId);
    
    // Test that value changed from initial and is within expected quantized range
    EXPECT_NE(actualValue, initialValue);
    EXPECT_GE(actualValue, 0.7f);   // Should be at least algorithm 5 (5/7 ≈ 0.714)
    EXPECT_LE(actualValue, 0.9f);   // Should be at most algorithm 6 (6/7 ≈ 0.857)
    
    // Verify the value is exactly one of the valid quantized values
    bool isValidQuantization = 
        (std::abs(actualValue - (5.0f/7.0f)) < 0.001f) ||  // Algorithm 5
        (std::abs(actualValue - (6.0f/7.0f)) < 0.001f);    // Algorithm 6
    EXPECT_TRUE(isValidQuantization) << "Value " << actualValue 
                                     << " is not properly quantized to YM2151 algorithm range";
}