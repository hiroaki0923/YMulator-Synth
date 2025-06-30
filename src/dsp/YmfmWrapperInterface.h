#pragma once

#include <cstdint>
#include <array>

/**
 * Interface for FM synthesis wrapper
 * Enables dependency injection and mocking for tests
 */
class YmfmWrapperInterface
{
public:
    virtual ~YmfmWrapperInterface() = default;
    
    enum class ChipType {
        OPM,   // YM2151
        OPNA   // YM2608
    };
    
    // Initialization and lifecycle
    virtual void initialize(ChipType type, uint32_t outputSampleRate) = 0;
    virtual void reset() = 0;
    virtual bool isInitialized() const = 0;
    
    // Audio generation
    virtual void generateSamples(float* leftBuffer, float* rightBuffer, int numSamples) = 0;
    
    // MIDI interface
    virtual void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) = 0;
    virtual void noteOff(uint8_t channel, uint8_t note) = 0;
    
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
        KeyScale,
        AmsEnable
    };
    
    enum class ChannelParameter {
        Algorithm,
        Feedback,
        Pan,
        AMS,
        PMS
    };
    
    // Parameter control
    virtual void setOperatorParameter(uint8_t channel, uint8_t operator_num, 
                                    OperatorParameter param, uint8_t value) = 0;
    virtual void setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value) = 0;
    virtual void setAlgorithm(uint8_t channel, uint8_t algorithm) = 0;
    virtual void setFeedback(uint8_t channel, uint8_t feedback) = 0;
    
    // Advanced features
    virtual void setPitchBend(uint8_t channel, float semitones) = 0;
    virtual void setChannelPan(uint8_t channel, float panValue) = 0;
    virtual void setLfoParameters(uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform) = 0;
    virtual void setChannelAmsPms(uint8_t channel, uint8_t ams, uint8_t pms) = 0;
    virtual void setOperatorAmsEnable(uint8_t channel, uint8_t operator_num, bool enable) = 0;
    
    // Envelope and parameter convenience methods
    virtual void setOperatorParameters(uint8_t channel, uint8_t operator_num, 
                                     uint8_t tl, uint8_t ar, uint8_t d1r, uint8_t d2r, 
                                     uint8_t rr, uint8_t d1l, uint8_t ks, uint8_t mul, uint8_t dt1, uint8_t dt2) = 0;
    virtual void setOperatorEnvelope(uint8_t channel, uint8_t operator_num, 
                                   uint8_t ar, uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l) = 0;
    
    // Velocity and dynamics
    virtual void setVelocitySensitivity(uint8_t channel, uint8_t operator_num, float sensitivity) = 0;
    virtual void applyVelocityToChannel(uint8_t channel, uint8_t velocity) = 0;
    
    // Noise generator control
    virtual void setNoiseEnable(bool enable) = 0;
    virtual void setNoiseFrequency(uint8_t frequency) = 0;
    virtual bool getNoiseEnable() const = 0;
    virtual uint8_t getNoiseFrequency() const = 0;
    virtual void setNoiseParameters(bool enable, uint8_t frequency) = 0;
    virtual void testNoiseChannel() = 0;
    
    // Register access
    virtual void writeRegister(int address, uint8_t data) = 0;
    virtual uint8_t readCurrentRegister(int address) const = 0;
    
    // Batch operations for efficiency
    virtual void batchUpdateChannelParameters(uint8_t channel, uint8_t algorithm, uint8_t feedback,
                                            const std::array<std::array<uint8_t, 10>, 4>& operatorParams) = 0;
    
    // Debug and monitoring
    struct EnvelopeDebugInfo {
        uint32_t currentState;     // Current envelope state (attack, decay, sustain, release)
        uint32_t currentLevel;     // Current attenuation level
        uint32_t effectiveRate;    // Effective rate with KSR applied
        bool isActive;             // Whether envelope is actively changing
    };
    
    virtual EnvelopeDebugInfo getEnvelopeDebugInfo(uint8_t channel, uint8_t operator_num) const = 0;
};