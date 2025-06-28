#include <gtest/gtest.h>
#include "../src/PluginProcessor.h"
#include "../src/utils/ParameterIDs.h"
#include "../src/utils/Debug.h"
#include "../src/core/ParameterManager.h"
/**
 * Simple debug test to trace Random Pan Mode execution
 */
class RandomPanDebugTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<YMulatorSynthAudioProcessor>();
        processor->setPlayConfigDetails(0, 2, 44100, 512);
        processor->prepareToPlay(44100, 512);
        
        CS_FILE_DBG("=== RandomPanDebugTest SetUp Complete ===");
    }
    
    void TearDown() override {
        if (processor) {
            processor->releaseResources();
        }
        CS_FILE_DBG("=== RandomPanDebugTest TearDown Complete ===");
    }
    
    juce::AudioParameterChoice* getGlobalPanParameter() {
        return dynamic_cast<juce::AudioParameterChoice*>(
            processor->getParameters().getParameter(ParamID::Global::GlobalPan));
    }
    
    std::unique_ptr<YMulatorSynthAudioProcessor> processor;
};

TEST_F(RandomPanDebugTest, VerifyRandomModeSetup) {
    CS_FILE_DBG("=== Starting VerifyRandomModeSetup test ===");
    
    // Find and set GlobalPan to RANDOM
    auto* globalPanParam = getGlobalPanParameter();
    ASSERT_NE(globalPanParam, nullptr) << "GlobalPan parameter not found";
    
    CS_FILE_DBG("Found GlobalPan parameter");
    CS_FILE_DBG("Current GlobalPan value: " + juce::String(globalPanParam->getIndex()));
    
    // Set to RANDOM mode
    float randomValue = globalPanParam->convertTo0to1(static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM));
    globalPanParam->setValueNotifyingHost(randomValue);
    
    CS_FILE_DBG("Set GlobalPan to RANDOM mode (index " + juce::String(static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM)) + ")");
    CS_FILE_DBG("New GlobalPan value: " + juce::String(globalPanParam->getIndex()));
    
    EXPECT_EQ(globalPanParam->getIndex(), static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM));
    
    CS_FILE_DBG("=== VerifyRandomModeSetup test complete ===");
}

TEST_F(RandomPanDebugTest, TraceNotePlayback) {
    CS_FILE_DBG("=== Starting TraceNotePlayback test ===");
    
    // Set to RANDOM mode first
    auto* globalPanParam = getGlobalPanParameter();
    float randomValue = globalPanParam->convertTo0to1(static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM));
    globalPanParam->setValueNotifyingHost(randomValue);
    
    CS_FILE_DBG("Set to RANDOM mode, now testing note playback...");
    
    // Play a few notes and trace the execution
    juce::AudioBuffer<float> audioBuffer(2, 512);
    
    for (int note = 60; note < 63; ++note) {
        CS_FILE_DBG("=== Playing note " + juce::String(note) + " ===");
        
        audioBuffer.clear();
        
        // Create MIDI note on
        juce::MidiBuffer midiBuffer;
        auto noteOnMessage = juce::MidiMessage::noteOn(1, note, static_cast<uint8_t>(100));
        midiBuffer.addEvent(noteOnMessage, 0);
        
        // Process block - this should trigger MidiProcessor::processMidiNoteOn
        processor->processBlock(audioBuffer, midiBuffer);
        
        CS_FILE_DBG("Note " + juce::String(note) + " processing complete");
        
        // Stop note
        juce::MidiBuffer stopMidi;
        auto noteOffMessage = juce::MidiMessage::noteOff(1, note, static_cast<uint8_t>(100));
        stopMidi.addEvent(noteOffMessage, 0);
        processor->processBlock(audioBuffer, stopMidi);
        
        CS_FILE_DBG("Note " + juce::String(note) + " stopped");
    }
    
    CS_FILE_DBG("=== TraceNotePlayback test complete ===");
}

TEST_F(RandomPanDebugTest, TestUpdateYmfmParameters) {
    CS_FILE_DBG("=== Starting TestUpdateYmfmParameters test ===");
    
    // Set to RANDOM mode
    auto* globalPanParam = getGlobalPanParameter();
    float randomValue = globalPanParam->convertTo0to1(static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM));
    globalPanParam->setValueNotifyingHost(randomValue);
    
    CS_FILE_DBG("Testing updateYmfmParameters in RANDOM mode...");
    
    // Process some audio blocks - this should trigger updateYmfmParameters periodically
    juce::AudioBuffer<float> audioBuffer(2, 512);
    juce::MidiBuffer emptyMidi;
    
    for (int block = 0; block < 5; ++block) {
        CS_FILE_DBG("Processing audio block " + juce::String(block));
        audioBuffer.clear();
        processor->processBlock(audioBuffer, emptyMidi);
    }
    
    CS_FILE_DBG("=== TestUpdateYmfmParameters test complete ===");
}