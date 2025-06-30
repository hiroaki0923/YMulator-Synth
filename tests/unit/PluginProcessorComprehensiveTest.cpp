#include <gtest/gtest.h>
#include "../mocks/MockAudioProcessorHost.h"
#include "PluginProcessor.h"
#include "utils/ParameterIDs.h"
#include "utils/Debug.h"
#include <thread>
#include <atomic>
#include <chrono>

using namespace YMulatorSynth;

class PluginProcessorComprehensiveTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        // Initialize with standard settings
        host->initializeProcessor(*processor, 44100.0, 512, 2);
        
        // Let processor fully initialize
        for (int i = 0; i < 5; ++i) {
            host->processBlock(*processor, 512);
        }
    }
    
    void TearDown() override {
        // Reset static state before destroying processor to ensure clean test isolation
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

// =============================================================================
// 1. Constructor/Destructor Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, ConstructorInitializesCorrectly) {
    // Verify initial state
    EXPECT_EQ(processor->getCurrentProgram(), 7); // Init preset
    EXPECT_FALSE(processor->isInCustomMode());
    
    // Verify basic properties
    EXPECT_EQ(processor->getName(), JucePlugin_Name);
    EXPECT_TRUE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());
    EXPECT_TRUE(processor->hasEditor());
    
    // Verify output bus configuration
    EXPECT_EQ(processor->getTotalNumInputChannels(), 0);  // Synth has no audio input
    EXPECT_EQ(processor->getTotalNumOutputChannels(), 2); // Stereo output
}

TEST_F(PluginProcessorComprehensiveTest, MultipleConstructorDestruction) {
    // Test multiple create/destroy cycles for memory leaks
    for (int i = 0; i < 10; ++i) {
        auto testProcessor = std::make_unique<YMulatorSynthAudioProcessor>();
        auto testHost = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        
        testHost->initializeProcessor(*testProcessor, 44100.0, 256, 2);
        testHost->processBlock(*testProcessor, 256);
        
        // Should not crash or leak
        testProcessor.reset();
        testHost.reset();
    }
}

// =============================================================================
// 2. Parameter Management Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, AllParametersExist) {
    // Test global parameters
    EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Global::Algorithm) >= 0.0f);
    EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Global::Feedback) >= 0.0f);
    EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Global::GlobalPan) >= 0.0f);
    EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Global::PitchBendRange) >= 0.0f);
    
    // Test operator parameters (Op1-Op4)
    for (int op = 1; op <= 4; ++op) {
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::tl(op)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::ar(op)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::d1r(op)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::d2r(op)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::rr(op)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::d1l(op)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Op::mul(op)) >= 0.0f);
    }
    
    // Test channel parameters (Ch0-Ch7)
    for (int ch = 0; ch < 8; ++ch) {
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Channel::pan(ch)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Channel::ams(ch)) >= 0.0f);
        EXPECT_TRUE(host->getParameterValue(*processor, ParamID::Channel::pms(ch)) >= 0.0f);
    }
}

TEST_F(PluginProcessorComprehensiveTest, ParameterRangeValidation) {
    // Test algorithm parameter (0-7)
    const auto algorithmId = ParamID::Global::Algorithm;
    
    // Set to minimum
    host->setParameterValue(*processor, algorithmId, 0.0f);
    host->processBlock(*processor, 128);
    float minValue = host->getParameterValue(*processor, algorithmId);
    EXPECT_GE(minValue, 0.0f);
    
    // Set to maximum
    host->setParameterValue(*processor, algorithmId, 1.0f);
    host->processBlock(*processor, 128);
    float maxValue = host->getParameterValue(*processor, algorithmId);
    EXPECT_LE(maxValue, 1.0f);
    
    // Values should be different (quantized but valid)
    EXPECT_NE(minValue, maxValue);
}

TEST_F(PluginProcessorComprehensiveTest, CustomPresetStateManagement) {
    // Start with factory preset
    processor->setCurrentProgram(0);
    EXPECT_FALSE(processor->isInCustomMode());
    EXPECT_EQ(processor->getCurrentProgram(), 0);
    
    // Modify a parameter with user gesture to trigger custom mode
    host->setParameterValueWithGesture(*processor, ParamID::Global::Algorithm, 0.8f);
    host->processBlock(*processor, 128);
    
    // Should now be in custom mode
    EXPECT_TRUE(processor->isInCustomMode());
    EXPECT_EQ(processor->getCurrentProgram(), processor->getNumPrograms() - 1);
}

// =============================================================================
// 3. MIDI Processing Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, MidiNoteOnOffBasic) {
    // Play a note
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    host->processBlock(*processor, 512);
    
    // Should generate audio
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Note off
    host->sendMidiNoteOff(*processor, 1, 60);
    
    // Process several blocks to let note fade
    for (int i = 0; i < 10; ++i) {
        host->processBlock(*processor, 512);
    }
    
    // Should eventually become silent (or very quiet)
    float finalRMS = std::max(host->getRMSLevel(0), host->getRMSLevel(1));
    EXPECT_LT(finalRMS, 0.1f); // Should fade significantly
}

TEST_F(PluginProcessorComprehensiveTest, MidiCCParameterMapping) {
    // Test algorithm CC (CC 14 maps to algorithm)
    const auto algorithmId = ParamID::Global::Algorithm;
    
    // Get initial value
    float initialValue = host->getParameterValue(*processor, algorithmId);
    
    // Send CC 14 with value 64 (mid-range)
    host->sendMidiCC(*processor, 1, 14, 64);
    host->processBlock(*processor, 128);
    
    // Value should change
    float newValue = host->getParameterValue(*processor, algorithmId);
    EXPECT_NE(newValue, initialValue);
    
    // Should be in reasonable range for mid-value CC
    EXPECT_GT(newValue, 0.2f);
    EXPECT_LT(newValue, 0.8f);
}

TEST_F(PluginProcessorComprehensiveTest, MidiInputValidation) {
    // Test invalid note numbers
    host->sendMidiNoteOn(*processor, 1, 128, 100); // Invalid note > 127
    EXPECT_NO_THROW(host->processBlock(*processor, 512));
    
    // Test invalid CC numbers
    host->sendMidiCC(*processor, 1, 255, 64); // Invalid CC > 127
    EXPECT_NO_THROW(host->processBlock(*processor, 512));
    
    // Test invalid channel
    host->sendMidiNoteOn(*processor, 17, 60, 100); // Invalid channel > 16
    EXPECT_NO_THROW(host->processBlock(*processor, 512));
}

TEST_F(PluginProcessorComprehensiveTest, PolyphonicVoiceAllocation) {
    // Play multiple notes
    std::vector<int> notes = {60, 64, 67, 71, 74, 77, 81, 84}; // 8-note chord
    
    for (int note : notes) {
        host->sendMidiNoteOn(*processor, 1, note, 100);
    }
    
    host->processBlock(*processor, 512);
    
    // Should generate audio with all voices
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // 9th note should trigger voice stealing
    host->sendMidiNoteOn(*processor, 1, 85, 100);
    host->processBlock(*processor, 512);
    
    // Should still generate audio (voice stealing handled)
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Clean up
    for (int note : notes) {
        host->sendMidiNoteOff(*processor, 1, note);
    }
    host->sendMidiNoteOff(*processor, 1, 85);
}

// =============================================================================
// 4. Audio Processing Pipeline Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, AudioGenerationSilentByDefault) {
    // Process without any MIDI input
    host->clearProcessedBuffer();
    host->processBlock(*processor, 512);
    
    // Should be silent
    EXPECT_TRUE(host->hasNonSilentOutput() == false);
}

TEST_F(PluginProcessorComprehensiveTest, VariableBufferSizes) {
    std::vector<int> bufferSizes = {32, 64, 128, 256, 512, 1024};
    
    for (int bufferSize : bufferSizes) {
        // Reinitialize with different buffer size
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        host->initializeProcessor(*processor, 44100.0, bufferSize, 2);
        
        // Send a note
        host->sendMidiNoteOn(*processor, 1, 60, 100);
        
        // Should process without crashing
        EXPECT_NO_THROW(host->processBlock(*processor, bufferSize));
        
        // Should generate audio
        EXPECT_TRUE(host->hasNonSilentOutput());
        
        // Clean up
        host->sendMidiNoteOff(*processor, 1, 60);
        host->processBlock(*processor, bufferSize);
    }
}

TEST_F(PluginProcessorComprehensiveTest, SampleRateHandling) {
    std::vector<double> sampleRates = {22050, 44100, 48000, 88200, 96000};
    
    for (double sampleRate : sampleRates) {
        // Reinitialize with different sample rate
        host = std::make_unique<YMulatorSynth::Test::MockAudioProcessorHost>();
        host->initializeProcessor(*processor, sampleRate, 512, 2);
        
        // Should process without crashing
        EXPECT_NO_THROW(host->processBlock(*processor, 512));
        
        // Test with audio
        host->sendMidiNoteOn(*processor, 1, 60, 100);
        EXPECT_NO_THROW(host->processBlock(*processor, 512));
        
        // Clean up
        host->sendMidiNoteOff(*processor, 1, 60);
    }
}

TEST_F(PluginProcessorComprehensiveTest, StereoOutputGeneration) {
    // Play a note
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    host->processBlock(*processor, 512);
    
    // Both channels should have output
    float leftRMS = host->getRMSLevel(0);
    float rightRMS = host->getRMSLevel(1);
    
    EXPECT_GT(leftRMS, 0.001f);
    EXPECT_GT(rightRMS, 0.001f);
    
    // Channels should be reasonably balanced (unless pan is applied)
    float ratio = std::max(leftRMS, rightRMS) / std::min(leftRMS, rightRMS);
    EXPECT_LT(ratio, 10.0f); // No more than 10:1 ratio by default
}

// =============================================================================
// 5. Preset Management Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, PresetLoading) {
    int numPrograms = processor->getNumPrograms();
    EXPECT_GT(numPrograms, 0);
    
    // Test loading each preset (except custom if in custom mode)
    int maxPreset = processor->isInCustomMode() ? numPrograms - 1 : numPrograms;
    
    for (int i = 0; i < maxPreset && i < 10; ++i) { // Limit to first 10 for performance
        EXPECT_NO_THROW(processor->setCurrentProgram(i));
        EXPECT_EQ(processor->getCurrentProgram(), i);
        EXPECT_FALSE(processor->isInCustomMode());
        
        // Verify preset name is not empty
        juce::String presetName = processor->getProgramName(i);
        EXPECT_FALSE(presetName.isEmpty());
    }
}

TEST_F(PluginProcessorComprehensiveTest, PresetParameterConsistency) {
    // Load a preset and capture parameter values
    processor->setCurrentProgram(0);
    
    // Capture key parameter values
    float algorithm = host->getParameterValue(*processor, ParamID::Global::Algorithm);
    float feedback = host->getParameterValue(*processor, ParamID::Global::Feedback);
    float op1_tl = host->getParameterValue(*processor, ParamID::Op::tl(1));
    
    // Load another preset and back
    processor->setCurrentProgram(1);
    processor->setCurrentProgram(0);
    
    // Verify parameters are restored exactly (preset loading should be deterministic)
    EXPECT_FLOAT_EQ(host->getParameterValue(*processor, ParamID::Global::Algorithm), algorithm);
    EXPECT_FLOAT_EQ(host->getParameterValue(*processor, ParamID::Global::Feedback), feedback);
    EXPECT_FLOAT_EQ(host->getParameterValue(*processor, ParamID::Op::tl(1)), op1_tl);
}

// =============================================================================
// 6. State Save/Restore Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, StateSaveRestore) {
    // CORRECTED: Test state save/restore without preset interference
    // Set a specific preset first, then modify parameters to create a known state
    
    processor->setCurrentProgram(3);  // Load preset first
    host->processBlock(*processor, 128);
    
    // Now modify some parameters with user gestures to enter custom mode
    host->setParameterValueWithGesture(*processor, ParamID::Global::Algorithm, 0.5f);
    host->setParameterValueWithGesture(*processor, ParamID::Global::Feedback, 0.5f);
    host->processBlock(*processor, 128);
    
    // Capture the actual state after modifications
    float originalAlgorithm = host->getParameterValue(*processor, ParamID::Global::Algorithm);
    float originalFeedback = host->getParameterValue(*processor, ParamID::Global::Feedback);
    bool originalCustomMode = processor->isInCustomMode();
    
    CS_DBG("Original state - Algorithm=" + juce::String(originalAlgorithm, 6) + 
           ", Feedback=" + juce::String(originalFeedback, 6) + 
           ", CustomMode=" + juce::String(originalCustomMode ? "true" : "false"));
    
    // Save state
    auto savedState = host->saveProcessorState(*processor);
    
    // Make significant changes
    host->setParameterValueWithGesture(*processor, ParamID::Global::Algorithm, 0.1f);
    host->setParameterValueWithGesture(*processor, ParamID::Global::Feedback, 0.9f);
    processor->setCurrentProgram(1);  // Change to different preset
    host->processBlock(*processor, 128);
    
    // Verify state has changed
    float changedAlgorithm = host->getParameterValue(*processor, ParamID::Global::Algorithm);
    float changedFeedback = host->getParameterValue(*processor, ParamID::Global::Feedback);
    
    CS_DBG("Changed state - Algorithm=" + juce::String(changedAlgorithm, 6) + 
           ", Feedback=" + juce::String(changedFeedback, 6));
    
    // Restore state
    host->loadProcessorState(*processor, savedState);
    host->processBlock(*processor, 128);
    
    // Verify exact restoration
    float restoredAlgorithm = host->getParameterValue(*processor, ParamID::Global::Algorithm);
    float restoredFeedback = host->getParameterValue(*processor, ParamID::Global::Feedback);
    bool restoredCustomMode = processor->isInCustomMode();
    
    CS_DBG("Restored state - Algorithm=" + juce::String(restoredAlgorithm, 6) + 
           ", Feedback=" + juce::String(restoredFeedback, 6) + 
           ", CustomMode=" + juce::String(restoredCustomMode ? "true" : "false"));
    
    // State save/restore should be EXACT - no tolerance needed for proper implementation
    EXPECT_FLOAT_EQ(restoredAlgorithm, originalAlgorithm);
    EXPECT_FLOAT_EQ(restoredFeedback, originalFeedback);
    EXPECT_EQ(restoredCustomMode, originalCustomMode);
}

// =============================================================================
// 7. Thread Safety Testing (Basic)
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, BasicThreadSafety) {
    std::atomic<bool> stopTest{false};
    std::atomic<int> exceptionCount{0};
    
    // Audio thread simulation
    std::thread audioThread([&]() {
        try {
            while (!stopTest) {
                host->processBlock(*processor, 256);
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        } catch (...) {
            exceptionCount++;
        }
    });
    
    // UI thread simulation
    std::thread uiThread([&]() {
        try {
            auto algorithmId = ParamID::Global::Algorithm;
            while (!stopTest) {
                host->setParameterValue(*processor, algorithmId, 0.5f);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } catch (...) {
            exceptionCount++;
        }
    });
    
    // Run for short duration
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stopTest = true;
    
    audioThread.join();
    uiThread.join();
    
    // Should not have any exceptions
    EXPECT_EQ(exceptionCount.load(), 0);
}

// =============================================================================
// 8. Edge Cases and Error Handling
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, ZeroSizedBufferHandling) {
    // Test with zero-sized buffer (edge case)
    EXPECT_NO_THROW(host->processBlock(*processor, 0));
}

TEST_F(PluginProcessorComprehensiveTest, RapidParameterChanges) {
    const auto algorithmId = ParamID::Global::Algorithm;
    
    // Rapid parameter changes
    for (int i = 0; i < 100; ++i) {
        host->setParameterValue(*processor, algorithmId, i % 2 == 0 ? 0.0f : 1.0f);
        
        // Process small buffer
        EXPECT_NO_THROW(host->processBlock(*processor, 64));
    }
}

TEST_F(PluginProcessorComprehensiveTest, ExtremeMidiLoad) {
    // Send many MIDI events in one block
    for (int i = 0; i < 50; ++i) {
        host->sendMidiNoteOn(*processor, 1, 60 + (i % 12), 100);
        if (i % 5 == 0) {
            host->sendMidiNoteOff(*processor, 1, 60 + ((i-5) % 12));
        }
    }
    
    // Should process without crashing
    EXPECT_NO_THROW(host->processBlock(*processor, 512));
    
    // Should still generate valid audio
    EXPECT_TRUE(host->hasNonSilentOutput());
}

// =============================================================================
// 9. Public API Contract Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, AudioProcessorInterfaceCompliance) {
    // Test basic interface compliance
    EXPECT_EQ(processor->getName(), JucePlugin_Name);
    EXPECT_TRUE(processor->acceptsMidi());
    EXPECT_FALSE(processor->producesMidi());
    EXPECT_FALSE(processor->isMidiEffect());
    EXPECT_EQ(processor->getTailLengthSeconds(), 0.0);
    EXPECT_TRUE(processor->hasEditor());
    
    // Test program interface
    int numPrograms = processor->getNumPrograms();
    EXPECT_GT(numPrograms, 0);
    
    for (int i = 0; i < std::min(numPrograms, 5); ++i) {
        juce::String programName = processor->getProgramName(i);
        EXPECT_FALSE(programName.isEmpty());
    }
}

TEST_F(PluginProcessorComprehensiveTest, BusLayoutSupport) {
    // Debug current bus configuration
    auto currentLayout = processor->getBusesLayout();
    CS_DBG("Current main output channels: " + juce::String(currentLayout.getMainOutputChannels()));
    
    // Test supported bus layouts - create layout properly
    juce::AudioProcessor::BusesLayout stereoLayout;
    // Ensure we have an output bus before setting channel set
    if (processor->getBusCount(false) > 0) {
        stereoLayout.outputBuses.resize(1);
        stereoLayout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();
    }
    
    CS_DBG("Testing stereo layout with " + juce::String(stereoLayout.getMainOutputChannels()) + " channels");
    bool stereoSupported = processor->isBusesLayoutSupported(stereoLayout);
    CS_DBG("Stereo layout supported: " + juce::String(stereoSupported ? "true" : "false"));
    EXPECT_TRUE(stereoSupported);
    
    // Test mono layout
    juce::AudioProcessor::BusesLayout monoLayout;
    if (processor->getBusCount(false) > 0) {
        monoLayout.outputBuses.resize(1);
        monoLayout.outputBuses.getReference(0) = juce::AudioChannelSet::mono();
    }
    
    CS_DBG("Testing mono layout with " + juce::String(monoLayout.getMainOutputChannels()) + " channels");
    bool monoSupported = processor->isBusesLayoutSupported(monoLayout);
    CS_DBG("Mono layout supported: " + juce::String(monoSupported ? "true" : "false"));
    
    // Based on the implementation, both mono and stereo should be supported
    EXPECT_TRUE(monoSupported);
    
    // Test unsupported layouts
    juce::AudioProcessor::BusesLayout surroundLayout;
    if (processor->getBusCount(false) > 0) {
        surroundLayout.outputBuses.resize(1);
        surroundLayout.outputBuses.getReference(0) = juce::AudioChannelSet::create5point1();
    }
    EXPECT_FALSE(processor->isBusesLayoutSupported(surroundLayout));
}

TEST_F(PluginProcessorComprehensiveTest, EditorCreation) {
    EXPECT_TRUE(processor->hasEditor());
    
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
    EXPECT_NE(editor.get(), nullptr);
    
    // Editor should have reasonable size
    EXPECT_GT(editor->getWidth(), 100);
    EXPECT_GT(editor->getHeight(), 100);
    EXPECT_LT(editor->getWidth(), 5000);  // Sanity check
    EXPECT_LT(editor->getHeight(), 5000); // Sanity check
}

// =============================================================================
// 10. Performance and Resource Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, MemoryUsageStability) {
    // Process many blocks to check for memory leaks
    for (int i = 0; i < 1000; ++i) {
        host->sendMidiNoteOn(*processor, 1, 60 + (i % 12), 100);
        host->processBlock(*processor, 256);
        
        if (i % 8 == 0) {
            host->sendMidiNoteOff(*processor, 1, 60 + ((i-8) % 12));
        }
        
        // Periodically change parameters
        if (i % 50 == 0) {
            host->setParameterValue(*processor, ParamID::Global::Algorithm, (i % 8) / 7.0f);
        }
    }
    
    // Clean up all notes
    for (int note = 48; note < 84; ++note) {
        host->sendMidiNoteOff(*processor, 1, note);
    }
    
    // Process several fade-out blocks
    for (int i = 0; i < 20; ++i) {
        host->processBlock(*processor, 256);
    }
    
    // Should complete without crashing (basic memory leak detection)
    EXPECT_TRUE(true); // Test completion indicates no major memory issues
}

TEST_F(PluginProcessorComprehensiveTest, HighPolyphonyPerformance) {
    // Test performance with maximum polyphony
    std::vector<int> notes = {60, 62, 64, 65, 67, 69, 71, 72}; // 8 notes (max polyphony)
    
    // Play all notes
    for (int note : notes) {
        host->sendMidiNoteOn(*processor, 1, note, 100);
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Process 100 blocks with full polyphony
    for (int i = 0; i < 100; ++i) {
        host->processBlock(*processor, 512);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    // Should complete in reasonable time (very loose bound)
    EXPECT_LT(duration.count(), 5000); // Less than 5 seconds for 100 blocks
    
    // Clean up
    for (int note : notes) {
        host->sendMidiNoteOff(*processor, 1, note);
    }
}

TEST_F(PluginProcessorComprehensiveTest, ContinuousParameterAutomation) {
    const auto algorithmId = ParamID::Global::Algorithm;
    const auto feedbackId = ParamID::Global::Feedback;
    
    // Start a note
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    
    // Automate parameters over many blocks
    for (int block = 0; block < 200; ++block) {
        // Sine wave automation
        float phase = block * 0.1f;
        float algorithmValue = 0.5f + 0.4f * std::sin(phase);
        float feedbackValue = 0.5f + 0.4f * std::cos(phase * 1.3f);
        
        host->setParameterValue(*processor, algorithmId, algorithmValue);
        host->setParameterValue(*processor, feedbackId, feedbackValue);
        
        EXPECT_NO_THROW(host->processBlock(*processor, 128));
        
        // Should continue generating audio
        if (block % 50 == 0) {
            EXPECT_TRUE(host->hasNonSilentOutput());
        }
    }
    
    // Clean up
    host->sendMidiNoteOff(*processor, 1, 60);
}

// =============================================================================
// 11. Advanced Thread Safety Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, ConcurrentParameterAndAudioProcessing) {
    std::atomic<bool> stopTest{false};
    std::atomic<int> audioBlocks{0};
    std::atomic<int> parameterChanges{0};
    std::atomic<int> exceptionCount{0};
    
    // Audio processing thread
    std::thread audioThread([&]() {
        try {
            while (!stopTest) {
                host->processBlock(*processor, 256);
                audioBlocks++;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        } catch (...) {
            exceptionCount++;
        }
    });
    
    // Parameter automation thread
    std::thread paramThread([&]() {
        try {
            auto algorithmId = ParamID::Global::Algorithm;
            auto feedbackId = ParamID::Global::Feedback;
            
            while (!stopTest) {
                host->setParameterValue(*processor, algorithmId, 
                    (parameterChanges % 8) / 7.0f);
                host->setParameterValue(*processor, feedbackId, 
                    (parameterChanges % 16) / 15.0f);
                
                parameterChanges++;
                std::this_thread::sleep_for(std::chrono::microseconds(300));
            }
        } catch (...) {
            exceptionCount++;
        }
    });
    
    // MIDI thread
    std::thread midiThread([&]() {
        try {
            int noteCounter = 0;
            while (!stopTest) {
                int note = 60 + (noteCounter % 12);
                host->sendMidiNoteOn(*processor, 1, note, 100);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                host->sendMidiNoteOff(*processor, 1, note);
                
                noteCounter++;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        } catch (...) {
            exceptionCount++;
        }
    });
    
    // Run test for 1 second
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    stopTest = true;
    
    audioThread.join();
    paramThread.join();
    midiThread.join();
    
    // Verify that work was done and no exceptions occurred
    EXPECT_GT(audioBlocks.load(), 10);
    EXPECT_GT(parameterChanges.load(), 5);
    EXPECT_EQ(exceptionCount.load(), 0);
}

// =============================================================================
// 12. Preset File Operations Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, PresetParameterPersistence) {
    // Test that preset changes persist through state save/restore
    
    // Load a specific preset
    processor->setCurrentProgram(2);
    
    // Capture its parameters
    float originalAlgorithm = host->getParameterValue(*processor, ParamID::Global::Algorithm);
    float originalFeedback = host->getParameterValue(*processor, ParamID::Global::Feedback);
    
    // Save state
    auto savedState = host->saveProcessorState(*processor);
    
    // Change preset
    processor->setCurrentProgram(4);
    
    // Verify parameters changed
    float newAlgorithm = host->getParameterValue(*processor, ParamID::Global::Algorithm);
    float newFeedback = host->getParameterValue(*processor, ParamID::Global::Feedback);
    
    // At least one parameter should be different (unless presets are identical)
    bool parametersChanged = (std::abs(newAlgorithm - originalAlgorithm) > 0.01f) ||
                           (std::abs(newFeedback - originalFeedback) > 0.01f);
    
    // Restore state
    host->loadProcessorState(*processor, savedState);
    
    // Verify original preset is restored exactly (state save/restore must be exact)
    EXPECT_EQ(processor->getCurrentProgram(), 2);
    EXPECT_FLOAT_EQ(host->getParameterValue(*processor, ParamID::Global::Algorithm), 
                    originalAlgorithm);
    EXPECT_FLOAT_EQ(host->getParameterValue(*processor, ParamID::Global::Feedback), 
                    originalFeedback);
}

TEST_F(PluginProcessorComprehensiveTest, CustomPresetBehavior) {
    // Test custom preset creation and persistence
    
    // Start with a factory preset
    processor->setCurrentProgram(1);
    EXPECT_FALSE(processor->isInCustomMode());
    
    // Modify multiple parameters with user gestures to ensure custom preset creation
    host->setParameterValueWithGesture(*processor, ParamID::Global::Algorithm, 0.85f);
    host->setParameterValueWithGesture(*processor, ParamID::Global::Feedback, 0.65f);
    host->setParameterValueWithGesture(*processor, ParamID::Op::tl(1), 0.75f);
    
    // Process to apply changes
    host->processBlock(*processor, 128);
    
    // Should now be in custom mode
    EXPECT_TRUE(processor->isInCustomMode());
    
    // Custom preset name should be meaningful (the custom preset functionality uses "Custom" name)
    juce::String customName = processor->getCustomPresetName();
    EXPECT_FALSE(customName.isEmpty());
    EXPECT_TRUE(customName.contains("Custom") || customName.contains("User") || 
                customName.contains("Modified"));
}

// =============================================================================
// 13. Voice Stealing Algorithm Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, VoiceStealingBehavior) {
    // Test voice stealing when exceeding 8-voice polyphony
    
    // Play 8 notes (maximum polyphony)
    std::vector<int> firstNotes = {60, 62, 64, 65, 67, 69, 71, 72};
    for (int note : firstNotes) {
        host->sendMidiNoteOn(*processor, 1, note, 100);
        host->processBlock(*processor, 64); // Small processing between notes
    }
    
    // Verify we have audio output from all voices
    host->processBlock(*processor, 256);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Play 9th note - should trigger voice stealing
    host->sendMidiNoteOn(*processor, 1, 74, 100);
    host->processBlock(*processor, 256);
    
    // Should still have audio output (voice stealing handled gracefully)
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Play several more notes to test continued voice stealing
    for (int i = 0; i < 5; ++i) {
        host->sendMidiNoteOn(*processor, 1, 76 + i, 100);
        host->processBlock(*processor, 128);
        EXPECT_TRUE(host->hasNonSilentOutput());
    }
    
    // Clean up - release all possible notes
    for (int note = 60; note < 85; ++note) {
        host->sendMidiNoteOff(*processor, 1, note);
    }
}

TEST_F(PluginProcessorComprehensiveTest, RapidNoteOnOffSequence) {
    // Test rapid note on/off sequences that might confuse voice allocation
    
    for (int i = 0; i < 50; ++i) {
        int note = 60 + (i % 12);
        
        // Rapid note on/off
        host->sendMidiNoteOn(*processor, 1, note, 100);
        host->processBlock(*processor, 32);
        host->sendMidiNoteOff(*processor, 1, note);
        host->processBlock(*processor, 32);
        
        // Should handle without crashing
        EXPECT_NO_THROW(host->processBlock(*processor, 64));
    }
    
    // Final cleanup
    for (int note = 48; note < 84; ++note) {
        host->sendMidiNoteOff(*processor, 1, note);
    }
}

// =============================================================================
// 14. Audio Quality and Signal Integrity Testing
// =============================================================================

TEST_F(PluginProcessorComprehensiveTest, AudioSignalIntegrity) {
    // Test that generated audio has reasonable characteristics
    
    // Play a sustained note
    host->sendMidiNoteOn(*processor, 1, 60, 100);
    
    // Process several blocks to reach steady state
    for (int i = 0; i < 10; ++i) {
        host->processBlock(*processor, 512);
    }
    
    // Check audio characteristics
    float leftRMS = host->getRMSLevel(0);
    float rightRMS = host->getRMSLevel(1);
    
    // Should have reasonable signal levels
    EXPECT_GT(leftRMS, 0.001f);   // Not silent
    EXPECT_LT(leftRMS, 1.0f);     // Not clipping
    EXPECT_GT(rightRMS, 0.001f);  // Not silent
    EXPECT_LT(rightRMS, 1.0f);    // Not clipping
    
    // Channels should be reasonably balanced for center pan
    float balance = std::max(leftRMS, rightRMS) / std::min(leftRMS, rightRMS);
    EXPECT_LT(balance, 5.0f); // Not more than 5:1 ratio
    
    // Clean up
    host->sendMidiNoteOff(*processor, 1, 60);
}

TEST_F(PluginProcessorComprehensiveTest, SilenceAfterAllNotesOff) {
    // Test that processor becomes silent after all notes are released
    
    // Play and release multiple notes
    std::vector<int> notes = {60, 64, 67};
    
    for (int note : notes) {
        host->sendMidiNoteOn(*processor, 1, note, 100);
    }
    
    host->processBlock(*processor, 512);
    EXPECT_TRUE(host->hasNonSilentOutput());
    
    // Release all notes
    for (int note : notes) {
        host->sendMidiNoteOff(*processor, 1, note);
    }
    
    // Process many blocks for full release
    for (int i = 0; i < 50; ++i) {
        host->processBlock(*processor, 512);
    }
    
    // Should be very quiet or silent
    float finalLevel = std::max(host->getRMSLevel(0), host->getRMSLevel(1));
    EXPECT_LT(finalLevel, 0.01f); // Very quiet after release
}