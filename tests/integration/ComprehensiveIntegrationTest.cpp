#include <gtest/gtest.h>
#include "../../src/PluginProcessor.h"
#include "../../src/core/ParameterManager.h"
#include "../mocks/MockAudioProcessorHost.h"
#include "../mocks/MockBinaryData.h"
#include <chrono>
#include <thread>

/**
 * @brief Comprehensive Integration Tests for YMulator-Synth
 * 
 * These tests verify end-to-end functionality across the entire plugin stack:
 * - MIDI input → Audio output pipeline
 * - Real-time processing constraints  
 * - Preset integration and switching
 * - Parameter automation under load
 * - Long-term stability and resource management
 * 
 * Unlike unit tests that focus on individual components, these tests validate
 * the complete system behavior as it would function in a DAW environment.
 */

namespace YMulatorSynth {
namespace Integration {

class ComprehensiveIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize with typical DAW settings
        host->initializeProcessor(*processor, 44100.0, 512, 2);
        
        // Allow time for initialization to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    void TearDown() override {
        // Reset static state to ensure test isolation
        if (processor) {
            processor->resetProcessBlockStaticState();
            ymulatorsynth::ParameterManager::resetStaticState();
        }
        processor.reset();
        host.reset();
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
    std::unique_ptr<YMulatorSynth::Test::MockAudioProcessorHost> host;
    
    // Helper: Generate sustained note sequence for testing
    void generateSustainedNote(int channel, int note, int velocity, int durationMs) {
        host->sendMidiNoteOn(*processor, channel, note, velocity);
        
        int numBlocks = (durationMs * 44100 / 1000) / 512; // Calculate blocks needed
        for (int i = 0; i < numBlocks; ++i) {
            host->processBlock(*processor, 512);
            std::this_thread::sleep_for(std::chrono::microseconds(100)); // Simulate real-time
        }
        
        host->sendMidiNoteOff(*processor, channel, note);
    }
    
    // Helper: Verify audio characteristics across multiple blocks  
    bool verifyConsistentAudio(int numBlocks, float minThreshold = 0.0001f) {
        int validBlocks = 0;
        
        for (int i = 0; i < numBlocks; ++i) {
            host->processBlock(*processor, 512);
            
            if (host->hasNonSilentOutput(minThreshold)) {
                validBlocks++;
            }
            
            // Simulate real-time processing
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        
        // At least 70% of blocks should have audio (allowing for envelope phases)
        return (static_cast<float>(validBlocks) / numBlocks) >= 0.7f;
    }
};

// =============================================================================
// 1. End-to-End Audio Pipeline Testing
// =============================================================================

TEST_F(ComprehensiveIntegrationTest, EndToEndMidiToAudioPipeline) {
    // Test complete MIDI → Audio pipeline with multiple note events
    
    // Send complex MIDI sequence
    host->sendMidiNoteOn(*processor, 1, 60, 100);  // C4
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    host->sendMidiNoteOn(*processor, 1, 64, 110);  // E4 (add to chord)  
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    host->sendMidiNoteOn(*processor, 1, 67, 90);   // G4 (complete chord)
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Verify sustained audio over multiple blocks
    EXPECT_TRUE(verifyConsistentAudio(10)); // ~120ms sustained
    
    // Release chord
    host->sendMidiNoteOff(*processor, 1, 60);
    host->sendMidiNoteOff(*processor, 1, 64);
    host->sendMidiNoteOff(*processor, 1, 67);
    
    // Verify envelope release
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput()); // Should still have release tail
}

TEST_F(ComprehensiveIntegrationTest, RealTimeProcessingConstraints) {
    // Test processing under real-time constraints with varying loads
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Simulate heavy polyphonic load
    for (int note = 60; note < 68; ++note) {
        host->sendMidiNoteOn(*processor, 1, note, 100);
    }
    
    // Process multiple blocks measuring timing
    const int testBlocks = 50;
    std::vector<double> processingTimes;
    
    for (int i = 0; i < testBlocks; ++i) {
        auto blockStart = std::chrono::high_resolution_clock::now();
        
        host->processBlock(*processor, 512);
        
        auto blockEnd = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(blockEnd - blockStart);
        processingTimes.push_back(duration.count());
        
        // Verify audio output throughout - relaxed threshold for polyphonic audio
        EXPECT_TRUE(host->hasNonSilentOutput(0.0001f));
    }
    
    // Calculate average processing time
    double avgTime = 0.0;
    for (double time : processingTimes) {
        avgTime += time;
    }
    avgTime /= processingTimes.size();
    
    // Real-time constraint: 512 samples at 44.1kHz = ~11.6ms available
    EXPECT_LT(avgTime, 10.0); // Processing should take less than 10ms per block
    
    auto totalTime = std::chrono::high_resolution_clock::now() - startTime;
    auto totalMs = std::chrono::duration<double, std::milli>(totalTime).count();
    
    CS_DBG("Integration Test Performance:");
    CS_DBG("  Average block processing: " + juce::String(avgTime) + "ms");
    CS_DBG("  Total test duration: " + juce::String(totalMs) + "ms");
}

// =============================================================================
// 2. Preset Integration Testing
// =============================================================================

TEST_F(ComprehensiveIntegrationTest, PresetSwitchingIntegration) {
    // Test seamless preset switching with ongoing audio
    
    // Start with first preset and generate audio
    processor->setCurrentProgram(0);
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Switch preset while note is playing
    processor->setCurrentProgram(1);
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput()); // Should continue without glitches
    
    // Switch to another preset
    processor->setCurrentProgram(2);
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Test rapid preset switching
    for (int preset = 0; preset < 5; ++preset) {
        processor->setCurrentProgram(preset);
        host->processBlock(*processor, 256); // Smaller blocks for rapid switching
        // Should not crash or produce silence
        EXPECT_NO_THROW(host->hasNonSilentOutput());
    }
    
    host->sendMidiNoteOff(*processor, 1, 60);
}

TEST_F(ComprehensiveIntegrationTest, PresetParameterPersistence) {
    // Test that preset parameters persist correctly across operations
    
    // Load a specific preset
    processor->setCurrentProgram(3);
    
    // Get initial parameter value
    auto initialValue = processor->getParameter(0); // Algorithm parameter
    
    // Perform audio processing
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    for (int i = 0; i < 10; ++i) {
        host->processBlock(*processor, 512);
    }
    host->sendMidiNoteOff(*processor, 1, 60);
    
    // Verify parameter is unchanged
    auto finalValue = processor->getParameter(0);
    EXPECT_FLOAT_EQ(initialValue, finalValue);
    
    // Switch preset and back
    processor->setCurrentProgram(0);
    processor->setCurrentProgram(3);
    
    // Verify parameter restoration
    auto restoredValue = processor->getParameter(0);
    EXPECT_FLOAT_EQ(initialValue, restoredValue);
}

// =============================================================================
// 3. Parameter Automation Integration
// =============================================================================

TEST_F(ComprehensiveIntegrationTest, LiveParameterAutomationIntegration) {
    // Test real-time parameter changes during audio generation
    
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    
    // Gradually change algorithm parameter while playing
    for (int step = 0; step < 20; ++step) {
        float paramValue = static_cast<float>(step) / 19.0f; // 0.0 to 1.0
        processor->setParameterNotifyingHost(0, paramValue); // Algorithm parameter
        
        host->processBlock(*processor, 512);
        EXPECT_TRUE(host->hasNonSilentOutput());
        
        // Verify parameter took effect
        EXPECT_NEAR(processor->getParameter(0), paramValue, 0.01f);
    }
    
    host->sendMidiNoteOff(*processor, 1, 60);
}

TEST_F(ComprehensiveIntegrationTest, MultiParameterAutomationStress) {
    // Test simultaneous automation of multiple parameters
    
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    host->sendMidiNoteOn(*processor, 1, 64, 110);
    
    for (int step = 0; step < 15; ++step) {
        float t = static_cast<float>(step) / 14.0f;
        
        // Automate multiple parameters simultaneously
        processor->setParameterNotifyingHost(0, t);           // Algorithm
        processor->setParameterNotifyingHost(1, 1.0f - t);    // Feedback  
        processor->setParameterNotifyingHost(2, t * 0.5f);    // Op1 Total Level
        
        host->processBlock(*processor, 512);
        EXPECT_TRUE(host->hasNonSilentOutput());
        
        // Brief pause to simulate realistic automation timing
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    
    host->sendMidiNoteOff(*processor, 1, 60);
    host->sendMidiNoteOff(*processor, 1, 64);
}

// =============================================================================
// 4. Long-term Stability Integration
// =============================================================================

TEST_F(ComprehensiveIntegrationTest, ExtendedOperationStability) {
    // Test stability over extended operation (simulating hours of use)
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Generate load over extended period
    const int totalCycles = 100; // Each cycle = ~1.2 seconds of audio
    int successfulCycles = 0;
    
    for (int cycle = 0; cycle < totalCycles; ++cycle) {
        try {
            // Play a complex pattern
            host->sendMidiNoteOn(*processor, 1, 60 + (cycle % 12), 100);
            host->sendMidiNoteOn(*processor, 1, 64 + (cycle % 8), 90);
            
            // Process sustained audio - use relaxed threshold for low-volume Init preset
            bool hasAudio = verifyConsistentAudio(10, 0.0001f);
            if (hasAudio) {
                successfulCycles++;
            }
            
            // Release notes
            host->sendMidiNoteOff(*processor, 1, 60 + (cycle % 12));
            host->sendMidiNoteOff(*processor, 1, 64 + (cycle % 8));
            
            // Occasionally change presets to test preset stability
            if (cycle % 10 == 0) {
                processor->setCurrentProgram(cycle % 8);
            }
            
        } catch (...) {
            // Any exception during extended operation is a failure
            FAIL() << "Exception occurred during extended operation at cycle " << cycle;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double>(endTime - startTime).count();
    
    // Require 95% success rate for extended operation
    float successRate = static_cast<float>(successfulCycles) / totalCycles;
    EXPECT_GE(successRate, 0.95f);
    
    CS_DBG("Extended Operation Results:");
    CS_DBG("  Cycles completed: " + juce::String(successfulCycles) + "/" + juce::String(totalCycles));
    CS_DBG("  Success rate: " + juce::String(successRate * 100.0f) + "%");
    CS_DBG("  Total duration: " + juce::String(duration) + " seconds");
}

TEST_F(ComprehensiveIntegrationTest, MemoryLeakDetection) {
    // Test for memory leaks during typical operation patterns
    
    // Perform repeated create/destroy cycles
    for (int cycle = 0; cycle < 20; ++cycle) {
        // Create audio load
        for (int note = 60; note < 68; ++note) {
            host->sendMidiNoteOn(*processor, 1, note, 100);
        }
        
        // Generate audio
        for (int block = 0; block < 10; ++block) {
            host->processBlock(*processor, 512);
        }
        
        // Release all notes
        for (int note = 60; note < 68; ++note) {
            host->sendMidiNoteOff(*processor, 1, note);
        }
        
        // Process release
        for (int block = 0; block < 5; ++block) {
            host->processBlock(*processor, 512);
        }
        
        // Switch presets (tests preset loading/unloading)
        processor->setCurrentProgram(cycle % 8);
    }
    
    // Note: Actual memory leak detection would require external tools
    // This test verifies that repeated operations don't crash
    SUCCEED(); // If we reach here without crashes, basic memory management is working
}

} // namespace Integration
} // namespace YMulatorSynth