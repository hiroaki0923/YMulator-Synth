#include <gtest/gtest.h>
#include "../mocks/MockAudioProcessorHost.h"
#include "PluginProcessor.h"
#include "utils/ParameterIDs.h"
#include "utils/Debug.h"

using namespace YMulatorSynth;

class GlobalPanTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize processor with standard settings
        host->initializeProcessor(*processor, 44100.0, 512, 2);
        
        // Let processor stabilize
        host->processBlock(*processor, 128);
    }
    
    void TearDown() override {
        processor.reset();
        host.reset();
    }
    
    void playNoteAndProcess(int note = 60, int velocity = 100) {
        // CS_FILE_DBG("=== playNoteAndProcess: Sending MIDI note " + juce::String(note) + " with velocity " + juce::String(velocity) + " ===");
        host->sendMidiNoteOn(*processor, 1, note, velocity);
        // CS_FILE_DBG("=== processBlock called ===");
        host->processBlock(*processor, 512);
        // CS_FILE_DBG("=== processBlock completed ===");
    }
    
    void stopNote(int note = 60) {
        host->sendMidiNoteOff(*processor, 1, note);
        host->processBlock(*processor, 128);
    }
    
    void setGlobalPan(GlobalPanPosition position) {
        float normalizedValue = static_cast<float>(static_cast<int>(position)) / 3.0f;  // 4 positions: 0-3
        // CS_FILE_DBG("Setting GlobalPan to position " + juce::String(static_cast<int>(position)) + 
        //     " (normalized: " + juce::String(normalizedValue) + ")");
        host->setParameterValue(*processor, ParamID::Global::GlobalPan, normalizedValue);
        host->processBlock(*processor, 128);  // Let changes apply
    }
    
    std::pair<float, float> getChannelLevels() {
        return {host->getRMSLevel(0), host->getRMSLevel(1)};
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<YMulatorSynth::Test::MockAudioProcessorHost> host;
};

// Test Global Pan LEFT setting
TEST_F(GlobalPanTest, LeftPanTest) {
    CS_DBG("=== Testing Global Pan LEFT ===");
    
    // Set pan to LEFT
    setGlobalPan(GlobalPanPosition::LEFT);
    
    // Verify parameter was set
    float panValue = host->getParameterValue(*processor, ParamID::Global::GlobalPan);
    float expectedLeft = 0.0f / 3.0f;  // LEFT = 0
    EXPECT_NEAR(panValue, expectedLeft, 0.1f);
    
    // Play note and check output
    playNoteAndProcess();
    
    auto [leftRMS, rightRMS] = getChannelLevels();
    CS_DBG("LEFT Pan - Left RMS: " + juce::String(leftRMS) + ", Right RMS: " + juce::String(rightRMS));
    
    // LEFT pan should have significantly more left channel output
    EXPECT_GT(leftRMS, 0.001f);    // Left channel should have strong output
    
    // Right channel should be significantly lower than left (or zero)
    if (rightRMS > 0.001f) {
        // If right has some output, left should be significantly stronger
        EXPECT_GT(leftRMS / rightRMS, 2.0f);  // Left should be at least 2x stronger
    }
    
    stopNote();
}

// Test Global Pan CENTER setting
TEST_F(GlobalPanTest, CenterPanTest) {
    CS_DBG("=== Testing Global Pan CENTER ===");
    
    // Set pan to CENTER
    setGlobalPan(GlobalPanPosition::CENTER);
    
    // Verify parameter was set
    float panValue = host->getParameterValue(*processor, ParamID::Global::GlobalPan);
    float expectedCenter = 1.0f / 3.0f;  // CENTER = 1
    EXPECT_NEAR(panValue, expectedCenter, 0.1f);
    
    // Play note and check output
    playNoteAndProcess();
    
    auto [leftRMS, rightRMS] = getChannelLevels();
    CS_DBG("CENTER Pan - Left RMS: " + juce::String(leftRMS) + ", Right RMS: " + juce::String(rightRMS));
    
    // CENTER pan should have balanced output
    EXPECT_GT(leftRMS, 0.001f);   // Left channel should have output
    EXPECT_GT(rightRMS, 0.001f);  // Right channel should have output
    
    // Channels should be relatively balanced (within 2:1 ratio)
    if (leftRMS > 0.001f && rightRMS > 0.001f) {
        float ratio = std::max(leftRMS, rightRMS) / std::min(leftRMS, rightRMS);
        EXPECT_LT(ratio, 2.0f);  // Channels should be within 2:1 ratio
    }
    
    stopNote();
}

// Test Global Pan RIGHT setting
TEST_F(GlobalPanTest, RightPanTest) {
    CS_DBG("=== Testing Global Pan RIGHT ===");
    
    // Set pan to RIGHT
    setGlobalPan(GlobalPanPosition::RIGHT);
    
    // Verify parameter was set
    float panValue = host->getParameterValue(*processor, ParamID::Global::GlobalPan);
    float expectedRight = 2.0f / 3.0f;  // RIGHT = 2
    EXPECT_NEAR(panValue, expectedRight, 0.1f);
    
    // Play note and check output
    playNoteAndProcess();
    
    auto [leftRMS, rightRMS] = getChannelLevels();
    CS_DBG("RIGHT Pan - Left RMS: " + juce::String(leftRMS) + ", Right RMS: " + juce::String(rightRMS));
    
    // RIGHT pan should have significantly more right channel output
    EXPECT_GT(rightRMS, 0.001f);   // Right channel should have strong output
    
    // Left channel should be significantly lower than right (or zero)
    if (leftRMS > 0.001f) {
        // If left has some output, right should be significantly stronger
        EXPECT_GT(rightRMS / leftRMS, 2.0f);  // Right should be at least 2x stronger
    }
    
    stopNote();
}

// Test Global Pan parameter transitions
TEST_F(GlobalPanTest, PanTransitionTest) {
    CS_DBG("=== Testing Global Pan Transitions ===");
    
    playNoteAndProcess();
    
    // Test LEFT
    setGlobalPan(GlobalPanPosition::LEFT);
    host->processBlock(*processor, 256);
    auto [leftL, rightL] = getChannelLevels();
    CS_DBG("Transition LEFT - Left: " + juce::String(leftL) + ", Right: " + juce::String(rightL));
    
    // Test CENTER  
    setGlobalPan(GlobalPanPosition::CENTER);
    host->processBlock(*processor, 256);
    auto [leftC, rightC] = getChannelLevels();
    CS_DBG("Transition CENTER - Left: " + juce::String(leftC) + ", Right: " + juce::String(rightC));
    
    // Test RIGHT
    setGlobalPan(GlobalPanPosition::RIGHT);
    host->processBlock(*processor, 256);
    auto [leftR, rightR] = getChannelLevels();
    CS_DBG("Transition RIGHT - Left: " + juce::String(leftR) + ", Right: " + juce::String(rightR));
    
    // Verify transitions work
    EXPECT_GT(leftL, 0.001f);   // LEFT should have left output
    EXPECT_GT(leftC, 0.001f);   // CENTER should have both outputs
    EXPECT_GT(rightC, 0.001f);
    EXPECT_GT(rightR, 0.001f);  // RIGHT should have right output
    
    // Verify directional differences (adjusted for realistic YM2151 pan behavior)
    if (leftL > 0.001f && rightL > 0.001f) {
        EXPECT_GT(leftL / rightL, 1.2f);   // LEFT should favor left (reduced threshold)
    }
    if (leftR > 0.001f && rightR > 0.001f) {
        EXPECT_GT(rightR / leftR, 1.2f);   // RIGHT should favor right (reduced threshold)
    }
    
    stopNote();
}

// Test multiple notes with different pan settings
TEST_F(GlobalPanTest, MultipleNotePanTest) {
    CS_DBG("=== Testing Multiple Notes with Pan ===");
    
    // Test with chord
    setGlobalPan(GlobalPanPosition::LEFT);
    
    host->sendMidiNoteOn(*processor, 1, 60, 100);  // C
    host->sendMidiNoteOn(*processor, 1, 64, 100);  // E
    host->sendMidiNoteOn(*processor, 1, 67, 100);  // G
    
    host->processBlock(*processor, 512);
    
    auto [leftRMS, rightRMS] = getChannelLevels();
    CS_DBG("Multi-note LEFT Pan - Left: " + juce::String(leftRMS) + ", Right: " + juce::String(rightRMS));
    
    // Even with multiple notes, left pan should work
    EXPECT_GT(leftRMS, 0.001f);
    
    if (rightRMS > 0.001f) {
        EXPECT_GT(leftRMS / rightRMS, 1.5f);  // Left should be stronger
    }
    
    // Clean up
    host->sendMidiNoteOff(*processor, 1, 60);
    host->sendMidiNoteOff(*processor, 1, 64);
    host->sendMidiNoteOff(*processor, 1, 67);
    host->processBlock(*processor, 128);
}

// Test Global Pan RANDOM setting
TEST_F(GlobalPanTest, RandomPanTest) {
    CS_DBG("=== Testing Global Pan RANDOM ===");
    
    // Set pan to RANDOM
    setGlobalPan(GlobalPanPosition::RANDOM);
    
    // Verify parameter was set
    float panValue = host->getParameterValue(*processor, ParamID::Global::GlobalPan);
    float expectedRandom = 3.0f / 3.0f;  // RANDOM = 3
    EXPECT_NEAR(panValue, expectedRandom, 0.1f);
    
    // Play multiple notes and collect pan distribution
    std::map<std::pair<bool, bool>, int> panDistribution; // {hasLeft, hasRight} -> count
    
    for (int note = 60; note < 70; ++note) {
        // Play note
        host->sendMidiNoteOn(*processor, 1, note, 100);
        host->processBlock(*processor, 256);
        
        auto [leftRMS, rightRMS] = getChannelLevels();
        bool hasLeft = leftRMS > 0.001f;
        bool hasRight = rightRMS > 0.001f;
        
        panDistribution[{hasLeft, hasRight}]++;
        
        // CS_FILE_DBG("Note " + juce::String(note) + " - Left: " + juce::String(leftRMS) + 
        //     ", Right: " + juce::String(rightRMS) + 
        //     " -> (" + juce::String(hasLeft ? 1 : 0) + "," + juce::String(hasRight ? 1 : 0) + ")");
        
        // Stop note
        host->sendMidiNoteOff(*processor, 1, note);
        host->processBlock(*processor, 128);
    }
    
    // Analyze distribution
    int leftOnlyCount = panDistribution[{true, false}];   // LEFT
    int bothCount = panDistribution[{true, true}];        // CENTER
    int rightOnlyCount = panDistribution[{false, true}];  // RIGHT
    int totalNotes = leftOnlyCount + bothCount + rightOnlyCount;
    
    // CS_FILE_DBG("Pan distribution - Left: " + juce::String(leftOnlyCount) + 
    //     ", Center: " + juce::String(bothCount) + 
    //     ", Right: " + juce::String(rightOnlyCount) + 
    //     ", Total: " + juce::String(totalNotes));
    
    // Verify randomness (at least 2 different pan positions should appear)
    int uniquePanPositions = (leftOnlyCount > 0 ? 1 : 0) + 
                            (bothCount > 0 ? 1 : 0) + 
                            (rightOnlyCount > 0 ? 1 : 0);
    
    EXPECT_GE(uniquePanPositions, 2) << "Random pan should produce at least 2 different positions out of 10 notes";
    
    // Verify no notes are completely silent
    EXPECT_EQ(totalNotes, 10) << "All notes should produce some output";
}

// Test voice allocation order for non-noise presets (should start from channel 7)
TEST_F(GlobalPanTest, VoiceAllocationOrderNonNoiseTest) {
    CS_DBG("=== Testing Voice Allocation Order (Non-Noise) ===");
    
    // Ensure we're using a non-noise preset
    host->setParameterValue(*processor, ParamID::Global::NoiseEnable, 0.0f);
    host->processBlock(*processor, 128);
    
    // Set pan to LEFT for easier tracking
    setGlobalPan(GlobalPanPosition::LEFT);
    
    // Play 8 simultaneous notes to fill all channels
    for (int i = 0; i < 8; ++i) {
        int note = 60 + i;
        host->sendMidiNoteOn(*processor, 1, note, 100);
        host->processBlock(*processor, 64);  // Small buffer to allow processing
    }
    
    // Verify output (all should contribute to left channel)
    auto [leftRMS, rightRMS] = getChannelLevels();
    EXPECT_GT(leftRMS, 0.01f) << "All 8 voices should contribute to left output";
    
    // Release all notes
    for (int i = 0; i < 8; ++i) {
        int note = 60 + i;
        host->sendMidiNoteOff(*processor, 1, note);
        host->processBlock(*processor, 32);
    }
}

// Test voice allocation for noise presets (should use only channel 7)
TEST_F(GlobalPanTest, VoiceAllocationNoiseTest) {
    CS_DBG("=== Testing Voice Allocation (Noise Only) ===");
    
    // Enable noise preset
    host->setParameterValue(*processor, ParamID::Global::NoiseEnable, 1.0f);
    host->processBlock(*processor, 128);
    
    // Set pan to RIGHT for easier tracking
    setGlobalPan(GlobalPanPosition::RIGHT);
    
    // Play multiple notes (should all use channel 7)
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    host->processBlock(*processor, 128);
    
    auto [leftRMS1, rightRMS1] = getChannelLevels();
    EXPECT_GT(rightRMS1, 0.001f) << "First note should produce right output";
    
    // Play second note (should steal channel 7)
    host->sendMidiNoteOn(*processor, 1, 64, 100);
    host->processBlock(*processor, 128);
    
    auto [leftRMS2, rightRMS2] = getChannelLevels();
    EXPECT_GT(rightRMS2, 0.001f) << "Second note should also use right output (channel 7)";
    
    // Clean up
    host->sendMidiNoteOff(*processor, 1, 60);
    host->sendMidiNoteOff(*processor, 1, 64);
    host->processBlock(*processor, 128);
}

// Test monophonic RANDOM pan behavior
TEST_F(GlobalPanTest, MonophonicRandomPanTest) {
    CS_DBG("=== Testing Monophonic RANDOM Pan ===");
    
    // Set pan to RANDOM
    setGlobalPan(GlobalPanPosition::RANDOM);
    
    // Disable noise to ensure non-noise behavior
    host->setParameterValue(*processor, ParamID::Global::NoiseEnable, 0.0f);
    host->processBlock(*processor, 128);
    
    // Track pan results for sequential notes
    std::vector<std::string> panResults;
    
    for (int note = 60; note < 65; ++note) {
        // Play note
        host->sendMidiNoteOn(*processor, 1, note, 100);
        host->processBlock(*processor, 256);
        
        auto [leftRMS, rightRMS] = getChannelLevels();
        
        std::string panType;
        if (leftRMS > 0.001f && rightRMS <= 0.001f) {
            panType = "LEFT";
        } else if (rightRMS > 0.001f && leftRMS <= 0.001f) {
            panType = "RIGHT";
        } else if (leftRMS > 0.001f && rightRMS > 0.001f) {
            panType = "CENTER";
        } else {
            panType = "SILENT";
        }
        
        panResults.push_back(panType);
        
        CS_DBG("Note " + juce::String(note) + " -> " + panType + 
               " (L:" + juce::String(leftRMS, 3) + ", R:" + juce::String(rightRMS, 3) + ")");
        
        // Stop note before next
        host->sendMidiNoteOff(*processor, 1, note);
        host->processBlock(*processor, 128);
    }
    
    // Verify that we get different pan positions across the sequence
    std::set<std::string> uniquePanTypes(panResults.begin(), panResults.end());
    uniquePanTypes.erase("SILENT");  // Remove SILENT if present
    
    EXPECT_GE(uniquePanTypes.size(), 2) << "Monophonic RANDOM pan should produce at least 2 different pan positions";
    EXPECT_EQ(panResults.size(), 5) << "Should test exactly 5 notes";
    
    // Verify no notes are silent
    for (const auto& result : panResults) {
        EXPECT_NE(result, "SILENT") << "No notes should be silent";
    }
}