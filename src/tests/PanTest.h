#pragma once

#include <memory>
#include "../dsp/YmfmWrapper.h"
#include "../utils/Debug.h"

/**
 * PanTest - Test suite for verifying pan functionality
 * 
 * This class provides isolated testing of YM2151 pan register functionality
 * by directly testing ymfm library output without interference from DAW or UI.
 */
class PanTest {
public:
    // Main test runner
    static void runAllTests();
    
    // Individual test methods
    static void testYmfmPanOutput();
    static void testPanRegisters();
    static void testPanTransitions();
    
private:
    // Test utilities
    static std::unique_ptr<YmfmWrapper> createTestWrapper();
    static void logTestResult(const std::string& testName, bool passed, const std::string& details = "");
    static void logTestStart(const std::string& testName);
    
    // Pan output measurement
    struct PanMeasurement {
        float leftLevel = 0.0f;
        float rightLevel = 0.0f;
        float leftRMS = 0.0f;
        float rightRMS = 0.0f;
        int sampleCount = 0;
        
        float getLeftRatio() const {
            float total = leftRMS + rightRMS;
            return total > 0.0f ? (leftRMS / total) * 100.0f : 0.0f;
        }
        
        float getRightRatio() const {
            float total = leftRMS + rightRMS;
            return total > 0.0f ? (rightRMS / total) * 100.0f : 0.0f;
        }
    };
    
    static PanMeasurement measurePanOutput(YmfmWrapper& wrapper, uint8_t panBits, int sampleCount = 1024);
    static void setupTestVoice(YmfmWrapper& wrapper, int channel);
    
    // Test constants
    static constexpr int TEST_CHANNEL = 0;
    static constexpr int TEST_NOTE = 69; // A4
    static constexpr int TEST_VELOCITY = 100;
    static constexpr int TEST_SAMPLE_COUNT = 1024;
    static constexpr double TEST_SAMPLE_RATE = 44100.0;
};