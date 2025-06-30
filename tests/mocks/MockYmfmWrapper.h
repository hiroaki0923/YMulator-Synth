#pragma once

#include "../../src/dsp/YmfmWrapperInterface.h"
#include <gmock/gmock.h>
#include <vector>
#include <cstring>

/**
 * MockYmfmWrapper - Mock implementation of YmfmWrapperInterface for testing
 * 
 * This mock allows testing of components that depend on YmfmWrapper without
 * requiring actual FM synthesis computation.
 */
class MockYmfmWrapper : public YmfmWrapperInterface {
public:
    MockYmfmWrapper() {
        // Setup default behavior for common methods
        ON_CALL(*this, isInitialized()).WillByDefault(testing::Return(true));
        
        // Clear register cache
        std::memset(registerCache, 0, sizeof(registerCache));
    }
    
    // Initialization and lifecycle
    MOCK_METHOD(void, initialize, (ChipType type, uint32_t outputSampleRate), (override));
    MOCK_METHOD(void, reset, (), (override));
    MOCK_METHOD(bool, isInitialized, (), (const, override));
    
    // Audio generation - provide default implementation
    void generateSamples(float* leftBuffer, float* rightBuffer, int numSamples) override {
        generateSamplesCalled++;
        lastNumSamples = numSamples;
        
        // Generate simple test signal if notes are on
        if (!activeNotes.empty()) {
            float phase = 0.0f;
            float phaseInc = 440.0f / 44100.0f * 2.0f * 3.14159f;  // 440Hz test tone
            
            for (int i = 0; i < numSamples; ++i) {
                float sample = std::sin(phase) * 0.1f;  // Low volume sine wave
                leftBuffer[i] = sample;
                if (rightBuffer) {
                    rightBuffer[i] = sample;
                }
                phase += phaseInc;
            }
        } else {
            // Silence
            std::memset(leftBuffer, 0, numSamples * sizeof(float));
            if (rightBuffer) {
                std::memset(rightBuffer, 0, numSamples * sizeof(float));
            }
        }
    }
    
    // MIDI interface
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        noteOnCalls.push_back({channel, note, velocity});
        activeNotes.push_back({channel, note});
    }
    
    void noteOff(uint8_t channel, uint8_t note) override {
        noteOffCalls.push_back({channel, note});
        
        // Remove from active notes
        activeNotes.erase(
            std::remove_if(activeNotes.begin(), activeNotes.end(),
                          [channel, note](const auto& n) {
                              return n.channel == channel && n.note == note;
                          }),
            activeNotes.end()
        );
    }
    
    // Parameter control mocks
    MOCK_METHOD(void, setOperatorParameter, 
                (uint8_t channel, uint8_t operator_num, OperatorParameter param, uint8_t value), 
                (override));
    MOCK_METHOD(void, setChannelParameter, 
                (uint8_t channel, ChannelParameter param, uint8_t value), 
                (override));
    MOCK_METHOD(void, setAlgorithm, (uint8_t channel, uint8_t algorithm), (override));
    MOCK_METHOD(void, setFeedback, (uint8_t channel, uint8_t feedback), (override));
    
    // Advanced features
    MOCK_METHOD(void, setPitchBend, (uint8_t channel, float semitones), (override));
    MOCK_METHOD(void, setChannelPan, (uint8_t channel, float panValue), (override));
    MOCK_METHOD(void, setLfoParameters, 
                (uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform), (override));
    MOCK_METHOD(void, setChannelAmsPms, (uint8_t channel, uint8_t ams, uint8_t pms), (override));
    MOCK_METHOD(void, setOperatorAmsEnable, 
                (uint8_t channel, uint8_t operator_num, bool enable), (override));
    
    // Envelope methods
    MOCK_METHOD(void, setOperatorParameters, 
                (uint8_t channel, uint8_t operator_num, uint8_t tl, uint8_t ar, 
                 uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l, uint8_t ks, 
                 uint8_t mul, uint8_t dt1, uint8_t dt2), (override));
    MOCK_METHOD(void, setOperatorEnvelope, 
                (uint8_t channel, uint8_t operator_num, uint8_t ar, uint8_t d1r, 
                 uint8_t d2r, uint8_t rr, uint8_t d1l), (override));
    
    // Velocity
    MOCK_METHOD(void, setVelocitySensitivity, 
                (uint8_t channel, uint8_t operator_num, float sensitivity), (override));
    MOCK_METHOD(void, applyVelocityToChannel, (uint8_t channel, uint8_t velocity), (override));
    
    // Noise
    MOCK_METHOD(void, setNoiseEnable, (bool enable), (override));
    MOCK_METHOD(void, setNoiseFrequency, (uint8_t frequency), (override));
    MOCK_METHOD(bool, getNoiseEnable, (), (const, override));
    MOCK_METHOD(uint8_t, getNoiseFrequency, (), (const, override));
    MOCK_METHOD(void, setNoiseParameters, (bool enable, uint8_t frequency), (override));
    MOCK_METHOD(void, testNoiseChannel, (), (override));
    
    // Register access
    void writeRegister(int address, uint8_t data) override {
        registerWrites.push_back({address, data});
        if (address >= 0 && address < 256) {
            registerCache[address] = data;
        }
    }
    
    uint8_t readCurrentRegister(int address) const override {
        if (address >= 0 && address < 256) {
            return registerCache[address];
        }
        return 0;
    }
    
    // Batch operations
    MOCK_METHOD4(batchUpdateChannelParameters, 
                 void(uint8_t channel, uint8_t algorithm, uint8_t feedback,
                      const std::array<std::array<uint8_t, 10>, 4>& operatorParams));
    
    // Debug
    MOCK_METHOD(EnvelopeDebugInfo, getEnvelopeDebugInfo, 
                (uint8_t channel, uint8_t operator_num), (const, override));
    
    // Test helpers - track calls for verification
    struct NoteOnCall {
        uint8_t channel;
        uint8_t note;
        uint8_t velocity;
    };
    
    struct NoteOffCall {
        uint8_t channel;
        uint8_t note;
    };
    
    struct RegisterWrite {
        int address;
        uint8_t data;
    };
    
    struct ActiveNote {
        uint8_t channel;
        uint8_t note;
    };
    
    // Call tracking
    std::vector<NoteOnCall> noteOnCalls;
    std::vector<NoteOffCall> noteOffCalls;
    std::vector<RegisterWrite> registerWrites;
    std::vector<ActiveNote> activeNotes;
    
    int generateSamplesCalled = 0;
    int lastNumSamples = 0;
    
    // Register cache for read operations
    mutable uint8_t registerCache[256];
    
    // Clear all tracking data
    void clearCallHistory() {
        noteOnCalls.clear();
        noteOffCalls.clear();
        registerWrites.clear();
        activeNotes.clear();
        generateSamplesCalled = 0;
        lastNumSamples = 0;
    }
};