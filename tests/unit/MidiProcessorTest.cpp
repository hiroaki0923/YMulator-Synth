#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "../../src/core/MidiProcessor.h"
#include "../../src/core/VoiceManagerInterface.h" 
#include "../../src/dsp/YmfmWrapperInterface.h"
#include "../../src/utils/ParameterIDs.h"
#include "../../src/utils/Debug.h"

#include <juce_audio_processors/juce_audio_processors.h>

using namespace ymulatorsynth;
using ::testing::_;
using ::testing::Return;
using ::testing::InSequence;

// Dummy AudioProcessor implementation for testing
class DummyAudioProcessor : public juce::AudioProcessor {
public:
    DummyAudioProcessor() : juce::AudioProcessor(BusesProperties()) {}
    
    const juce::String getName() const override { return "DummyProcessor"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

// Mock VoiceManager for testing
class MockVoiceManager : public VoiceManagerInterface {
public:
    MOCK_METHOD(int, allocateVoice, (uint8_t note, uint8_t velocity), (override));
    MOCK_METHOD(int, allocateVoiceWithNoisePriority, (uint8_t note, uint8_t velocity, bool needsNoise), (override));
    MOCK_METHOD(void, releaseVoice, (uint8_t note), (override));
    MOCK_METHOD(void, releaseAllVoices, (), (override));
    MOCK_METHOD(int, getChannelForNote, (uint8_t note), (const, override));
    MOCK_METHOD(uint8_t, getNoteForChannel, (int channel), (const, override));
    MOCK_METHOD(uint8_t, getVelocityForChannel, (int channel), (const, override));
    MOCK_METHOD(bool, isVoiceActive, (int channel), (const, override));
    MOCK_METHOD(void, setStealingPolicy, (StealingPolicy policy), (override));
    MOCK_METHOD(void, reset, (), (override));
};

// Mock YmfmWrapper for testing (simplified for MidiProcessor tests)
class MockYmfmWrapper : public YmfmWrapperInterface {
public:
    // Required interface methods - provide minimal implementations to satisfy compiler
    MOCK_METHOD(void, initialize, (YmfmWrapperInterface::ChipType type, uint32_t outputSampleRate), (override));
    MOCK_METHOD(void, reset, (), (override));
    MOCK_METHOD(bool, isInitialized, (), (const, override));
    MOCK_METHOD(void, generateSamples, (float* leftBuffer, float* rightBuffer, int numSamples), (override));
    
    // MIDI interface methods (used by MidiProcessor)
    MOCK_METHOD(void, noteOn, (uint8_t channel, uint8_t note, uint8_t velocity), (override));
    MOCK_METHOD(void, noteOff, (uint8_t channel, uint8_t note), (override));
    
    // Parameter control methods (used by MidiProcessor)
    MOCK_METHOD(void, setOperatorParameter, (uint8_t channel, uint8_t operator_num, YmfmWrapperInterface::OperatorParameter param, uint8_t value), (override));
    MOCK_METHOD(void, setChannelParameter, (uint8_t channel, YmfmWrapperInterface::ChannelParameter param, uint8_t value), (override));
    MOCK_METHOD(void, setAlgorithm, (uint8_t channel, uint8_t algorithm), (override));
    MOCK_METHOD(void, setFeedback, (uint8_t channel, uint8_t feedback), (override));
    
    // Advanced features (used by MidiProcessor)  
    MOCK_METHOD(void, setPitchBend, (uint8_t channel, float semitones), (override));
    MOCK_METHOD(void, setChannelPan, (uint8_t channel, float panValue), (override));
    
    // Stub implementations for other required interface methods
    MOCK_METHOD(void, setLfoParameters, (uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform), (override));
    MOCK_METHOD(void, setChannelAmsPms, (uint8_t channel, uint8_t ams, uint8_t pms), (override));
    MOCK_METHOD(void, setOperatorAmsEnable, (uint8_t channel, uint8_t operator_num, bool enable), (override));
    MOCK_METHOD(void, setOperatorParameters, (uint8_t channel, uint8_t operator_num, uint8_t tl, uint8_t ar, uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l, uint8_t ks, uint8_t mul, uint8_t dt1, uint8_t dt2), (override));
    MOCK_METHOD(void, setOperatorEnvelope, (uint8_t channel, uint8_t operator_num, uint8_t ar, uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l), (override));
    MOCK_METHOD(void, setVelocitySensitivity, (uint8_t channel, uint8_t operator_num, float sensitivity), (override));
    MOCK_METHOD(void, applyVelocityToChannel, (uint8_t channel, uint8_t velocity), (override));
    MOCK_METHOD(void, setNoiseEnable, (bool enable), (override));
    MOCK_METHOD(void, setNoiseFrequency, (uint8_t frequency), (override));
    MOCK_METHOD(bool, getNoiseEnable, (), (const, override));
    MOCK_METHOD(uint8_t, getNoiseFrequency, (), (const, override));
    MOCK_METHOD(void, setNoiseParameters, (bool enable, uint8_t frequency), (override));
    MOCK_METHOD(void, testNoiseChannel, (), (override));
    
    // Register access
    MOCK_METHOD(void, writeRegister, (int address, uint8_t data), (override));
    MOCK_METHOD(uint8_t, readCurrentRegister, (int address), (const, override));
    
    // Batch operations (simplified for testing)
    void batchUpdateChannelParameters(uint8_t channel, uint8_t algorithm, uint8_t feedback, 
                                     const std::array<std::array<uint8_t, 10>, 4>& operatorParams) override {
        // Stub implementation for testing
    }
    
    // Debug and monitoring
    MOCK_METHOD(YmfmWrapperInterface::EnvelopeDebugInfo, getEnvelopeDebugInfo, (uint8_t channel, uint8_t operator_num), (const, override));
};

class MidiProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create parameter layout
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        
        // Add required parameters for testing
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParamID::Global::Algorithm, "Algorithm", 
            juce::StringArray{"Alg0", "Alg1", "Alg2", "Alg3", "Alg4", "Alg5", "Alg6", "Alg7"}, 0));
            
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Global::Feedback, "Feedback", 0, 7, 0));
            
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            ParamID::Global::GlobalPan, "Global Pan",
            juce::StringArray{"Left", "Center", "Right", "Random"}, 1));
            
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Global::PitchBendRange, "Pitch Bend Range", 1, 12, 2));
            
        layout.add(std::make_unique<juce::AudioParameterBool>(
            ParamID::Global::NoiseEnable, "Noise Enable", false));
            
        // Add operator 1 parameters for CC testing
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::tl(1), "Op1 TL", 0, 127, 0));
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::ar(1), "Op1 AR", 0, 31, 31));
            
        // Add channel pan parameters
        for (int ch = 0; ch < 8; ++ch) {
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                ParamID::Channel::pan(ch), "Ch" + juce::String(ch) + " Pan", 0.0f, 1.0f, 0.5f));
        }
        
        // Create parameter tree with dummy processor
        dummyProcessor = std::make_unique<DummyAudioProcessor>();
        parameters = std::make_unique<juce::AudioProcessorValueTreeState>(*dummyProcessor, nullptr, "TEST", std::move(layout));
        
        // Create mocks
        mockVoiceManager = std::make_unique<MockVoiceManager>();
        mockYmfmWrapper = std::make_unique<MockYmfmWrapper>();
        
        // Create MidiProcessor with mocks
        midiProcessor = std::make_unique<MidiProcessor>(*mockVoiceManager, *mockYmfmWrapper, *parameters);
    }
    
    void TearDown() override {
        midiProcessor.reset();
        mockYmfmWrapper.reset();
        mockVoiceManager.reset();
        parameters.reset();
        dummyProcessor.reset();
    }
    
    // Helper method to create MIDI messages
    juce::MidiMessage createNoteOnMessage(int note, int velocity) {
        return juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(velocity));
    }
    
    juce::MidiMessage createNoteOffMessage(int note) {
        return juce::MidiMessage::noteOff(1, note);
    }
    
    juce::MidiMessage createCCMessage(int ccNumber, int value) {
        return juce::MidiMessage::controllerEvent(1, ccNumber, value);
    }
    
    juce::MidiMessage createPitchBendMessage(int value) {
        return juce::MidiMessage::pitchWheel(1, value);
    }
    
protected:
    std::unique_ptr<DummyAudioProcessor> dummyProcessor;
    std::unique_ptr<juce::AudioProcessorValueTreeState> parameters;
    std::unique_ptr<MockVoiceManager> mockVoiceManager;
    std::unique_ptr<MockYmfmWrapper> mockYmfmWrapper;
    std::unique_ptr<MidiProcessor> midiProcessor;
};

// Test MIDI Note On handling
TEST_F(MidiProcessorTest, ProcessMidiNoteOn) {
    const int testNote = 60; // Middle C
    const int testVelocity = 100;
    const int allocatedChannel = 0;
    
    // Set up expectations
    EXPECT_CALL(*mockVoiceManager, allocateVoiceWithNoisePriority(testNote, testVelocity, false))
        .WillOnce(Return(allocatedChannel));
    EXPECT_CALL(*mockYmfmWrapper, setChannelPan(allocatedChannel, _));
    EXPECT_CALL(*mockYmfmWrapper, noteOn(allocatedChannel, testNote, testVelocity));
    
    // Create and process note on message
    auto message = createNoteOnMessage(testNote, testVelocity);
    midiProcessor->processMidiNoteOn(message);
}

// Test MIDI Note Off handling
TEST_F(MidiProcessorTest, ProcessMidiNoteOff) {
    const int testNote = 60;
    const int playingChannel = 2;
    
    // Set up expectations
    EXPECT_CALL(*mockVoiceManager, getChannelForNote(testNote))
        .WillOnce(Return(playingChannel));
    EXPECT_CALL(*mockYmfmWrapper, noteOff(playingChannel, testNote));
    EXPECT_CALL(*mockVoiceManager, releaseVoice(testNote));
    
    // Create and process note off message
    auto message = createNoteOffMessage(testNote);
    midiProcessor->processMidiNoteOff(message);
}

// Test MIDI Note Off when note not found
TEST_F(MidiProcessorTest, ProcessMidiNoteOffNoteNotFound) {
    const int testNote = 60;
    
    // Set up expectations - note not found
    EXPECT_CALL(*mockVoiceManager, getChannelForNote(testNote))
        .WillOnce(Return(-1));
    // No calls to ymfmWrapper or releaseVoice should be made
    
    // Create and process note off message
    auto message = createNoteOffMessage(testNote);
    midiProcessor->processMidiNoteOff(message);
}

// Test MIDI CC to Algorithm parameter mapping
TEST_F(MidiProcessorTest, HandleMidiCCAlgorithm) {
    const int algorithmCC = ParamID::MIDI_CC::Algorithm;
    const int ccValue = 64; // Mid-range CC value
    
    // Get algorithm parameter and verify initial state
    auto* algorithmParam = parameters->getParameter(ParamID::Global::Algorithm);
    ASSERT_NE(algorithmParam, nullptr);
    
    // Process CC message
    midiProcessor->handleMidiCC(algorithmCC, ccValue);
    
    // Verify parameter was updated
    // CC value 64 should map to normalized value ~0.5 (64/127)
    float expectedNormalized = 64.0f / 127.0f;
    EXPECT_NEAR(algorithmParam->getValue(), expectedNormalized, 0.01f);
}

// Test MIDI CC to Operator parameter mapping
TEST_F(MidiProcessorTest, HandleMidiCCOperatorParameter) {
    const int op1TlCC = ParamID::MIDI_CC::Op1_TL;
    const int ccValue = 100;
    
    // Get operator 1 TL parameter
    auto* tlParam = parameters->getParameter(ParamID::Op::tl(1));
    ASSERT_NE(tlParam, nullptr);
    
    // Process CC message
    midiProcessor->handleMidiCC(op1TlCC, ccValue);
    
    // Verify parameter was updated
    float expectedNormalized = 100.0f / 127.0f;
    EXPECT_NEAR(tlParam->getValue(), expectedNormalized, 0.01f);
}

// Test MIDI CC channel pan mapping
TEST_F(MidiProcessorTest, HandleMidiCCChannelPan) {
    const int channel = 3;
    const int channelPanCC = ParamID::MIDI_CC::Ch0_Pan + channel;
    const int ccValue = 32; // Left pan
    
    // Get channel pan parameter
    auto* panParam = parameters->getParameter(ParamID::Channel::pan(channel));
    ASSERT_NE(panParam, nullptr);
    
    // Process CC message
    midiProcessor->handleMidiCC(channelPanCC, ccValue);
    
    // Verify parameter was updated
    float expectedNormalized = 32.0f / 127.0f;
    EXPECT_NEAR(panParam->getValue(), expectedNormalized, 0.01f);
}

// Test pitch bend handling
TEST_F(MidiProcessorTest, HandlePitchBend) {
    const int pitchBendValue = 10000; // Above center (8192)
    const int testChannel = 1;
    const uint8_t testNote = 60;
    const uint8_t testVelocity = 100;
    
    // Set pitch bend range to 2 semitones (default)
    auto* pitchBendRangeParam = parameters->getParameter(ParamID::Global::PitchBendRange);
    pitchBendRangeParam->setValueNotifyingHost(2.0f / 12.0f); // 2 semitones normalized
    
    // Set up active voice
    EXPECT_CALL(*mockVoiceManager, isVoiceActive(testChannel))
        .WillOnce(Return(true));
    EXPECT_CALL(*mockVoiceManager, getNoteForChannel(testChannel))
        .WillOnce(Return(testNote));
    EXPECT_CALL(*mockVoiceManager, getVelocityForChannel(testChannel))
        .WillOnce(Return(testVelocity));
    
    // Expect pitch bend to be applied
    EXPECT_CALL(*mockYmfmWrapper, setPitchBend(testChannel, _));
    
    // Process pitch bend message
    midiProcessor->handlePitchBend(pitchBendValue);
}

// Test multiple MIDI messages in buffer
TEST_F(MidiProcessorTest, ProcessMidiMessagesBuffer) {
    juce::MidiBuffer midiBuffer;
    
    // Add multiple MIDI messages
    midiBuffer.addEvent(createNoteOnMessage(60, 100), 0);
    midiBuffer.addEvent(createCCMessage(ParamID::MIDI_CC::Algorithm, 64), 10);
    midiBuffer.addEvent(createNoteOffMessage(60), 20);
    
    // Set up expectations for note on
    EXPECT_CALL(*mockVoiceManager, allocateVoiceWithNoisePriority(60, 100, false))
        .WillOnce(Return(0));
    EXPECT_CALL(*mockYmfmWrapper, setChannelPan(0, _));
    EXPECT_CALL(*mockYmfmWrapper, noteOn(0, 60, 100));
    
    // Set up expectations for note off
    EXPECT_CALL(*mockVoiceManager, getChannelForNote(60))
        .WillOnce(Return(0));
    EXPECT_CALL(*mockYmfmWrapper, noteOff(0, 60));
    EXPECT_CALL(*mockVoiceManager, releaseVoice(60));
    
    // Process all MIDI messages
    midiProcessor->processMidiMessages(midiBuffer);
    
    // Verify algorithm parameter was updated by CC message
    auto* algorithmParam = parameters->getParameter(ParamID::Global::Algorithm);
    EXPECT_NEAR(algorithmParam->getValue(), 64.0f / 127.0f, 0.01f);
}

// Test random pan functionality
TEST_F(MidiProcessorTest, ApplyRandomPan) {
    const int testChannel = 2;
    
    // Set global pan to RANDOM mode
    auto* globalPanParam = static_cast<juce::AudioParameterChoice*>(parameters->getParameter(ParamID::Global::GlobalPan));
    globalPanParam->setValueNotifyingHost(static_cast<float>(GlobalPanPosition::RANDOM) / 3.0f);
    
    // Expect pan to be applied (actual value depends on random generation)
    EXPECT_CALL(*mockYmfmWrapper, setChannelPan(testChannel, _));
    
    // Apply random pan
    midiProcessor->setChannelRandomPan(testChannel);
    midiProcessor->applyGlobalPan(testChannel);
}

// Test noise priority voice allocation
TEST_F(MidiProcessorTest, NoisePriorityVoiceAllocation) {
    const int testNote = 60;
    const int testVelocity = 100;
    const int allocatedChannel = 0;
    
    // Enable noise in current preset
    auto* noiseParam = parameters->getParameter(ParamID::Global::NoiseEnable);
    noiseParam->setValueNotifyingHost(1.0f); // Enable noise
    
    // Set up expectations for noise-priority allocation
    EXPECT_CALL(*mockVoiceManager, allocateVoiceWithNoisePriority(testNote, testVelocity, true))
        .WillOnce(Return(allocatedChannel));
    EXPECT_CALL(*mockYmfmWrapper, setChannelPan(allocatedChannel, _));
    EXPECT_CALL(*mockYmfmWrapper, noteOn(allocatedChannel, testNote, testVelocity));
    
    // Create and process note on message
    auto message = createNoteOnMessage(testNote, testVelocity);
    midiProcessor->processMidiNoteOn(message);
}

// Test CC mapping setup
TEST_F(MidiProcessorTest, CCMappingSetup) {
    // CC mapping is set up in constructor, verify by testing a few key mappings
    
    // Test algorithm CC
    midiProcessor->handleMidiCC(ParamID::MIDI_CC::Algorithm, 127);
    auto* algorithmParam = parameters->getParameter(ParamID::Global::Algorithm);
    EXPECT_NEAR(algorithmParam->getValue(), 1.0f, 0.01f);
    
    // Test feedback CC
    midiProcessor->handleMidiCC(ParamID::MIDI_CC::Feedback, 63);
    auto* feedbackParam = parameters->getParameter(ParamID::Global::Feedback);
    EXPECT_NEAR(feedbackParam->getValue(), 63.0f / 127.0f, 0.01f);
    
    // Test operator 1 attack rate CC
    midiProcessor->handleMidiCC(ParamID::MIDI_CC::Op1_AR, 80);
    auto* arParam = parameters->getParameter(ParamID::Op::ar(1));
    EXPECT_NEAR(arParam->getValue(), 80.0f / 127.0f, 0.01f);
}