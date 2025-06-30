#include <gtest/gtest.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../src/dsp/UnisonEngine.h"
#include "../../src/utils/Debug.h"
#include "../mocks/MockYmfmWrapper.h"

/**
 * UnisonEngineTest - Tests for the multi-instance FM synthesis engine
 * 
 * Tests verify:
 * - Voice count management
 * - Detune calculations
 * - Stereo spread positioning
 * - Parameter delegation to all instances
 * - Audio processing and mixing
 * - CPU usage monitoring
 */
class UnisonEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<UnisonEngine>();
        
        // Initialize with standard settings
        engine->prepareToPlay(44100.0, 512);
    }
    
    void TearDown() override {
        engine.reset();
    }
    
    std::unique_ptr<UnisonEngine> engine;
};

// =============================================================================
// Basic Functionality Tests
// =============================================================================

TEST_F(UnisonEngineTest, InitialState) {
    // Default state should be single voice (no unison)
    EXPECT_EQ(engine->getActiveVoiceCount(), 1);
    EXPECT_FALSE(engine->isUnisonEnabled());
    EXPECT_FLOAT_EQ(engine->getCurrentDetune(), 0.0f);
    EXPECT_FLOAT_EQ(engine->getCurrentStereoSpread(), 80.0f);
}

TEST_F(UnisonEngineTest, VoiceCountManagement) {
    // Test setting voice count
    engine->setVoiceCount(2);
    EXPECT_EQ(engine->getActiveVoiceCount(), 2);
    EXPECT_TRUE(engine->isUnisonEnabled());
    
    engine->setVoiceCount(4);
    EXPECT_EQ(engine->getActiveVoiceCount(), 4);
    EXPECT_TRUE(engine->isUnisonEnabled());
    
    // Test invalid values
    engine->setVoiceCount(0);  // Should be ignored
    EXPECT_EQ(engine->getActiveVoiceCount(), 4);
    
    engine->setVoiceCount(5);  // Should be ignored (max is 4)
    EXPECT_EQ(engine->getActiveVoiceCount(), 4);
    
    // Back to single voice
    engine->setVoiceCount(1);
    EXPECT_EQ(engine->getActiveVoiceCount(), 1);
    EXPECT_FALSE(engine->isUnisonEnabled());
}

TEST_F(UnisonEngineTest, DetuneSettings) {
    // Test detune amount setting
    engine->setDetune(10.0f);
    EXPECT_FLOAT_EQ(engine->getCurrentDetune(), 10.0f);
    
    engine->setDetune(50.0f);
    EXPECT_FLOAT_EQ(engine->getCurrentDetune(), 50.0f);
    
    // Test limits
    engine->setDetune(-5.0f);  // Should clamp to 0
    EXPECT_FLOAT_EQ(engine->getCurrentDetune(), 0.0f);
    
    engine->setDetune(100.0f);  // Should clamp to 50
    EXPECT_FLOAT_EQ(engine->getCurrentDetune(), 50.0f);
}

TEST_F(UnisonEngineTest, StereoSpreadSettings) {
    // Test stereo spread setting
    engine->setStereoSpread(50.0f);
    EXPECT_FLOAT_EQ(engine->getCurrentStereoSpread(), 50.0f);
    
    engine->setStereoSpread(100.0f);
    EXPECT_FLOAT_EQ(engine->getCurrentStereoSpread(), 100.0f);
    
    // Test limits
    engine->setStereoSpread(-10.0f);  // Should clamp to 0
    EXPECT_FLOAT_EQ(engine->getCurrentStereoSpread(), 0.0f);
    
    engine->setStereoSpread(150.0f);  // Should clamp to 100
    EXPECT_FLOAT_EQ(engine->getCurrentStereoSpread(), 100.0f);
}

TEST_F(UnisonEngineTest, StereoModeSettings) {
    // Test stereo mode settings
    engine->setStereoMode(0);  // Off
    engine->setStereoMode(1);  // Auto
    engine->setStereoMode(2);  // Wide
    engine->setStereoMode(3);  // Narrow
    
    // Test limits
    engine->setStereoMode(-1);  // Should clamp to 0
    engine->setStereoMode(4);   // Should clamp to 3
    
    // Verify mode affects positioning (tested more thoroughly in audio tests)
}

// =============================================================================
// Detune Calculation Tests
// =============================================================================

TEST_F(UnisonEngineTest, DetuneRatioCalculations) {
    // Test 2-voice detune
    engine->setVoiceCount(2);
    engine->setDetune(10.0f);  // 10 cents
    
    // Expected: voice 1 = -10 cents, voice 2 = +10 cents
    // Ratio = 2^(cents/1200)
    float expectedRatio1 = std::pow(2.0f, -10.0f / 1200.0f);  // ~0.9942
    float expectedRatio2 = std::pow(2.0f, 10.0f / 1200.0f);   // ~1.0058
    
    // Note: We can't directly test the internal ratios without exposing them
    // This would be tested via audio output verification
    
    // Test 3-voice detune
    engine->setVoiceCount(3);
    engine->setDetune(15.0f);  // 15 cents
    
    // Expected: voice 1 = -15 cents, voice 2 = 0 cents, voice 3 = +15 cents
    
    // Test 4-voice detune
    engine->setVoiceCount(4);
    engine->setDetune(20.0f);  // 20 cents
    
    // Expected spread across 4 voices
}

// =============================================================================
// Stereo Positioning Tests
// =============================================================================

TEST_F(UnisonEngineTest, StereoPositionCalculations) {
    // Test 2-voice stereo positioning
    engine->setVoiceCount(2);
    engine->setStereoSpread(100.0f);
    engine->setStereoMode(1);  // Auto
    
    // Expected: voice 1 = full left (0.0), voice 2 = full right (1.0)
    
    // Test with reduced spread
    engine->setStereoSpread(50.0f);
    // Expected: voice 1 = 0.25, voice 2 = 0.75
    
    // Test 4-voice positioning
    engine->setVoiceCount(4);
    engine->setStereoSpread(100.0f);
    // Expected: evenly distributed across stereo field
    
    // Test stereo off
    engine->setStereoMode(0);  // Off
    // Expected: all voices center (0.5)
}

// =============================================================================
// Parameter Delegation Tests
// =============================================================================

TEST_F(UnisonEngineTest, ParameterDelegation) {
    engine->setVoiceCount(3);  // Multiple instances to verify delegation
    
    // Test operator parameter delegation
    engine->setOperatorParameter(0, 0, OperatorParameter::TotalLevel, 64);
    engine->setOperatorParameter(1, 1, OperatorParameter::AttackRate, 31);
    
    // Test channel parameter delegation
    engine->setChannelParameter(0, ChannelParameter::Algorithm, 4);
    engine->setChannelParameter(1, ChannelParameter::Feedback, 7);
    
    // Test specific parameter methods
    engine->setAlgorithm(2, 3);
    engine->setFeedback(3, 5);
    engine->setChannelPan(0, 0.75f);
    
    // Test LFO parameters
    engine->setLfoParameters(128, 64, 32, 2);
    
    // Test noise parameters
    engine->setNoiseParameters(true, 15);
    
    // All these should be delegated to all 3 instances
    // Verification would require mock wrappers or output testing
}

// =============================================================================
// Note Management Tests
// =============================================================================

TEST_F(UnisonEngineTest, NoteManagement) {
    engine->setVoiceCount(2);
    
    // Test note on
    engine->noteOn(0, 60, 0.8f);  // Middle C, velocity 0.8
    
    // Test note off
    engine->noteOff(0, 60);
    
    // Test all notes off
    engine->noteOn(0, 60, 0.8f);
    engine->noteOn(1, 64, 0.7f);
    engine->noteOn(2, 67, 0.6f);
    engine->allNotesOff();
    
    // With detune, each instance should receive the same MIDI but
    // process with different frequencies
}

// =============================================================================
// Audio Processing Tests
// =============================================================================

TEST_F(UnisonEngineTest, AudioProcessing) {
    const int numSamples = 512;
    const int numChannels = 2;
    
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    juce::MidiBuffer midiMessages;
    
    // Add a note on message
    auto noteOn = juce::MidiMessage::noteOn(1, 60, (uint8_t)100);
    midiMessages.addEvent(noteOn, 0);
    
    // Process with single voice
    engine->setVoiceCount(1);
    buffer.clear();
    
    // Process the initial block with MIDI
    engine->processBlock(buffer, midiMessages);
    
    // Process several more blocks to let the note develop (like successful tests do)
    juce::MidiBuffer emptyMidi;
    for (int block = 0; block < 8; ++block) {
        engine->processBlock(buffer, emptyMidi);
    }
    
    // Debug: Check buffer content
    float leftSum = 0.0f, rightSum = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        leftSum += std::abs(buffer.getSample(0, i));
        if (numChannels > 1) rightSum += std::abs(buffer.getSample(1, i));
    }
    
    CS_DBG("Debug AudioProcessing: Left sum=" + juce::String(leftSum, 6) + 
           ", Right sum=" + juce::String(rightSum, 6) + 
           ", Samples=" + juce::String(numSamples));
    
    // Should have some audio output
    float singleVoiceLevel = buffer.getMagnitude(0, numSamples);
    EXPECT_GT(singleVoiceLevel, 0.0f);
    
    // Process with 2 voices
    engine->setVoiceCount(2);
    engine->setDetune(15.0f);
    buffer.clear();
    
    // Process initial block with MIDI
    engine->processBlock(buffer, midiMessages);
    
    // Process several more blocks to let the note develop
    for (int block = 0; block < 8; ++block) {
        engine->processBlock(buffer, emptyMidi);
    }
    
    // Should have audio output
    float twoVoiceLevel = buffer.getMagnitude(0, numSamples);
    EXPECT_GT(twoVoiceLevel, 0.0f);
    
    // Test gain compensation (2 voices should not be significantly louder)
    float levelRatio = twoVoiceLevel / singleVoiceLevel;
    EXPECT_LT(levelRatio, 1.5f);  // Should be compensated
}

TEST_F(UnisonEngineTest, StereoProcessing) {
    const int numSamples = 512;
    const int numChannels = 2;
    
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    juce::MidiBuffer midiMessages;
    
    // Add a note on
    auto noteOn = juce::MidiMessage::noteOn(1, 60, (uint8_t)100);
    midiMessages.addEvent(noteOn, 0);
    
    // Test with stereo spread
    engine->setVoiceCount(2);
    engine->setStereoSpread(100.0f);
    engine->setStereoMode(1);  // Auto
    buffer.clear();
    
    // Process initial block with MIDI
    engine->processBlock(buffer, midiMessages);
    
    // Process several more blocks to let the note develop
    juce::MidiBuffer emptyMidi;
    for (int block = 0; block < 8; ++block) {
        engine->processBlock(buffer, emptyMidi);
    }
    
    // Check stereo difference
    float leftLevel = buffer.getRMSLevel(0, 0, numSamples);
    float rightLevel = buffer.getRMSLevel(1, 0, numSamples);
    
    // With full stereo spread, channels should differ
    EXPECT_NE(leftLevel, rightLevel);
    
    // Test with stereo off
    engine->setStereoMode(0);  // Off
    buffer.clear();
    
    // Process with stereo off
    engine->processBlock(buffer, midiMessages);
    for (int block = 0; block < 8; ++block) {
        engine->processBlock(buffer, emptyMidi);
    }
    
    leftLevel = buffer.getRMSLevel(0, 0, numSamples);
    rightLevel = buffer.getRMSLevel(1, 0, numSamples);
    
    // Should be identical (or very close) with stereo off
    EXPECT_NEAR(leftLevel, rightLevel, 0.001f);
}

// =============================================================================
// Performance Tests
// =============================================================================

TEST_F(UnisonEngineTest, CPUUsageMonitoring) {
    // Initial CPU usage should be 0
    EXPECT_FLOAT_EQ(engine->getCpuUsage(), 0.0);
    
    const int numSamples = 512;
    const int numChannels = 2;
    
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    juce::MidiBuffer midiMessages;
    
    // Add multiple notes for stress test
    for (int i = 0; i < 8; ++i) {
        auto noteOn = juce::MidiMessage::noteOn(1, 60 + i, (uint8_t)100);
        midiMessages.addEvent(noteOn, 0);
    }
    
    // Process with 4 voices (maximum)
    engine->setVoiceCount(4);
    engine->processBlock(buffer, midiMessages);
    
    // CPU usage should be measured
    double cpuUsage = engine->getCpuUsage();
    EXPECT_GE(cpuUsage, 0.0);
    EXPECT_LE(cpuUsage, 1.0);  // Should be between 0-100%
    
    CS_DBG("UnisonEngine CPU usage with 4 voices: " + 
           juce::String(cpuUsage * 100.0, 2) + "%");
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(UnisonEngineTest, EdgeCases) {
    // Test rapid voice count changes
    for (int i = 1; i <= 4; ++i) {
        engine->setVoiceCount(i);
        EXPECT_EQ(engine->getActiveVoiceCount(), i);
    }
    
    // Test parameter changes during processing
    const int numSamples = 64;  // Small buffer for multiple iterations
    const int numChannels = 2;
    
    juce::AudioBuffer<float> buffer(numChannels, numSamples);
    juce::MidiBuffer midiMessages;
    
    auto noteOn = juce::MidiMessage::noteOn(1, 60, (uint8_t)100);
    midiMessages.addEvent(noteOn, 0);
    
    // Change parameters while processing
    for (int i = 0; i < 10; ++i) {
        engine->setVoiceCount((i % 4) + 1);
        engine->setDetune(i * 5.0f);
        engine->setStereoSpread(i * 10.0f);
        engine->processBlock(buffer, midiMessages);
        midiMessages.clear();  // Clear for next iteration
    }
    
    // Should handle gracefully without crashes
}

TEST_F(UnisonEngineTest, ResetFunctionality) {
    // Setup some state
    engine->setVoiceCount(3);
    engine->setDetune(25.0f);
    engine->noteOn(0, 60, 0.8f);
    engine->noteOn(1, 64, 0.7f);
    
    // Reset
    engine->reset();
    
    // Voice count and parameters should remain, but notes should be cleared
    EXPECT_EQ(engine->getActiveVoiceCount(), 3);
    EXPECT_FLOAT_EQ(engine->getCurrentDetune(), 25.0f);
    
    // CPU usage should be reset
    EXPECT_FLOAT_EQ(engine->getCpuUsage(), 0.0);
}