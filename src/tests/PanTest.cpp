#include "PanTest.h"
#include "../dsp/YM2151Registers.h"
#include <cmath>

void PanTest::runAllTests()
{
    CS_FILE_DBG("=== PAN TEST SUITE STARTED ===");
    
    testYmfmPanOutput();
    testPanRegisters();
    testPanTransitions();
    
    CS_FILE_DBG("=== PAN TEST SUITE COMPLETED ===");
}

void PanTest::testYmfmPanOutput()
{
    logTestStart("YmfmPanOutput");
    
    auto wrapper = createTestWrapper();
    if (!wrapper) {
        logTestResult("YmfmPanOutput", false, "Failed to create test wrapper");
        return;
    }
    
    // Setup test voice
    setupTestVoice(*wrapper, TEST_CHANNEL);
    
    // Test each pan position
    CS_FILE_DBG("Testing LEFT pan (0x80)...");
    auto leftMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_LEFT_ONLY);
    
    CS_FILE_DBG("Testing CENTER pan (0xC0)...");
    auto centerMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_CENTER);
    
    CS_FILE_DBG("Testing RIGHT pan (0x40)...");
    auto rightMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_RIGHT_ONLY);
    
    CS_FILE_DBG("Testing OFF pan (0x00)...");
    auto offMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_OFF);
    
    // Log detailed results
    CS_FILE_DBG("PAN TEST RESULTS:");
    CS_FILE_DBG("LEFT   - L:" + juce::String(leftMeasurement.leftRMS, 6) + 
                " R:" + juce::String(leftMeasurement.rightRMS, 6) + 
                " Ratio L:" + juce::String(leftMeasurement.getLeftRatio(), 1) + "% R:" + juce::String(leftMeasurement.getRightRatio(), 1) + "%");
    
    CS_FILE_DBG("CENTER - L:" + juce::String(centerMeasurement.leftRMS, 6) + 
                " R:" + juce::String(centerMeasurement.rightRMS, 6) + 
                " Ratio L:" + juce::String(centerMeasurement.getLeftRatio(), 1) + "% R:" + juce::String(centerMeasurement.getRightRatio(), 1) + "%");
    
    CS_FILE_DBG("RIGHT  - L:" + juce::String(rightMeasurement.leftRMS, 6) + 
                " R:" + juce::String(rightMeasurement.rightRMS, 6) + 
                " Ratio L:" + juce::String(rightMeasurement.getLeftRatio(), 1) + "% R:" + juce::String(rightMeasurement.getRightRatio(), 1) + "%");
    
    CS_FILE_DBG("OFF    - L:" + juce::String(offMeasurement.leftRMS, 6) + 
                " R:" + juce::String(offMeasurement.rightRMS, 6) + 
                " Ratio L:" + juce::String(offMeasurement.getLeftRatio(), 1) + "% R:" + juce::String(offMeasurement.getRightRatio(), 1) + "%");
    
    // Analyze results
    bool leftPanCorrect = leftMeasurement.getLeftRatio() > 80.0f;  // Should be mostly left
    bool rightPanCorrect = rightMeasurement.getRightRatio() > 80.0f; // Should be mostly right
    bool centerPanCorrect = std::abs(centerMeasurement.getLeftRatio() - centerMeasurement.getRightRatio()) < 20.0f; // Should be balanced
    bool offPanCorrect = (offMeasurement.leftRMS + offMeasurement.rightRMS) < 0.001f; // Should be silent
    
    bool allTestsPassed = leftPanCorrect && rightPanCorrect && centerPanCorrect && offPanCorrect;
    
    std::string details = "LEFT:" + std::string(leftPanCorrect ? "PASS" : "FAIL") +
                         " RIGHT:" + std::string(rightPanCorrect ? "PASS" : "FAIL") +
                         " CENTER:" + std::string(centerPanCorrect ? "PASS" : "FAIL") +
                         " OFF:" + std::string(offPanCorrect ? "PASS" : "FAIL");
    
    logTestResult("YmfmPanOutput", allTestsPassed, details);
}

void PanTest::testPanRegisters()
{
    logTestStart("PanRegisters");
    
    auto wrapper = createTestWrapper();
    if (!wrapper) {
        logTestResult("PanRegisters", false, "Failed to create test wrapper");
        return;
    }
    
    // Test register values
    uint8_t testChannel = TEST_CHANNEL;
    uint8_t regAddress = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + testChannel;
    
    // Set different pan values and verify
    wrapper->writeRegister(regAddress, YM2151Regs::PAN_LEFT_ONLY);
    uint8_t leftValue = wrapper->readCurrentRegister(regAddress);
    
    wrapper->writeRegister(regAddress, YM2151Regs::PAN_CENTER);
    uint8_t centerValue = wrapper->readCurrentRegister(regAddress);
    
    wrapper->writeRegister(regAddress, YM2151Regs::PAN_RIGHT_ONLY);
    uint8_t rightValue = wrapper->readCurrentRegister(regAddress);
    
    CS_FILE_DBG("Register test - LEFT:0x" + juce::String::toHexString(leftValue) +
                " CENTER:0x" + juce::String::toHexString(centerValue) +
                " RIGHT:0x" + juce::String::toHexString(rightValue));
    
    bool registersCorrect = ((leftValue & YM2151Regs::MASK_PAN_LR) == YM2151Regs::PAN_LEFT_ONLY) &&
                           ((centerValue & YM2151Regs::MASK_PAN_LR) == YM2151Regs::PAN_CENTER) &&
                           ((rightValue & YM2151Regs::MASK_PAN_LR) == YM2151Regs::PAN_RIGHT_ONLY);
    
    logTestResult("PanRegisters", registersCorrect, "Register read/write verification");
}

void PanTest::testPanTransitions()
{
    logTestStart("PanTransitions");
    
    auto wrapper = createTestWrapper();
    if (!wrapper) {
        logTestResult("PanTransitions", false, "Failed to create test wrapper");
        return;
    }
    
    setupTestVoice(*wrapper, TEST_CHANNEL);
    
    // Test real-time pan changes during playback
    CS_FILE_DBG("Testing pan transitions during playback...");
    
    // Start with center, measure
    auto centerMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_CENTER, 256);
    
    // Switch to left during playback, measure
    auto leftMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_LEFT_ONLY, 256);
    
    // Switch to right during playback, measure
    auto rightMeasurement = measurePanOutput(*wrapper, YM2151Regs::PAN_RIGHT_ONLY, 256);
    
    CS_FILE_DBG("Transition test - CENTER->LEFT->RIGHT completed");
    
    bool transitionWorked = (leftMeasurement.getLeftRatio() > centerMeasurement.getLeftRatio()) &&
                           (rightMeasurement.getRightRatio() > centerMeasurement.getRightRatio());
    
    logTestResult("PanTransitions", transitionWorked, "Real-time pan transitions");
}

std::unique_ptr<YmfmWrapper> PanTest::createTestWrapper()
{
    auto wrapper = std::make_unique<YmfmWrapper>();
    wrapper->initialize(YmfmWrapper::ChipType::OPM, static_cast<uint32_t>(TEST_SAMPLE_RATE));
    return wrapper;
}

void PanTest::setupTestVoice(YmfmWrapper& wrapper, int channel)
{
    // Setup a basic voice for testing
    // Using simple sine wave-like settings
    
    // Operator 1 (carrier) - audible
    wrapper.setOperatorParameter(static_cast<uint8_t>(channel), 0, YmfmWrapper::OperatorParameter::TotalLevel, 32); // Moderate level
    wrapper.setOperatorParameter(static_cast<uint8_t>(channel), 0, YmfmWrapper::OperatorParameter::AttackRate, 31);  // Fast attack
    wrapper.setOperatorParameter(static_cast<uint8_t>(channel), 0, YmfmWrapper::OperatorParameter::Decay1Rate, 0);    // No decay
    wrapper.setOperatorParameter(static_cast<uint8_t>(channel), 0, YmfmWrapper::OperatorParameter::SustainLevel, 0); // Full sustain
    wrapper.setOperatorParameter(static_cast<uint8_t>(channel), 0, YmfmWrapper::OperatorParameter::ReleaseRate, 7);  // Medium release
    wrapper.setOperatorParameter(static_cast<uint8_t>(channel), 0, YmfmWrapper::OperatorParameter::Multiple, 1);     // 1x frequency
    
    // Operators 2-4 (modulators) - silent
    for (int op = 1; op < 4; ++op) {
        wrapper.setOperatorParameter(static_cast<uint8_t>(channel), static_cast<uint8_t>(op), YmfmWrapper::OperatorParameter::TotalLevel, 127); // Silent
    }
    
    // Set algorithm to simple carrier-only (algorithm 0)
    wrapper.setAlgorithm(static_cast<uint8_t>(channel), 0);
    wrapper.setFeedback(static_cast<uint8_t>(channel), 0);
    
    // Start the note
    wrapper.noteOn(static_cast<uint8_t>(channel), static_cast<uint8_t>(TEST_NOTE), static_cast<uint8_t>(TEST_VELOCITY));
}

PanTest::PanMeasurement PanTest::measurePanOutput(YmfmWrapper& wrapper, uint8_t panBits, int sampleCount)
{
    PanMeasurement measurement;
    measurement.sampleCount = sampleCount;
    
    // Set the pan bits
    uint8_t regAddress = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + TEST_CHANNEL;
    uint8_t currentReg = wrapper.readCurrentRegister(regAddress);
    uint8_t otherBits = currentReg & YM2151Regs::PRESERVE_ALG_FB;
    wrapper.writeRegister(regAddress, otherBits | panBits);
    
    CS_FILE_DBG("Measuring pan output - panBits:0x" + juce::String::toHexString(panBits) + 
                " register:0x" + juce::String::toHexString(otherBits | panBits));
    
    // Generate and measure samples
    float leftSum = 0.0f, rightSum = 0.0f;
    float leftSqSum = 0.0f, rightSqSum = 0.0f;
    
    for (int i = 0; i < sampleCount; ++i) {
        float leftSample, rightSample;
        wrapper.generateSamples(&leftSample, &rightSample, 1);
        
        leftSum += std::abs(leftSample);
        rightSum += std::abs(rightSample);
        leftSqSum += leftSample * leftSample;
        rightSqSum += rightSample * rightSample;
        
        measurement.leftLevel = std::max(measurement.leftLevel, std::abs(leftSample));
        measurement.rightLevel = std::max(measurement.rightLevel, std::abs(rightSample));
    }
    
    measurement.leftRMS = std::sqrt(leftSqSum / sampleCount);
    measurement.rightRMS = std::sqrt(rightSqSum / sampleCount);
    
    return measurement;
}

void PanTest::logTestStart(const std::string& testName)
{
    CS_FILE_DBG("--- Starting test: " + testName + " ---");
}

void PanTest::logTestResult(const std::string& testName, bool passed, const std::string& details)
{
    std::string result = passed ? "PASSED" : "FAILED";
    std::string message = "Test " + testName + ": " + result;
    if (!details.empty()) {
        message += " (" + details + ")";
    }
    CS_FILE_DBG(message);
}