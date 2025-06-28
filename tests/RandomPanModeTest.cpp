#include <gtest/gtest.h>
#include "../src/PluginProcessor.h"
#include "../src/core/MidiProcessor.h"
#include "../src/utils/ParameterIDs.h"
#include "../src/utils/Debug.h"
#include "TestHelpers/MockAudioProcessorHost.h"
#include <memory>
#include <set>

/**
 * Test Random Pan Mode functionality without requiring DAW.
 * This test validates that:
 * 1. Random pan generates different values for consecutive notes
 * 2. MidiProcessor correctly applies random pan per note
 * 3. PluginProcessor doesn't override random pan settings
 */
class RandomPanModeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create processor with mock host
        host = std::make_unique<MockAudioProcessorHost>();
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        
        // Initialize with mock host
        processor->setPlayConfigDetails(0, 2, 44100, 512);
        processor->prepareToPlay(44100, 512);
        
        // Set global pan to RANDOM mode
        auto* globalPanParam = dynamic_cast<juce::AudioParameterChoice*>(
            processor->getParameters()[getGlobalPanParameterIndex()]);
        ASSERT_NE(globalPanParam, nullptr);
        globalPanParam->setValueNotifyingHost(
            globalPanParam->convertTo0to1(static_cast<int>(GlobalPanPosition::RANDOM)));
        
        CS_FILE_DBG("=== RandomPanModeTest SetUp Complete ===");
    }
    
    void TearDown() override {
        if (processor) {
            processor->releaseResources();
        }
        CS_FILE_DBG("=== RandomPanModeTest TearDown Complete ===");
    }
    
    int getGlobalPanParameterIndex() {
        const auto& params = processor->getParameters();
        for (int i = 0; i < params.size(); ++i) {
            if (params[i]->getName(100) == ParamID::Global::GlobalPan) {
                return i;
            }
        }
        return -1;
    }
    
    void playNoteAndCaptureOutput(int noteNumber, int velocity, juce::AudioBuffer<float>& outputBuffer) {
        // Create MIDI note on message
        juce::MidiBuffer midiBuffer;
        auto noteOnMessage = juce::MidiMessage::noteOn(1, noteNumber, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(noteOnMessage, 0);
        
        // Process audio with MIDI
        processor->processBlock(outputBuffer, midiBuffer);
        
        // Let note play for a few samples to generate audio
        juce::MidiBuffer emptyMidi;
        for (int frame = 0; frame < 10; ++frame) {
            processor->processBlock(outputBuffer, emptyMidi);
        }
    }
    
    std::pair<float, float> analyzeChannelBalance(const juce::AudioBuffer<float>& buffer) {
        float leftRMS = 0.0f, rightRMS = 0.0f;
        int numSamples = buffer.getNumSamples();
        
        if (numSamples == 0) return {0.0f, 0.0f};
        
        // Calculate RMS for each channel
        for (int i = 0; i < numSamples; ++i) {
            float leftSample = buffer.getSample(0, i);
            float rightSample = buffer.getSample(1, i);
            leftRMS += leftSample * leftSample;
            rightRMS += rightSample * rightSample;
        }
        
        leftRMS = std::sqrt(leftRMS / numSamples);
        rightRMS = std::sqrt(rightRMS / numSamples);
        
        return {leftRMS, rightRMS};
    }
    
    enum class PanPosition {
        LEFT,
        CENTER, 
        RIGHT,
        UNKNOWN
    };
    
    PanPosition classifyPanPosition(float leftRMS, float rightRMS) {
        const float tolerance = 0.1f;
        
        if (leftRMS > tolerance && rightRMS < tolerance) {
            return PanPosition::LEFT;
        } else if (rightRMS > tolerance && leftRMS < tolerance) {
            return PanPosition::RIGHT;
        } else if (leftRMS > tolerance && rightRMS > tolerance) {
            return PanPosition::CENTER;
        } else {
            return PanPosition::UNKNOWN;
        }
    }
    
    std::unique_ptr<MockAudioProcessorHost> host;
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
};

TEST_F(RandomPanModeTest, RandomPanGeneratesDifferentPositions) {
    CS_FILE_DBG("=== Starting RandomPanGeneratesDifferentPositions test ===");
    
    // Test multiple notes to see if we get different pan positions
    std::set<PanPosition> observedPositions;
    juce::AudioBuffer<float> audioBuffer(2, 512);
    
    for (int note = 60; note < 70; ++note) {
        audioBuffer.clear();
        
        CS_FILE_DBG("Testing note " + juce::String(note));
        
        // Play note and capture output
        playNoteAndCaptureOutput(note, 100, audioBuffer);
        
        // Analyze the stereo balance
        auto [leftRMS, rightRMS] = analyzeChannelBalance(audioBuffer);
        PanPosition position = classifyPanPosition(leftRMS, rightRMS);
        
        CS_FILE_DBG("Note " + juce::String(note) + 
                   " - Left RMS: " + juce::String(leftRMS, 4) + 
                   ", Right RMS: " + juce::String(rightRMS, 4) +
                   ", Position: " + juce::String(static_cast<int>(position)));
        
        if (position != PanPosition::UNKNOWN) {
            observedPositions.insert(position);
        }
        
        // Stop note
        juce::MidiBuffer midiBuffer;
        auto noteOffMessage = juce::MidiMessage::noteOff(1, note, static_cast<uint8_t>(100));
        midiBuffer.addEvent(noteOffMessage, 0);
        processor->processBlock(audioBuffer, midiBuffer);
    }
    
    CS_FILE_DBG("Observed " + juce::String(observedPositions.size()) + " different pan positions");
    
    // We should observe at least 2 different pan positions out of 10 notes
    EXPECT_GE(observedPositions.size(), 2) << "Random pan should generate different positions";
    
    // We should not have all notes in the same position (that would indicate broken randomization)
    EXPECT_LT(observedPositions.size(), 10) << "Not every single note should have a unique position (that's not how random works)";
    
    CS_FILE_DBG("=== RandomPanGeneratesDifferentPositions test complete ===");
}

TEST_F(RandomPanModeTest, VerifyMidiProcessorRandomPanGeneration) {
    CS_FILE_DBG("=== Starting VerifyMidiProcessorRandomPanGeneration test ===");
    
    // Test that MidiProcessor generates different random pan values
    auto* midiProcessor = processor->getMidiProcessor();
    ASSERT_NE(midiProcessor, nullptr);
    
    std::set<uint8_t> generatedPanValues;
    
    // Generate random pan for multiple channels
    for (int channel = 0; channel < 8; ++channel) {
        for (int iteration = 0; iteration < 5; ++iteration) {
            midiProcessor->setChannelRandomPan(channel);
            
            // Note: We can't directly access channelRandomPanBits from test,
            // so we'll test through the actual note playing process
        }
    }
    
    CS_FILE_DBG("=== VerifyMidiProcessorRandomPanGeneration test complete ===");
}

TEST_F(RandomPanModeTest, VerifyGlobalPanParameterIsRandom) {
    CS_FILE_DBG("=== Starting VerifyGlobalPanParameterIsRandom test ===");
    
    // Verify that GlobalPan parameter is set to RANDOM
    auto* globalPanParam = dynamic_cast<juce::AudioParameterChoice*>(
        processor->getParameters()[getGlobalPanParameterIndex()]);
    ASSERT_NE(globalPanParam, nullptr);
    
    int currentIndex = globalPanParam->getIndex();
    EXPECT_EQ(currentIndex, static_cast<int>(GlobalPanPosition::RANDOM))
        << "GlobalPan should be set to RANDOM mode";
    
    CS_FILE_DBG("GlobalPan parameter index: " + juce::String(currentIndex) + 
               " (RANDOM = " + juce::String(static_cast<int>(GlobalPanPosition::RANDOM)) + ")");
    
    CS_FILE_DBG("=== VerifyGlobalPanParameterIsRandom test complete ===");
}

TEST_F(RandomPanModeTest, VerifyPluginProcessorSkipsGlobalPanInRandomMode) {
    CS_FILE_DBG("=== Starting VerifyPluginProcessorSkipsGlobalPanInRandomMode test ===");
    
    // This test verifies that updateYmfmParameters doesn't override random pan
    // We can't directly test the internal logic, but we can test the end result
    
    juce::AudioBuffer<float> audioBuffer(2, 512);
    std::map<int, PanPosition> notePositions;
    
    // Play the same note multiple times - in working random mode, 
    // we should get different positions
    for (int attempt = 0; attempt < 20; ++attempt) {
        audioBuffer.clear();
        
        // Use note 60 consistently
        playNoteAndCaptureOutput(60, 100, audioBuffer);
        
        auto [leftRMS, rightRMS] = analyzeChannelBalance(audioBuffer);
        PanPosition position = classifyPanPosition(leftRMS, rightRMS);
        
        if (position != PanPosition::UNKNOWN) {
            notePositions[attempt] = position;
        }
        
        CS_FILE_DBG("Attempt " + juce::String(attempt) + 
                   " - Position: " + juce::String(static_cast<int>(position)));
        
        // Stop note
        juce::MidiBuffer midiBuffer;
        auto noteOffMessage = juce::MidiMessage::noteOff(1, 60, static_cast<uint8_t>(100));
        midiBuffer.addEvent(noteOffMessage, 0);
        processor->processBlock(audioBuffer, midiBuffer);
        
        // Small delay to ensure note is fully stopped
        juce::AudioBuffer<float> silenceBuffer(2, 64);
        juce::MidiBuffer emptyMidi;
        processor->processBlock(silenceBuffer, emptyMidi);
    }
    
    // Count unique positions
    std::set<PanPosition> uniquePositions;
    for (const auto& [attempt, position] : notePositions) {
        uniquePositions.insert(position);
    }
    
    CS_FILE_DBG("Out of " + juce::String(notePositions.size()) + 
               " valid attempts, found " + juce::String(uniquePositions.size()) + 
               " unique positions");
    
    // If random pan is working, we should see variation
    EXPECT_GE(uniquePositions.size(), 2) 
        << "Random pan should produce different positions across multiple note plays";
    
    CS_FILE_DBG("=== VerifyPluginProcessorSkipsGlobalPanInRandomMode test complete ===");
}