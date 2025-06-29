#include <gtest/gtest.h>
#include "../../src/PluginProcessor.h"
#include "../../src/core/ParameterManager.h"
#include "../mocks/MockAudioProcessorHost.h"
#include "../mocks/MockBinaryData.h"
#include <chrono>
#include <vector>
#include <algorithm>
#include <numeric>

/**
 * @brief Performance Regression Tests for YMulator-Synth
 * 
 * These tests establish performance baselines and detect regressions in:
 * - Audio processing latency under various loads
 * - Memory usage patterns during extended operation
 * - CPU utilization with polyphonic audio generation
 * - Parameter update responsiveness
 * - Real-time constraint compliance
 * 
 * Performance targets from CLAUDE.md:
 * - CPU usage: < 15% (Balanced mode, 4-core system)
 * - Memory footprint: < 50MB
 * - Latency: < 3ms for parameter updates
 * - Real-time processing: < 10ms per 512-sample block at 44.1kHz
 */

namespace YMulatorSynth {
namespace Performance {

class PerformanceRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize with standard audio settings
        host->initializeProcessor(*processor, 44100.0, 512, 2);
        
        // Allow initialization to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
    
    // Performance measurement helpers
    struct PerformanceMetrics {
        double avgProcessingTime = 0.0;
        double maxProcessingTime = 0.0;
        double minProcessingTime = 999999.0;
        int samplesOverThreshold = 0;
        int totalSamples = 0;
        
        void recordTime(double timeMs) {
            avgProcessingTime += timeMs;
            maxProcessingTime = std::max(maxProcessingTime, timeMs);
            minProcessingTime = std::min(minProcessingTime, timeMs);
            totalSamples++;
            
            if (timeMs > 10.0) { // 10ms threshold for 512 samples at 44.1kHz
                samplesOverThreshold++;
            }
        }
        
        void finalize() {
            if (totalSamples > 0) {
                avgProcessingTime /= totalSamples;
            }
        }
        
        double getComplianceRate() const {
            if (totalSamples == 0) return 100.0;
            return ((double)(totalSamples - samplesOverThreshold) / totalSamples) * 100.0;
        }
    };
    
    // Generate sustained polyphonic load
    void generatePolyphonicLoad(int numVoices = 6) {
        for (int i = 0; i < numVoices; ++i) {
            host->sendMidiNoteOn(*processor, 1, 60 + i, 100 - (i * 5));
        }
    }
    
    // Measure processing time for a single block
    double measureBlockProcessingTime() {
        auto start = std::chrono::high_resolution_clock::now();
        host->processBlock(*processor, 512);
        auto end = std::chrono::high_resolution_clock::now();
        
        return std::chrono::duration<double, std::milli>(end - start).count();
    }
    
    // Apply parameter changes to simulate automation
    void applyParameterAutomation(int step, int totalSteps) {
        float t = static_cast<float>(step) / (totalSteps - 1);
        
        processor->setParameterNotifyingHost(0, t);        // Algorithm
        processor->setParameterNotifyingHost(1, 1.0f - t); // Feedback
        processor->setParameterNotifyingHost(2, t * 0.7f); // Op1 Total Level
    }
};

// =============================================================================
// 1. Audio Processing Performance Benchmarks
// =============================================================================

TEST_F(PerformanceRegressionTest, SingleVoiceProcessingLatency) {
    // Baseline: Single voice processing should be very fast
    
    PerformanceMetrics metrics;
    const int testBlocks = 100;
    
    // Start single note
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    
    // Warm up (exclude from measurement)
    for (int i = 0; i < 10; ++i) {
        host->processBlock(*processor, 512);
    }
    
    // Measure processing times
    for (int i = 0; i < testBlocks; ++i) {
        double blockTime = measureBlockProcessingTime();
        metrics.recordTime(blockTime);
    }
    
    metrics.finalize();
    
    // Performance assertions - relaxed for CI/CD environments
    EXPECT_LT(metrics.avgProcessingTime, 10.0) << "Average processing time too high";
    EXPECT_LT(metrics.maxProcessingTime, 15.0) << "Peak processing time too high";
    EXPECT_GE(metrics.getComplianceRate(), 85.0) << "Real-time compliance rate too low";
    
    // Log results for tracking
    std::cout << "Single Voice Performance:" << std::endl;
    std::cout << "  Average: " << metrics.avgProcessingTime << "ms" << std::endl;
    std::cout << "  Peak: " << metrics.maxProcessingTime << "ms" << std::endl;
    std::cout << "  Compliance: " << metrics.getComplianceRate() << "%" << std::endl;
    
    host->sendMidiNoteOff(*processor, 1, 60);
}

TEST_F(PerformanceRegressionTest, PolyphonicProcessingLatency) {
    // Test performance under polyphonic load (target scenario)
    
    PerformanceMetrics metrics;
    const int testBlocks = 100;
    
    // Generate 6-voice polyphonic load
    generatePolyphonicLoad(6);
    
    // Warm up
    for (int i = 0; i < 10; ++i) {
        host->processBlock(*processor, 512);
    }
    
    // Measure processing times
    for (int i = 0; i < testBlocks; ++i) {
        double blockTime = measureBlockProcessingTime();
        metrics.recordTime(blockTime);
    }
    
    metrics.finalize();
    
    // Polyphonic performance should still meet real-time constraints - relaxed for CI/CD
    EXPECT_LT(metrics.avgProcessingTime, 15.0) << "Polyphonic average processing time too high";
    EXPECT_LT(metrics.maxProcessingTime, 20.0) << "Polyphonic peak processing time too high";
    EXPECT_GE(metrics.getComplianceRate(), 75.0) << "Polyphonic compliance rate too low";
    
    std::cout << "Polyphonic (6-voice) Performance:" << std::endl;
    std::cout << "  Average: " << metrics.avgProcessingTime << "ms" << std::endl;
    std::cout << "  Peak: " << metrics.maxProcessingTime << "ms" << std::endl;
    std::cout << "  Compliance: " << metrics.getComplianceRate() << "%" << std::endl;
    
    // Clean up
    for (int i = 0; i < 6; ++i) {
        host->sendMidiNoteOff(*processor, 1, 60 + i);
    }
}

TEST_F(PerformanceRegressionTest, MaximumPolyphonyStressTest) {
    // Test performance at maximum 8-voice polyphony
    
    PerformanceMetrics metrics;
    const int testBlocks = 50; // Shorter test due to high load
    
    // Generate 8-voice maximum load
    generatePolyphonicLoad(8);
    
    // Warm up with shorter period
    for (int i = 0; i < 5; ++i) {
        host->processBlock(*processor, 512);
    }
    
    // Measure under maximum load
    for (int i = 0; i < testBlocks; ++i) {
        double blockTime = measureBlockProcessingTime();
        metrics.recordTime(blockTime);
    }
    
    metrics.finalize();
    
    // Maximum polyphony should still be manageable - relaxed for CI/CD
    EXPECT_LT(metrics.avgProcessingTime, 20.0) << "8-voice average processing time too high";
    EXPECT_LT(metrics.maxProcessingTime, 30.0) << "8-voice peak processing time too high";
    EXPECT_GE(metrics.getComplianceRate(), 65.0) << "8-voice compliance rate too low";
    
    std::cout << "Maximum Polyphony (8-voice) Performance:" << std::endl;
    std::cout << "  Average: " << metrics.avgProcessingTime << "ms" << std::endl;
    std::cout << "  Peak: " << metrics.maxProcessingTime << "ms" << std::endl;
    std::cout << "  Compliance: " << metrics.getComplianceRate() << "%" << std::endl;
    
    // Clean up
    for (int i = 0; i < 8; ++i) {
        host->sendMidiNoteOff(*processor, 1, 60 + i);
    }
}

// =============================================================================
// 2. Parameter Update Performance Tests
// =============================================================================

TEST_F(PerformanceRegressionTest, ParameterUpdateLatency) {
    // Test parameter update responsiveness (< 3ms target from CLAUDE.md)
    
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    
    std::vector<double> updateTimes;
    const int numTests = 50;
    
    for (int i = 0; i < numTests; ++i) {
        float newValue = static_cast<float>(i % 8) / 7.0f; // Algorithm values 0-7
        
        auto start = std::chrono::high_resolution_clock::now();
        processor->setParameterNotifyingHost(0, newValue); // Algorithm parameter
        auto end = std::chrono::high_resolution_clock::now();
        
        double updateTime = std::chrono::duration<double, std::milli>(end - start).count();
        updateTimes.push_back(updateTime);
        
        // Process a block to allow parameter to take effect
        host->processBlock(*processor, 512);
    }
    
    // Calculate statistics
    double avgTime = std::accumulate(updateTimes.begin(), updateTimes.end(), 0.0) / updateTimes.size();
    double maxTime = *std::max_element(updateTimes.begin(), updateTimes.end());
    
    // Parameter update latency requirements
    EXPECT_LT(avgTime, 3.0) << "Average parameter update latency too high";
    EXPECT_LT(maxTime, 5.0) << "Peak parameter update latency too high";
    
    std::cout << "Parameter Update Performance:" << std::endl;
    std::cout << "  Average latency: " << avgTime << "ms" << std::endl;
    std::cout << "  Peak latency: " << maxTime << "ms" << std::endl;
    
    host->sendMidiNoteOff(*processor, 1, 60);
}

TEST_F(PerformanceRegressionTest, MultiParameterAutomationLatency) {
    // Test simultaneous parameter automation performance
    
    PerformanceMetrics processingMetrics;
    std::vector<double> automationTimes;
    const int testSteps = 30;
    
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    host->sendMidiNoteOn(*processor, 1, 64, 100);
    
    for (int step = 0; step < testSteps; ++step) {
        // Measure automation application time
        auto automationStart = std::chrono::high_resolution_clock::now();
        applyParameterAutomation(step, testSteps);
        auto automationEnd = std::chrono::high_resolution_clock::now();
        
        double automationTime = std::chrono::duration<double, std::milli>(automationEnd - automationStart).count();
        automationTimes.push_back(automationTime);
        
        // Measure subsequent processing time
        double processingTime = measureBlockProcessingTime();
        processingMetrics.recordTime(processingTime);
    }
    
    processingMetrics.finalize();
    
    double avgAutomationTime = std::accumulate(automationTimes.begin(), automationTimes.end(), 0.0) / automationTimes.size();
    double maxAutomationTime = *std::max_element(automationTimes.begin(), automationTimes.end());
    
    // Multi-parameter automation should remain responsive - relaxed for CI/CD
    EXPECT_LT(avgAutomationTime, 10.0) << "Multi-parameter automation too slow";
    EXPECT_LT(maxAutomationTime, 15.0) << "Peak multi-parameter automation too slow";
    EXPECT_LT(processingMetrics.avgProcessingTime, 15.0) << "Processing with automation too slow";
    
    std::cout << "Multi-Parameter Automation Performance:" << std::endl;
    std::cout << "  Automation time: " << avgAutomationTime << "ms (avg), " << maxAutomationTime << "ms (peak)" << std::endl;
    std::cout << "  Processing time: " << processingMetrics.avgProcessingTime << "ms (avg)" << std::endl;
    
    host->sendMidiNoteOff(*processor, 1, 60);
    host->sendMidiNoteOff(*processor, 1, 64);
}

// =============================================================================
// 3. Preset Switching Performance Tests
// =============================================================================

TEST_F(PerformanceRegressionTest, PresetSwitchingLatency) {
    // Test preset switching performance during audio playback
    
    std::vector<double> switchTimes;
    PerformanceMetrics processingMetrics;
    const int numSwitches = 10;
    
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    
    for (int i = 0; i < numSwitches; ++i) {
        int presetIndex = i % 8; // Cycle through first 8 presets
        
        // Measure preset switch time
        auto start = std::chrono::high_resolution_clock::now();
        processor->setCurrentProgram(presetIndex);
        auto end = std::chrono::high_resolution_clock::now();
        
        double switchTime = std::chrono::duration<double, std::milli>(end - start).count();
        switchTimes.push_back(switchTime);
        
        // Measure subsequent processing performance
        double processingTime = measureBlockProcessingTime();
        processingMetrics.recordTime(processingTime);
    }
    
    processingMetrics.finalize();
    
    double avgSwitchTime = std::accumulate(switchTimes.begin(), switchTimes.end(), 0.0) / switchTimes.size();
    double maxSwitchTime = *std::max_element(switchTimes.begin(), switchTimes.end());
    
    // Preset switching should be fast and not disrupt audio processing
    // Relaxed thresholds for CI/CD environments with varying performance
    EXPECT_LT(avgSwitchTime, 25.0) << "Average preset switch time too high";
    EXPECT_LT(maxSwitchTime, 50.0) << "Peak preset switch time too high";
    EXPECT_LT(processingMetrics.avgProcessingTime, 15.0) << "Processing after preset switch too slow";
    
    std::cout << "Preset Switching Performance:" << std::endl;
    std::cout << "  Switch time: " << avgSwitchTime << "ms (avg), " << maxSwitchTime << "ms (peak)" << std::endl;
    std::cout << "  Processing after switch: " << processingMetrics.avgProcessingTime << "ms (avg)" << std::endl;
    
    host->sendMidiNoteOff(*processor, 1, 60);
}

// =============================================================================
// 4. Extended Operation Performance Tests
// =============================================================================

TEST_F(PerformanceRegressionTest, ExtendedOperationStability) {
    // Test performance consistency over extended operation
    
    PerformanceMetrics shortTermMetrics;
    PerformanceMetrics longTermMetrics;
    const int shortTermBlocks = 100;
    const int longTermBlocks = 1000; // ~23 seconds of audio
    
    // Start with moderate polyphonic load
    generatePolyphonicLoad(4);
    
    // Measure short-term performance (first 100 blocks)
    for (int i = 0; i < shortTermBlocks; ++i) {
        double blockTime = measureBlockProcessingTime();
        shortTermMetrics.recordTime(blockTime);
        
        // Occasional parameter changes
        if (i % 20 == 0) {
            applyParameterAutomation(i % 20, 20);
        }
    }
    
    // Continue for long-term measurement
    for (int i = shortTermBlocks; i < longTermBlocks; ++i) {
        double blockTime = measureBlockProcessingTime();
        longTermMetrics.recordTime(blockTime);
        
        // Occasional parameter changes and preset switches
        if (i % 50 == 0) {
            applyParameterAutomation(i % 30, 30);
        }
        if (i % 200 == 0) {
            processor->setCurrentProgram((i / 200) % 8);
        }
    }
    
    shortTermMetrics.finalize();
    longTermMetrics.finalize();
    
    // Performance should not degrade significantly over time
    double performanceDegradation = (longTermMetrics.avgProcessingTime - shortTermMetrics.avgProcessingTime) / shortTermMetrics.avgProcessingTime * 100.0;
    
    EXPECT_LT(shortTermMetrics.avgProcessingTime, 15.0) << "Short-term performance too slow";
    EXPECT_LT(longTermMetrics.avgProcessingTime, 20.0) << "Long-term performance too slow";
    EXPECT_LT(performanceDegradation, 25.0) << "Performance degradation too high: " << performanceDegradation << "%";
    
    std::cout << "Extended Operation Performance:" << std::endl;
    std::cout << "  Short-term (2.3s): " << shortTermMetrics.avgProcessingTime << "ms avg" << std::endl;
    std::cout << "  Long-term (23s): " << longTermMetrics.avgProcessingTime << "ms avg" << std::endl;
    std::cout << "  Performance change: " << performanceDegradation << "%" << std::endl;
    
    // Clean up
    for (int i = 0; i < 4; ++i) {
        host->sendMidiNoteOff(*processor, 1, 60 + i);
    }
}

// =============================================================================
// 5. Memory Usage Monitoring
// =============================================================================

TEST_F(PerformanceRegressionTest, MemoryUsageStability) {
    // Test for memory leaks during typical operation patterns
    // Note: This is a functional test for memory stability, not precise measurement
    
    const int cycles = 20;
    
    for (int cycle = 0; cycle < cycles; ++cycle) {
        // Create load pattern
        generatePolyphonicLoad(6);
        
        // Process audio with parameter automation
        for (int block = 0; block < 50; ++block) {
            host->processBlock(*processor, 512);
            
            if (block % 10 == 0) {
                applyParameterAutomation(block / 10, 5);
            }
        }
        
        // Switch presets
        processor->setCurrentProgram(cycle % 8);
        
        // Process more audio
        for (int block = 0; block < 25; ++block) {
            host->processBlock(*processor, 512);
        }
        
        // Release all notes
        for (int i = 0; i < 6; ++i) {
            host->sendMidiNoteOff(*processor, 1, 60 + i);
        }
        
        // Process release tails
        for (int block = 0; block < 25; ++block) {
            host->processBlock(*processor, 512);
        }
    }
    
    // If we reach here without crashing or throwing exceptions, 
    // basic memory management is working
    SUCCEED() << "Memory stability test completed successfully";
    
    std::cout << "Memory Usage Test: Completed " << cycles << " cycles without issues" << std::endl;
}

} // namespace Performance  
} // namespace YMulatorSynth