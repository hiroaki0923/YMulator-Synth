#include <gtest/gtest.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../src/PluginProcessor.h"
#include "../../src/utils/Debug.h"
#include "../../src/utils/ParameterIDs.h"
#include <cmath>
#include <fstream>

/**
 * Standalone test for Unison Detune functionality
 * Tests the complete unison detune pipeline without requiring DAW
 */
class TestUnisonDetuneStandalone : public ::testing::Test {
protected:
    void SetUp() override {
        // Create the audio processor
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        
        // Initialize with test audio settings
        processor->setPlayConfigDetails(0, 2, 44100.0, 512);
        processor->prepareToPlay(44100.0, 512);
        
        // Enable debug output to file for detailed analysis
        auto debugFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                        .getChildFile("unison_detune_test_debug.txt");
        debugFile.deleteFile();
        
        CS_DBG("=== STANDALONE UNISON DETUNE TEST START ===");
    }
    
    void TearDown() override {
        CS_DBG("=== STANDALONE UNISON DETUNE TEST END ===");
        processor.reset();
    }
    
    // Helper: Set unison parameters
    void setUnisonParameters(bool enabled, int voiceCount, float detuneCents) {
        auto& params = processor->getParameters();
        
        // Enable unison
        if (auto* enableParam = params.getParameter(ParamID::Global::UnisonEnable)) {
            enableParam->setValueNotifyingHost(enabled ? 1.0f : 0.0f);
        }
        
        // Set voice count (parameter uses normalized 0-1 range)
        if (auto* voiceParam = params.getParameter(ParamID::Global::UnisonVoiceCount)) {
            float normalizedValue = (voiceCount - 1) / 7.0f;  // Convert 1-8 to 0-1
            voiceParam->setValueNotifyingHost(normalizedValue);
        }
        
        // Set detune amount (0-1200 cents, normalized to 0-1)
        if (auto* detuneParam = params.getParameter(ParamID::Global::UnisonDetune)) {
            float normalizedValue = detuneCents / 1200.0f;  // Convert cents to 0-1 normalized
            detuneParam->setValueNotifyingHost(normalizedValue);
        }
        
        CS_DBG("Set unison parameters: enabled=" + juce::String(enabled ? "true" : "false") + 
               ", voices=" + juce::String(voiceCount) + 
               ", detune=" + juce::String(detuneCents, 1) + " cents");
    }
    
    // Helper: Trigger MIDI note
    void triggerNote(int note, int velocity, int durationSamples = 2048) {
        juce::MidiBuffer midiBuffer;
        
        // Note ON
        auto noteOnMsg = juce::MidiMessage::noteOn(1, note, (uint8_t)velocity);
        midiBuffer.addEvent(noteOnMsg, 0);
        
        // Process several blocks to let the note develop
        juce::AudioBuffer<float> buffer(2, 512);
        for (int block = 0; block < (durationSamples / 512); ++block) {
            buffer.clear();
            
            if (block == 0) {
                // First block with MIDI
                processor->processBlock(buffer, midiBuffer);
            } else {
                // Subsequent blocks without MIDI
                juce::MidiBuffer emptyMidi;
                processor->processBlock(buffer, emptyMidi);
            }
        }
        
        // Note OFF
        midiBuffer.clear();
        auto noteOffMsg = juce::MidiMessage::noteOff(1, note, (uint8_t)0);
        midiBuffer.addEvent(noteOffMsg, 0);
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        CS_DBG("Triggered note " + juce::String(note) + " with velocity " + juce::String(velocity));
    }
    
    // Helper: Capture audio output with unison settings
    std::vector<float> captureAudioOutput(int note, int velocity, int voiceCount, float detuneCents) {
        setUnisonParameters(true, voiceCount, detuneCents);
        
        // Allow parameters to settle
        juce::Thread::sleep(50);
        
        juce::MidiBuffer midiBuffer;
        auto noteOnMsg = juce::MidiMessage::noteOn(1, note, (uint8_t)velocity);
        midiBuffer.addEvent(noteOnMsg, 0);
        
        std::vector<float> audioSamples;
        juce::AudioBuffer<float> buffer(2, 512);
        
        // Capture audio from multiple blocks
        for (int block = 0; block < 8; ++block) {
            buffer.clear();
            
            if (block == 0) {
                processor->processBlock(buffer, midiBuffer);
            } else {
                juce::MidiBuffer emptyMidi;
                processor->processBlock(buffer, emptyMidi);
            }
            
            // Capture left channel samples
            const float* leftChannel = buffer.getReadPointer(0);
            for (int i = 0; i < 512; ++i) {
                audioSamples.push_back(leftChannel[i]);
            }
        }
        
        // Note OFF
        midiBuffer.clear();
        auto noteOffMsg = juce::MidiMessage::noteOff(1, note, (uint8_t)0);
        midiBuffer.addEvent(noteOffMsg, 0);
        buffer.clear();
        processor->processBlock(buffer, midiBuffer);
        
        return audioSamples;
    }
    
    // Helper: Calculate RMS level of audio samples
    float calculateRMS(const std::vector<float>& samples) {
        if (samples.empty()) return 0.0f;
        
        float sum = 0.0f;
        for (float sample : samples) {
            sum += sample * sample;
        }
        return std::sqrt(sum / samples.size());
    }
    
    // Helper: Save audio samples to file for analysis
    void saveAudioToFile(const std::vector<float>& samples, const juce::String& filename) {
        auto file = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                   .getChildFile(filename);
        
        std::ofstream stream(file.getFullPathName().toStdString());
        for (size_t i = 0; i < samples.size(); ++i) {
            stream << i << "," << samples[i] << "\n";
        }
        stream.close();
        
        CS_DBG("Saved audio samples to " + filename);
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
};

// Test 1: Basic unison functionality
TEST_F(TestUnisonDetuneStandalone, BasicUnisonTest) {
    CS_DBG("=== TEST: Basic Unison Functionality ===");
    
    // Test single voice (no unison)
    auto singleVoiceSamples = captureAudioOutput(60, 100, 1, 0.0f);
    float singleVoiceRMS = calculateRMS(singleVoiceSamples);
    
    CS_DBG("Single voice RMS: " + juce::String(singleVoiceRMS, 6));
    EXPECT_GT(singleVoiceRMS, 0.001f) << "Single voice should generate audio";
    
    // Test dual voice unison (no detune)
    auto dualVoiceSamples = captureAudioOutput(60, 100, 2, 0.0f);
    float dualVoiceRMS = calculateRMS(dualVoiceSamples);
    
    CS_DBG("Dual voice RMS: " + juce::String(dualVoiceRMS, 6));
    EXPECT_GT(dualVoiceRMS, 0.001f) << "Dual voice unison should generate audio";
    
    // Save samples for analysis
    saveAudioToFile(singleVoiceSamples, "single_voice_test.csv");
    saveAudioToFile(dualVoiceSamples, "dual_voice_test.csv");
    
    CS_DBG("Basic unison test completed");
}

// Test 2: Detune functionality
TEST_F(TestUnisonDetuneStandalone, DetuneTest) {
    CS_DBG("=== TEST: Detune Functionality ===");
    
    // Test unison without detune
    auto noDetuneSamples = captureAudioOutput(60, 100, 2, 0.0f);
    float noDetuneRMS = calculateRMS(noDetuneSamples);
    
    // Test unison with detune
    auto withDetuneSamples = captureAudioOutput(60, 100, 2, 15.0f);
    float withDetuneRMS = calculateRMS(withDetuneSamples);
    
    CS_DBG("No detune RMS: " + juce::String(noDetuneRMS, 6));
    CS_DBG("With detune RMS: " + juce::String(withDetuneRMS, 6));
    
    // Both should generate audio
    EXPECT_GT(noDetuneRMS, 0.001f) << "Unison without detune should generate audio";
    EXPECT_GT(withDetuneRMS, 0.001f) << "Unison with detune should generate audio";
    
    // Save samples for frequency analysis
    saveAudioToFile(noDetuneSamples, "no_detune_test.csv");
    saveAudioToFile(withDetuneSamples, "with_detune_test.csv");
    
    CS_DBG("Detune test completed");
}

// Test 3: Multiple voice counts
TEST_F(TestUnisonDetuneStandalone, MultipleVoiceTest) {
    CS_DBG("=== TEST: Multiple Voice Counts ===");
    
    std::vector<int> voiceCounts = {1, 2, 3, 4};
    std::vector<float> rmsLevels;
    
    for (int voices : voiceCounts) {
        auto samples = captureAudioOutput(60, 100, voices, 10.0f);
        float rms = calculateRMS(samples);
        rmsLevels.push_back(rms);
        
        CS_DBG("Voice count " + juce::String(voices) + " RMS: " + juce::String(rms, 6));
        EXPECT_GT(rms, 0.001f) << "Voice count " << voices << " should generate audio";
        
        // Save each test
        saveAudioToFile(samples, "voice_count_" + juce::String(voices) + "_test.csv");
    }
    
    CS_DBG("Multiple voice test completed");
}

// Test 4: Parameter synchronization verification
TEST_F(TestUnisonDetuneStandalone, ParameterSyncTest) {
    CS_DBG("=== TEST: Parameter Synchronization ===");
    
    // Enable unison with detailed logging
    setUnisonParameters(true, 2, 20.0f);
    
    // Trigger note and check internal state
    triggerNote(60, 100, 1024);
    
    // Verify unison state through public interface
    EXPECT_TRUE(processor->isUnisonEnabled()) << "Unison should be enabled";
    
    // Test parameter changes during playback
    setUnisonParameters(true, 3, 30.0f);
    triggerNote(64, 100, 1024);
    
    setUnisonParameters(true, 4, 5.0f);
    triggerNote(67, 100, 1024);
    
    CS_DBG("Parameter synchronization test completed");
}

// Test 5: Detune range verification
TEST_F(TestUnisonDetuneStandalone, DetuneRangeTest) {
    CS_DBG("=== TEST: Detune Range Verification ===");
    
    std::vector<float> detuneValues = {0.0f, 5.0f, 10.0f, 25.0f, 50.0f};
    
    for (float detune : detuneValues) {
        auto samples = captureAudioOutput(60, 100, 2, detune);
        float rms = calculateRMS(samples);
        
        CS_DBG("Detune " + juce::String(detune, 1) + " cents RMS: " + juce::String(rms, 6));
        EXPECT_GT(rms, 0.001f) << "Detune " << detune << " cents should generate audio";
        
        saveAudioToFile(samples, "detune_" + juce::String(static_cast<int>(detune)) + "_cents.csv");
    }
    
    CS_DBG("Detune range test completed");
}

// Test 6: Real-time parameter changes
TEST_F(TestUnisonDetuneStandalone, RealTimeParameterTest) {
    CS_DBG("=== TEST: Real-time Parameter Changes ===");
    
    // Start with unison disabled
    setUnisonParameters(false, 1, 0.0f);
    
    juce::MidiBuffer midiBuffer;
    auto noteOnMsg = juce::MidiMessage::noteOn(1, 60, (uint8_t)100);
    midiBuffer.addEvent(noteOnMsg, 0);
    
    juce::AudioBuffer<float> buffer(2, 512);
    std::vector<float> audioSamples;
    
    // Process while changing parameters in real-time
    for (int block = 0; block < 16; ++block) {
        buffer.clear();
        
        if (block == 0) {
            processor->processBlock(buffer, midiBuffer);
        } else {
            juce::MidiBuffer emptyMidi;
            processor->processBlock(buffer, emptyMidi);
        }
        
        // Change parameters during playback
        if (block == 4) {
            setUnisonParameters(true, 2, 15.0f);
            CS_DBG("Enabled unison during playback");
        } else if (block == 8) {
            setUnisonParameters(true, 3, 25.0f);
            CS_DBG("Changed to 3 voices with more detune");
        } else if (block == 12) {
            setUnisonParameters(true, 2, 5.0f);
            CS_DBG("Reduced to 2 voices with less detune");
        }
        
        // Capture samples
        const float* leftChannel = buffer.getReadPointer(0);
        for (int i = 0; i < 512; ++i) {
            audioSamples.push_back(leftChannel[i]);
        }
    }
    
    // Note OFF
    midiBuffer.clear();
    auto noteOffMsg = juce::MidiMessage::noteOff(1, 60, (uint8_t)0);
    midiBuffer.addEvent(noteOffMsg, 0);
    buffer.clear();
    processor->processBlock(buffer, midiBuffer);
    
    float totalRMS = calculateRMS(audioSamples);
    EXPECT_GT(totalRMS, 0.001f) << "Real-time parameter changes should not break audio";
    
    saveAudioToFile(audioSamples, "realtime_parameter_test.csv");
    CS_DBG("Real-time parameter test completed");
}

// Run all tests
int main(int argc, char** argv) {
    std::cout << "=== Standalone Unison Detune Test Suite ===" << std::endl;
    
    ::testing::InitGoogleTest(&argc, argv);
    
    // Run tests
    int result = RUN_ALL_TESTS();
    
    std::cout << "=== Test Results ===" << std::endl;
    std::cout << "Check ~/Desktop/ for generated audio files (.csv format)" << std::endl;
    std::cout << "Check ~/Desktop/unison_detune_test_debug.txt for detailed logs" << std::endl;
    
    return result;
}