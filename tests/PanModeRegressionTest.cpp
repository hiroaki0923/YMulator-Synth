#include <gtest/gtest.h>
#include "../src/PluginProcessor.h"
#include "../src/utils/ParameterIDs.h"
#include "../src/utils/Debug.h"

/**
 * Regression tests to prevent pan mode issues discovered during Random Pan Mode debugging.
 * These tests ensure that the complex interaction between MidiProcessor and PluginProcessor
 * pan settings doesn't cause future regressions.
 */
class PanModeRegressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        processor->setPlayConfigDetails(0, 2, 44100, 512);
        processor->prepareToPlay(44100, 512);
    }
    
    void TearDown() override {
        if (processor) {
            processor->releaseResources();
        }
    }
    
    juce::AudioParameterChoice* getGlobalPanParameter() {
        return dynamic_cast<juce::AudioParameterChoice*>(
            processor->getParameters().getParameter(ParamID::Global::GlobalPan));
    }
    
    void setGlobalPanMode(GlobalPanPosition mode) {
        auto* globalPanParam = getGlobalPanParameter();
        ASSERT_NE(globalPanParam, nullptr);
        float value = globalPanParam->convertTo0to1(static_cast<int>(mode));
        globalPanParam->setValueNotifyingHost(value);
    }
    
    struct PanAnalysis {
        float leftRMS;
        float rightRMS;
        bool isLeft() const { return leftRMS > 0.1f && rightRMS < 0.1f; }
        bool isRight() const { return rightRMS > 0.1f && leftRMS < 0.1f; }
        bool isCenter() const { return leftRMS > 0.1f && rightRMS > 0.1f; }
        bool hasAudio() const { return leftRMS > 0.001f || rightRMS > 0.001f; }
    };
    
    PanAnalysis playNoteAndAnalyze(int noteNumber, int velocity = 100) {
        juce::AudioBuffer<float> audioBuffer(2, 1024);
        audioBuffer.clear();
        
        // Note on
        juce::MidiBuffer midiBuffer;
        auto noteOnMessage = juce::MidiMessage::noteOn(1, noteNumber, static_cast<uint8_t>(velocity));
        midiBuffer.addEvent(noteOnMessage, 0);
        processor->processBlock(audioBuffer, midiBuffer);
        
        // Let note develop
        juce::MidiBuffer emptyMidi;
        for (int block = 0; block < 5; ++block) {
            processor->processBlock(audioBuffer, emptyMidi);
        }
        
        // Analyze
        PanAnalysis result = {0.0f, 0.0f};
        int numSamples = audioBuffer.getNumSamples();
        for (int i = 0; i < numSamples; ++i) {
            float leftSample = audioBuffer.getSample(0, i);
            float rightSample = audioBuffer.getSample(1, i);
            result.leftRMS += leftSample * leftSample;
            result.rightRMS += rightSample * rightSample;
        }
        result.leftRMS = std::sqrt(result.leftRMS / numSamples);
        result.rightRMS = std::sqrt(result.rightRMS / numSamples);
        
        // Note off
        juce::MidiBuffer noteOffMidi;
        auto noteOffMessage = juce::MidiMessage::noteOff(1, noteNumber, static_cast<uint8_t>(velocity));
        noteOffMidi.addEvent(noteOffMessage, 0);
        processor->processBlock(audioBuffer, noteOffMidi);
        
        return result;
    }
    
    void forceUpdateYmfmParameters(int cycles = 10) {
        juce::AudioBuffer<float> audioBuffer(2, 512);
        juce::MidiBuffer emptyMidi;
        for (int i = 0; i < cycles; ++i) {
            audioBuffer.clear();
            processor->processBlock(audioBuffer, emptyMidi);
        }
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
};

TEST_F(PanModeRegressionTest, RandomModeNotOverriddenByUpdateYmfmParameters) {
    // Set to RANDOM mode
    setGlobalPanMode(GlobalPanPosition::RANDOM);
    
    // Play multiple notes to establish different random pan values
    std::vector<PanAnalysis> initialResults;
    for (int note = 60; note < 65; ++note) {
        auto analysis = playNoteAndAnalyze(note);
        if (analysis.hasAudio()) {
            initialResults.push_back(analysis);
        }
    }
    
    // Force many updateYmfmParameters cycles (simulates long playback)
    forceUpdateYmfmParameters(50);
    
    // Play the same notes again
    std::vector<PanAnalysis> afterUpdateResults;
    for (int note = 60; note < 65; ++note) {
        auto analysis = playNoteAndAnalyze(note);
        if (analysis.hasAudio()) {
            afterUpdateResults.push_back(analysis);
        }
    }
    
    ASSERT_EQ(initialResults.size(), afterUpdateResults.size());
    
    // Random mode should still produce varied pan positions
    // (Not necessarily the same as before, but still varied)
    int centerCount = 0, leftCount = 0, rightCount = 0;
    for (const auto& result : afterUpdateResults) {
        if (result.isCenter()) centerCount++;
        else if (result.isLeft()) leftCount++;
        else if (result.isRight()) rightCount++;
    }
    
    int differentPositions = (centerCount > 0 ? 1 : 0) + 
                           (leftCount > 0 ? 1 : 0) + 
                           (rightCount > 0 ? 1 : 0);
    
    EXPECT_GE(differentPositions, 2) 
        << "Random pan should still work after updateYmfmParameters cycles. "
        << "Got Center=" << centerCount << ", Left=" << leftCount << ", Right=" << rightCount;
}

TEST_F(PanModeRegressionTest, FixedPanModesNotAffectedByRandomModeCode) {
    // Test that LEFT/CENTER/RIGHT modes work correctly and aren't affected by
    // the RANDOM mode code paths
    
    struct TestCase {
        GlobalPanPosition mode;
        std::string name;
        std::function<bool(const PanAnalysis&)> validator;
    };
    
    std::vector<TestCase> testCases = {
        {GlobalPanPosition::LEFT, "LEFT", [](const PanAnalysis& a) { return a.isLeft(); }},
        {GlobalPanPosition::CENTER, "CENTER", [](const PanAnalysis& a) { return a.isCenter(); }},
        {GlobalPanPosition::RIGHT, "RIGHT", [](const PanAnalysis& a) { return a.isRight(); }}
    };
    
    for (const auto& testCase : testCases) {
        setGlobalPanMode(testCase.mode);
        
        // Test multiple notes to ensure consistency
        int validCount = 0;
        for (int note = 60; note < 65; ++note) {
            auto analysis = playNoteAndAnalyze(note);
            if (analysis.hasAudio()) {
                EXPECT_TRUE(testCase.validator(analysis))
                    << "Note " << note << " in " << testCase.name << " mode failed validation. "
                    << "Left RMS: " << analysis.leftRMS << ", Right RMS: " << analysis.rightRMS;
                validCount++;
            }
        }
        
        EXPECT_GT(validCount, 0) << "No audio output in " << testCase.name << " mode";
    }
}

TEST_F(PanModeRegressionTest, PresetLoadingPreservesRandomMode) {
    // Set to RANDOM mode
    setGlobalPanMode(GlobalPanPosition::RANDOM);
    
    // Verify RANDOM mode is active
    auto analysis1 = playNoteAndAnalyze(60);
    ASSERT_TRUE(analysis1.hasAudio()) << "Should have audio output in RANDOM mode";
    
    // Load a preset (this was one of the places that could override pan)
    processor->setCurrentPreset(0);  // Load first preset
    
    // Verify RANDOM mode is still active
    EXPECT_EQ(getGlobalPanParameter()->getIndex(), static_cast<int>(GlobalPanPosition::RANDOM))
        << "RANDOM mode should be preserved after preset loading";
    
    // Verify RANDOM mode still works functionally
    std::vector<PanAnalysis> results;
    for (int note = 60; note < 65; ++note) {
        auto analysis = playNoteAndAnalyze(note);
        if (analysis.hasAudio()) {
            results.push_back(analysis);
        }
    }
    
    // Should still get varied pan positions
    int centerCount = 0, leftCount = 0, rightCount = 0;
    for (const auto& result : results) {
        if (result.isCenter()) centerCount++;
        else if (result.isLeft()) leftCount++;
        else if (result.isRight()) rightCount++;
    }
    
    int differentPositions = (centerCount > 0 ? 1 : 0) + 
                           (leftCount > 0 ? 1 : 0) + 
                           (rightCount > 0 ? 1 : 0);
    
    EXPECT_GE(differentPositions, 2) 
        << "Random pan should work after preset loading";
}

TEST_F(PanModeRegressionTest, RapidPanModeChanges) {
    // Test rapid switching between pan modes to ensure no race conditions
    
    GlobalPanPosition modes[] = {
        GlobalPanPosition::LEFT,
        GlobalPanPosition::RANDOM,
        GlobalPanPosition::CENTER,
        GlobalPanPosition::RANDOM,
        GlobalPanPosition::RIGHT,
        GlobalPanPosition::RANDOM
    };
    
    for (auto mode : modes) {
        setGlobalPanMode(mode);
        
        // Immediately test the mode
        auto analysis = playNoteAndAnalyze(60);
        
        EXPECT_TRUE(analysis.hasAudio()) 
            << "Should have audio output after switching to mode " << static_cast<int>(mode);
        
        // Force some parameter updates
        forceUpdateYmfmParameters(3);
    }
    
    // Final verification that RANDOM mode works after all the switching
    setGlobalPanMode(GlobalPanPosition::RANDOM);
    
    std::vector<PanAnalysis> finalResults;
    for (int note = 60; note < 64; ++note) {
        auto analysis = playNoteAndAnalyze(note);
        if (analysis.hasAudio()) {
            finalResults.push_back(analysis);
        }
    }
    
    // Should still get some variation in the final RANDOM mode test
    bool hasVariation = false;
    if (finalResults.size() >= 2) {
        for (size_t i = 1; i < finalResults.size(); ++i) {
            float leftDiff = std::abs(finalResults[i].leftRMS - finalResults[0].leftRMS);
            float rightDiff = std::abs(finalResults[i].rightRMS - finalResults[0].rightRMS);
            if (leftDiff > 0.1f || rightDiff > 0.1f) {
                hasVariation = true;
                break;
            }
        }
    }
    
    EXPECT_TRUE(hasVariation) 
        << "RANDOM mode should show variation after rapid mode changes";
}