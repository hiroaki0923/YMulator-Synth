#include <gtest/gtest.h>
#include "../src/PluginProcessor.h"
#include "../src/utils/ParameterIDs.h"
#include "../src/utils/Debug.h"
#include "../src/core/ParameterManager.h"

/**
 * Test actual audio output in Random Pan Mode
 */
class RandomPanOutputTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        processor->setPlayConfigDetails(0, 2, 44100, 512);
        processor->prepareToPlay(44100, 512);
        
        // Set to RANDOM mode
        auto* globalPanParam = dynamic_cast<juce::AudioParameterChoice*>(
            processor->getParameters().getParameter(ParamID::Global::GlobalPan));
        ASSERT_NE(globalPanParam, nullptr);
        float randomValue = globalPanParam->convertTo0to1(static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM));
        globalPanParam->setValueNotifyingHost(randomValue);
        
        CS_FILE_DBG("=== RandomPanOutputTest SetUp Complete - RANDOM mode active ===");
    }
    
    void TearDown() override {
        if (processor) {
            processor->releaseResources();
        }
        CS_FILE_DBG("=== RandomPanOutputTest TearDown Complete ===");
    }
    
    struct AudioAnalysis {
        float leftRMS;
        float rightRMS;
        float leftPeak;
        float rightPeak;
        bool hasAudio;
    };
    
    AudioAnalysis analyzeAudioOutput(const juce::AudioBuffer<float>& buffer) {
        AudioAnalysis result = {0.0f, 0.0f, 0.0f, 0.0f, false};
        
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0) {
            return result;
        }
        
        int numSamples = buffer.getNumSamples();
        float leftSum = 0.0f, rightSum = 0.0f;
        
        for (int i = 0; i < numSamples; ++i) {
            float leftSample = buffer.getSample(0, i);
            float rightSample = buffer.getSample(1, i);
            
            // RMS calculation
            leftSum += leftSample * leftSample;
            rightSum += rightSample * rightSample;
            
            // Peak calculation
            result.leftPeak = std::max(result.leftPeak, std::abs(leftSample));
            result.rightPeak = std::max(result.rightPeak, std::abs(rightSample));
        }
        
        result.leftRMS = std::sqrt(leftSum / numSamples);
        result.rightRMS = std::sqrt(rightSum / numSamples);
        result.hasAudio = (result.leftPeak > 0.001f || result.rightPeak > 0.001f);
        
        return result;
    }
    
    void playNoteAndAnalyze(int noteNumber, int velocity, AudioAnalysis& analysis) {
        juce::AudioBuffer<float> audioBuffer(2, 1024);  // Larger buffer for better analysis
        audioBuffer.clear();
        
        // Note on
        juce::MidiBuffer midiBuffer;
        auto noteOnMessage = juce::MidiMessage::noteOn(1, noteNumber, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(noteOnMessage, 0);
        
        CS_FILE_DBG("Playing note " + juce::String(noteNumber) + " with velocity " + juce::String(velocity));
        
        // Process initial block with note on
        processor->processBlock(audioBuffer, midiBuffer);
        
        // Process several more blocks to let the note develop
        juce::MidiBuffer emptyMidi;
        for (int block = 0; block < 8; ++block) {
            processor->processBlock(audioBuffer, emptyMidi);
        }
        
        analysis = analyzeAudioOutput(audioBuffer);
        
        CS_FILE_DBG("Note " + juce::String(noteNumber) + " analysis: " +
                   "Left RMS=" + juce::String(analysis.leftRMS, 6) + 
                   ", Right RMS=" + juce::String(analysis.rightRMS, 6) +
                   ", Left Peak=" + juce::String(analysis.leftPeak, 6) +
                   ", Right Peak=" + juce::String(analysis.rightPeak, 6) +
                   ", Has Audio=" + (analysis.hasAudio ? "YES" : "NO"));
        
        // Note off
        juce::MidiBuffer noteOffMidi;
        auto noteOffMessage = juce::MidiMessage::noteOff(1, noteNumber, static_cast<uint8_t>(velocity));
        noteOffMidi.addEvent(noteOffMessage, 0);
        processor->processBlock(audioBuffer, noteOffMidi);
        
        // Process a few blocks to let note fade
        for (int block = 0; block < 3; ++block) {
            processor->processBlock(audioBuffer, emptyMidi);
        }
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
};

TEST_F(RandomPanOutputTest, VerifyDifferentStereoOutputs) {
    CS_FILE_DBG("=== Starting VerifyDifferentStereoOutputs test ===");
    
    std::vector<AudioAnalysis> results;
    
    // Test multiple notes to check for different pan positions
    for (int note = 60; note < 68; ++note) {
        AudioAnalysis analysis;
        playNoteAndAnalyze(note, 100, analysis);
        results.push_back(analysis);
        
        if (!analysis.hasAudio) {
            CS_FILE_DBG("WARNING: Note " + juce::String(note) + " produced no audible output");
        }
    }
    
    // Analyze the results
    int leftOnlyCount = 0, rightOnlyCount = 0, centerCount = 0, noAudioCount = 0;
    const float panThreshold = 0.1f;  // Threshold for determining pan position
    
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& result = results[i];
        
        if (!result.hasAudio) {
            noAudioCount++;
            continue;
        }
        
        // Determine pan position based on RMS levels
        float leftRatio = result.leftRMS / (result.leftRMS + result.rightRMS + 0.0001f);
        float rightRatio = result.rightRMS / (result.leftRMS + result.rightRMS + 0.0001f);
        
        CS_FILE_DBG("Note " + juce::String(60 + i) + " - Left ratio: " + juce::String(leftRatio, 3) + 
                   ", Right ratio: " + juce::String(rightRatio, 3));
        
        if (leftRatio > (1.0f - panThreshold) && rightRatio < panThreshold) {
            leftOnlyCount++;
            CS_FILE_DBG("Note " + juce::String(60 + i) + " classified as LEFT");
        } else if (rightRatio > (1.0f - panThreshold) && leftRatio < panThreshold) {
            rightOnlyCount++;
            CS_FILE_DBG("Note " + juce::String(60 + i) + " classified as RIGHT");
        } else if (leftRatio > panThreshold && rightRatio > panThreshold) {
            centerCount++;
            CS_FILE_DBG("Note " + juce::String(60 + i) + " classified as CENTER");
        }
    }
    
    CS_FILE_DBG("Analysis summary: Left=" + juce::String(leftOnlyCount) + 
               ", Right=" + juce::String(rightOnlyCount) + 
               ", Center=" + juce::String(centerCount) + 
               ", No Audio=" + juce::String(noAudioCount));
    
    // Assertions
    EXPECT_GT(results.size() - noAudioCount, 0) << "At least some notes should produce audio";
    
    // In random pan mode, we should see different pan positions
    int differentPositions = (leftOnlyCount > 0 ? 1 : 0) + 
                           (rightOnlyCount > 0 ? 1 : 0) + 
                           (centerCount > 0 ? 1 : 0);
    
    EXPECT_GE(differentPositions, 2) << "Random pan should produce at least 2 different stereo positions. Got: Left=" + 
                                        std::to_string(leftOnlyCount) + ", Right=" + std::to_string(rightOnlyCount) + 
                                        ", Center=" + std::to_string(centerCount);
    
    CS_FILE_DBG("=== VerifyDifferentStereoOutputs test complete ===");
}

TEST_F(RandomPanOutputTest, ForceUpdateYmfmParametersInRandomMode) {
    CS_FILE_DBG("=== Starting ForceUpdateYmfmParametersInRandomMode test ===");
    
    // Play a note first to get a baseline
    AudioAnalysis beforeAnalysis;
    playNoteAndAnalyze(60, 100, beforeAnalysis);
    
    CS_FILE_DBG("Before forced parameter update - Left RMS: " + juce::String(beforeAnalysis.leftRMS, 6) + 
               ", Right RMS: " + juce::String(beforeAnalysis.rightRMS, 6));
    
    // Force processing many audio blocks to trigger updateYmfmParameters
    juce::AudioBuffer<float> audioBuffer(2, 512);
    juce::MidiBuffer emptyMidi;
    
    CS_FILE_DBG("Processing many blocks to trigger updateYmfmParameters...");
    for (int block = 0; block < 50; ++block) {
        audioBuffer.clear();
        processor->processBlock(audioBuffer, emptyMidi);
    }
    
    // Play the same note again to see if pan was overridden
    AudioAnalysis afterAnalysis;
    playNoteAndAnalyze(60, 100, afterAnalysis);
    
    CS_FILE_DBG("After forced parameter update - Left RMS: " + juce::String(afterAnalysis.leftRMS, 6) + 
               ", Right RMS: " + juce::String(afterAnalysis.rightRMS, 6));
    
    CS_FILE_DBG("=== ForceUpdateYmfmParametersInRandomMode test complete ===");
}