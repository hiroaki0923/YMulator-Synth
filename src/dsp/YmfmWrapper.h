#pragma once

#include "ymfm_opm.h"
#include "ymfm_opn.h"
#include <array>
#include <cstdint>
#include <memory>

// Minimal ymfm wrapper for YMulator Synth
class YmfmWrapper : public ymfm::ymfm_interface
{
public:
    enum class ChipType {
        OPM,   // YM2151
        OPNA   // YM2608
    };

    YmfmWrapper();
    ~YmfmWrapper() = default;
    
    // Initialization
    void initialize(ChipType type, uint32_t outputSampleRate);
    void reset();
    bool isInitialized() const { return initialized; }
    
    // Register access
    void writeRegister(int address, uint8_t data);
    
    // Audio generation
    void generateSamples(float* leftBuffer, float* rightBuffer, int numSamples);
    
    // Simple note interface for testing
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t note);
    
    // Parameter control enums
    enum class OperatorParameter {
        TotalLevel,
        AttackRate,
        Decay1Rate,
        Decay2Rate,
        ReleaseRate,
        SustainLevel,
        Multiple,
        Detune1,
        Detune2,
        KeyScale
    };
    
    enum class ChannelParameter {
        Algorithm,
        Feedback
    };
    
    // Parameter control methods
    void setOperatorParameter(uint8_t channel, uint8_t operator_num, OperatorParameter param, uint8_t value);
    void setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value);
    
    // Convenience methods for parameter updates
    void setAlgorithm(uint8_t channel, uint8_t algorithm);
    void setFeedback(uint8_t channel, uint8_t feedback);
    void setOperatorParameters(uint8_t channel, uint8_t operator_num, 
                              uint8_t tl, uint8_t ar, uint8_t d1r, uint8_t d2r, 
                              uint8_t rr, uint8_t d1l, uint8_t ks, uint8_t mul, uint8_t dt1, uint8_t dt2);
    
    // Envelope optimization methods
    void setOperatorEnvelope(uint8_t channel, uint8_t operator_num, 
                            uint8_t ar, uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l);
    void batchUpdateChannelParameters(uint8_t channel, uint8_t algorithm, uint8_t feedback,
                                     const std::array<std::array<uint8_t, 10>, 4>& operatorParams);
    
    // Pitch bend support
    void setPitchBend(uint8_t channel, float semitones);
    
    // Pan control support
    void setChannelPan(uint8_t channel, float panValue);
    
    // LFO support
    void setLfoParameters(uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform);
    void setChannelAmsPms(uint8_t channel, uint8_t ams, uint8_t pms);
    void setOperatorAmsEnable(uint8_t channel, uint8_t operator_num, bool enable);
    
    // Envelope monitoring and debugging
    struct EnvelopeDebugInfo {
        uint32_t currentState;     // Current envelope state (attack, decay, sustain, release)
        uint32_t currentLevel;     // Current attenuation level
        uint32_t effectiveRate;    // Effective rate with KSR applied
        bool isActive;             // Whether envelope is actively changing
    };
    
    EnvelopeDebugInfo getEnvelopeDebugInfo(uint8_t channel, uint8_t operator_num) const;
    void setVelocitySensitivity(uint8_t channel, uint8_t operator_num, float sensitivity);
    void applyVelocityToChannel(uint8_t channel, uint8_t velocity);
    
    // Noise generator control
    void setNoiseEnable(bool enable);
    void setNoiseFrequency(uint8_t frequency);
    bool getNoiseEnable() const;
    uint8_t getNoiseFrequency() const;
    void setNoiseParameters(bool enable, uint8_t frequency);
    void testNoiseChannel();  // Test method to verify noise functionality
    
    // Register access for global pan functionality
    uint8_t readCurrentRegister(int address) const;
    
    // ymfm_interface overrides
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override 
    { 
        return 0; 
    }
    
    void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override 
    {
        // Not used for OPM/OPNA
    }
    
private:
    ChipType chipType;
    uint32_t outputSampleRate;
    uint32_t internalSampleRate;
    bool initialized = false;
    
    // ymfm interface - no longer needed since we inherit from ymfm_interface
    // YMulatorSynthInterface interface;
    
    // ymfm chip instances
    std::unique_ptr<ymfm::ym2151> opmChip;
    std::unique_ptr<ymfm::ym2608> opnaChip;
    
    // Output data holders
    ymfm::ym2151::output_data opmOutput;
    ymfm::ym2608::output_data opnaOutput;
    
    // Current register values (for read-modify-write operations)
    uint8_t currentRegisters[256];
    
    // Pitch bend state per channel
    struct ChannelState {
        uint8_t baseNote = 0;      // Original MIDI note
        float pitchBend = 0.0f;    // Current pitch bend in semitones
        bool active = false;       // Is this channel playing a note
    };
    std::array<ChannelState, 8> channelStates;
    
    // Velocity sensitivity per operator (32 operators total: 8 channels × 4 operators)
    std::array<std::array<float, 4>, 8> velocitySensitivity;
    
    // Helper methods
    void initializeOPM();
    void initializeOPNA();
    uint16_t noteToFnum(uint8_t note);
    uint16_t noteToFnumWithPitchBend(uint8_t note, float pitchBendSemitones);
    void setupBasicPianoVoice(uint8_t channel);
    void playTestNote();
    void updateRegisterCache(uint8_t address, uint8_t value);
};