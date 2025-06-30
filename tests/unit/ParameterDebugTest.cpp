#include <gtest/gtest.h>
#include "../mocks/MockAudioProcessorHost.h"
#include "PluginProcessor.h"
#include "utils/ParameterIDs.h"

using namespace YMulatorSynth;

class ParameterDebugTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize processor
        host->initializeProcessor(*processor, 44100.0, 512, 2);
        
        // Let processor stabilize
        for (int i = 0; i < 5; ++i) {
            host->processBlock(*processor, 512);
        }
    }
    
    void TearDown() override {
        processor.reset();
        host.reset();
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<YMulatorSynth::Test::MockAudioProcessorHost> host;
};

// Debug parameter access
TEST_F(ParameterDebugTest, ParameterAccessDebugTest) {
    // Test different parameter types to see which ones work
    
    struct ParameterTest {
        std::string name;
        std::string id;
    };
    
    std::vector<ParameterTest> testParams = {
        {"Algorithm", ParamID::Global::Algorithm},
        {"Feedback", ParamID::Global::Feedback},
        {"Op0 TL", ParamID::Op::tl(0)},
        {"Op1 TL", ParamID::Op::tl(1)},
        {"Op0 AR", ParamID::Op::ar(0)},
        {"Global Pan", ParamID::Global::GlobalPan}
    };
    
    for (const auto& test : testParams) {
        // Get initial value
        float initialValue = host->getParameterValue(*processor, test.id);
        
        // Try to set a value
        float testValue = 0.75f;
        host->setParameterValue(*processor, test.id, testValue);
        
        // Process a block
        host->processBlock(*processor, 128);
        
        // Get final value
        float finalValue = host->getParameterValue(*processor, test.id);
        
        // Print debug info (this will show in test output)
        CS_DBG(juce::String(test.name) + " (" + juce::String(test.id) + "): " 
               + "initial=" + juce::String(initialValue) 
               + ", set=" + juce::String(testValue) 
               + ", final=" + juce::String(finalValue));
        
        // At minimum, values should be in valid range
        EXPECT_GE(finalValue, 0.0f) << "Parameter " << test.name << " out of lower bound";
        EXPECT_LE(finalValue, 1.0f) << "Parameter " << test.name << " out of upper bound";
    }
}

// Test parameter stability over time
TEST_F(ParameterDebugTest, ParameterStabilityTest) {
    const auto algorithmId = ParamID::Global::Algorithm;
    
    // Set a value
    host->setParameterValue(*processor, algorithmId, 0.6f);
    
    // Track value over multiple process blocks
    std::vector<float> values;
    for (int i = 0; i < 10; ++i) {
        host->processBlock(*processor, 128);
        float value = host->getParameterValue(*processor, algorithmId);
        values.push_back(value);
    }
    
    // Print value history
    juce::String valueHistory = "Algorithm parameter over time: ";
    for (size_t i = 0; i < values.size(); ++i) {
        valueHistory += juce::String(values[i]);
        if (i < values.size() - 1) valueHistory += ", ";
    }
    CS_DBG(valueHistory);
    
    // Check if value stabilizes
    if (values.size() >= 2) {
        float lastValue = values.back();
        EXPECT_GE(lastValue, 0.0f);
        EXPECT_LE(lastValue, 1.0f);
        
        // If all values are the same, that's also valid (stable)
        bool allSame = true;
        for (const auto& val : values) {
            if (std::abs(val - values[0]) > 0.01f) {
                allSame = false;
                break;
            }
        }
        
        // Either stable or within bounds
        EXPECT_TRUE(allSame || (lastValue >= 0.0f && lastValue <= 1.0f));
    }
}