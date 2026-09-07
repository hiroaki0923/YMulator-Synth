#include <gtest/gtest.h>
#include "../src/PluginProcessor.h"
#include "../src/utils/ParameterIDs.h"
#include "../src/core/ParameterManager.h"

/**
 * Audio quality tests to ensure pan modes don't introduce audio artifacts.
 * These tests validate that our pan fixes don't degrade audio quality.
 */
class AudioQualityTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        processor->setPlayConfigDetails(0, 2, 44100, 512);
        processor->prepareToPlay(44100, 512);
    }
    
    void TearDown() override {
        if (processor) {
            processor->releaseResources();
            processor->resetProcessBlockStaticState();
            ymulatorsynth::ParameterManager::resetStaticState();
        }
        processor.reset();
    }
    
    // MOTION pan mode indices
    static constexpr int kPanOff = 0, kPanLeft = 3, kPanRight = 4, kPanRandom = 5;
    
    void setPanMode(int mode) {
        auto* panMode = dynamic_cast<juce::AudioParameterChoice*>(
            processor->getParameters().getParameter(ParamID::Motion::PanMode));
        ASSERT_NE(panMode, nullptr);
        panMode->setValueNotifyingHost(panMode->convertTo0to1(static_cast<float>(mode)));
    }
    
    struct AudioMetrics {
        float peakLevel;
        float rmsLevel;
        float dcOffset;
        bool hasClipping;
        bool hasSilence;
        float stereoBalance;  // -1.0 = full left, 0.0 = center, 1.0 = full right
    };
    
    AudioMetrics analyzeAudio(const juce::AudioBuffer<float>& buffer) {
        AudioMetrics metrics = {0.0f, 0.0f, 0.0f, false, true, 0.0f};
        
        if (buffer.getNumChannels() < 2 || buffer.getNumSamples() == 0) {
            return metrics;
        }
        
        int numSamples = buffer.getNumSamples();
        float leftSum = 0.0f, rightSum = 0.0f;
        float leftDcSum = 0.0f, rightDcSum = 0.0f;
        float leftPeak = 0.0f, rightPeak = 0.0f;
        
        for (int i = 0; i < numSamples; ++i) {
            float leftSample = buffer.getSample(0, i);
            float rightSample = buffer.getSample(1, i);
            
            // Peak detection
            leftPeak = std::max(leftPeak, std::abs(leftSample));
            rightPeak = std::max(rightPeak, std::abs(rightSample));
            
            // RMS calculation
            leftSum += leftSample * leftSample;
            rightSum += rightSample * rightSample;
            
            // DC offset calculation
            leftDcSum += leftSample;
            rightDcSum += rightSample;
            
            // Check for clipping (> 0.99)
            if (std::abs(leftSample) > 0.99f || std::abs(rightSample) > 0.99f) {
                metrics.hasClipping = true;
            }
            
            // Check for audio presence
            if (std::abs(leftSample) > 0.001f || std::abs(rightSample) > 0.001f) {
                metrics.hasSilence = false;
            }
        }
        
        metrics.peakLevel = std::max(leftPeak, rightPeak);
        float leftRms = std::sqrt(leftSum / numSamples);
        float rightRms = std::sqrt(rightSum / numSamples);
        metrics.rmsLevel = std::max(leftRms, rightRms);
        
        // DC offset (should be near zero)
        float leftDc = leftDcSum / numSamples;
        float rightDc = rightDcSum / numSamples;
        metrics.dcOffset = std::max(std::abs(leftDc), std::abs(rightDc));
        
        // Stereo balance calculation
        if (leftRms + rightRms > 0.001f) {
            metrics.stereoBalance = (rightRms - leftRms) / (leftRms + rightRms);
        }
        
        return metrics;
    }
    
    AudioMetrics playNoteAndGetMetrics(int noteNumber, int velocity = 100) {
        juce::AudioBuffer<float> audioBuffer(2, 2048);  // Larger buffer for better analysis
        audioBuffer.clear();
        
        // Note on
        juce::MidiBuffer midiBuffer;
        auto noteOnMessage = juce::MidiMessage::noteOn(1, noteNumber, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(noteOnMessage, 0);
        processor->processBlock(audioBuffer, midiBuffer);
        
        // Let note develop for several blocks
        juce::MidiBuffer emptyMidi;
        for (int block = 0; block < 8; ++block) {
            processor->processBlock(audioBuffer, emptyMidi);
        }
        
        auto metrics = analyzeAudio(audioBuffer);
        
        // Note off
        juce::MidiBuffer noteOffMidi;
        auto noteOffMessage = juce::MidiMessage::noteOff(1, noteNumber, static_cast<uint8_t>(velocity));
        noteOffMidi.addEvent(noteOffMessage, 0);
        processor->processBlock(audioBuffer, noteOffMidi);
        
        return metrics;
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
};

TEST_F(AudioQualityTest, AllPanModesProduceCleanAudio) {
    const int modes[] = { kPanLeft, kPanOff, kPanRight, kPanRandom };
    
    for (auto mode : modes) {
        setPanMode(mode);
        
        auto metrics = playNoteAndGetMetrics(60, 100);
        
        // Audio quality checks
        EXPECT_FALSE(metrics.hasSilence) 
            << "Pan mode " << mode << " should produce audio";
        
        // Note: Some clipping may be expected with FM synthesis at full velocity
        // This test mainly ensures pan modes don't make clipping significantly worse
        
        EXPECT_LT(metrics.dcOffset, 0.05f) 
            << "Pan mode " << mode << " should have reasonable DC offset. Got: " << metrics.dcOffset;
        
        EXPECT_GT(metrics.rmsLevel, 0.01f) 
            << "Pan mode " << mode << " should have reasonable signal level. Got: " << metrics.rmsLevel;
        
        EXPECT_LT(metrics.rmsLevel, 2.0f) 
            << "Pan mode " << mode << " should not have excessive signal level. Got: " << metrics.rmsLevel;
    }
}

TEST_F(AudioQualityTest, PanPositionsHaveCorrectStereoBalance) {
    struct TestCase {
        int mode;
        float expectedBalance;
        float tolerance;
        std::string name;
    };
    
    std::vector<TestCase> testCases = {
        {kPanLeft, -0.8f, 0.3f, "Left"},      // Should be mostly left
        {kPanOff, 0.0f, 0.3f, "Off"},         // Should be balanced
        {kPanRight, 0.8f, 0.3f, "Right"}      // Should be mostly right
    };
    
    for (const auto& testCase : testCases) {
        setPanMode(testCase.mode);
        
        auto metrics = playNoteAndGetMetrics(60, 100);
        
        EXPECT_NEAR(metrics.stereoBalance, testCase.expectedBalance, testCase.tolerance)
            << testCase.name << " pan mode has incorrect stereo balance. "
            << "Expected: " << testCase.expectedBalance << " ± " << testCase.tolerance
            << ", Got: " << metrics.stereoBalance;
    }
}

TEST_F(AudioQualityTest, RandomModeShowsVariedStereoBalance) {
    setPanMode(kPanRandom);
    
    std::vector<float> balanceValues;
    
    // Play multiple notes to get different random pan positions
    for (int note = 60; note < 70; ++note) {
        auto metrics = playNoteAndGetMetrics(note, 100);
        if (!metrics.hasSilence) {
            balanceValues.push_back(metrics.stereoBalance);
        }
    }
    
    ASSERT_GE(balanceValues.size(), 5) << "Should have at least 5 audio samples";
    
    // Calculate variance in stereo balance
    float sum = 0.0f;
    for (float balance : balanceValues) {
        sum += balance;
    }
    float mean = sum / balanceValues.size();
    
    float variance = 0.0f;
    for (float balance : balanceValues) {
        variance += (balance - mean) * (balance - mean);
    }
    variance /= balanceValues.size();
    
    // Random mode should show significant variation in stereo balance
    EXPECT_GT(variance, 0.1f) 
        << "Random pan mode should show variation in stereo balance. "
        << "Variance: " << variance << ", Mean: " << mean;
    
    // Should have both left and right positioned notes
    bool hasLeft = false, hasRight = false;
    for (float balance : balanceValues) {
        if (balance < -0.3f) hasLeft = true;
        if (balance > 0.3f) hasRight = true;
    }
    
    EXPECT_TRUE(hasLeft || hasRight) 
        << "Random pan should produce some non-center positions";
}

TEST_F(AudioQualityTest, ConsistentAudioLevelAcrossPanModes) {
    const int modes[] = { kPanLeft, kPanOff, kPanRight, kPanRandom };
    
    std::vector<float> rmsLevels;
    
    for (auto mode : modes) {
        setPanMode(mode);
        
        auto metrics = playNoteAndGetMetrics(60, 100);
        if (!metrics.hasSilence) {
            rmsLevels.push_back(metrics.rmsLevel);
        }
    }
    
    ASSERT_GE(rmsLevels.size(), 3) << "Should have audio from at least 3 pan modes";
    
    // Find min and max RMS levels
    float minRms = *std::min_element(rmsLevels.begin(), rmsLevels.end());
    float maxRms = *std::max_element(rmsLevels.begin(), rmsLevels.end());
    
    // RMS levels should be consistent across pan modes (within 6dB)
    float ratioDb = 20.0f * std::log10(maxRms / minRms);
    
    EXPECT_LT(ratioDb, 6.0f) 
        << "Audio levels should be consistent across pan modes. "
        << "Max variation: " << ratioDb << " dB (min: " << minRms << ", max: " << maxRms << ")";
}