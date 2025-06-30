#pragma once

#include "YmfmWrapperInterface.h"
#include "ymfm_opm.h"
#include "ymfm_opn.h"
#include <array>
#include <cstdint>
#include <memory>

// Minimal ymfm wrapper for YMulator Synth
class YmfmWrapper : public YmfmWrapperInterface, public ymfm::ymfm_interface
{
public:
    YmfmWrapper();
    ~YmfmWrapper() override = default;
    
    // Initialization and lifecycle - interface implementation
    void initialize(ChipType type, uint32_t outputSampleRate) override;
    void reset() override;
    bool isInitialized() const override { return initialized; }
    
    // Audio generation - interface implementation
    void generateSamples(float* leftBuffer, float* rightBuffer, int numSamples) override;
    
    // MIDI interface - interface implementation
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void noteOff(uint8_t channel, uint8_t note) override;
    
    // Parameter control methods - interface implementation
    void setOperatorParameter(uint8_t channel, uint8_t operator_num, OperatorParameter param, uint8_t value) override;
    void setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value) override;
    void setAlgorithm(uint8_t channel, uint8_t algorithm) override;
    void setFeedback(uint8_t channel, uint8_t feedback) override;
    
    // Advanced features - interface implementation
    void setPitchBend(uint8_t channel, float semitones) override;
    void setChannelPan(uint8_t channel, float panValue) override;
    void setLfoParameters(uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform) override;
    void setChannelAmsPms(uint8_t channel, uint8_t ams, uint8_t pms) override;
    void setOperatorAmsEnable(uint8_t channel, uint8_t operator_num, bool enable) override;
    
    // Envelope and parameter convenience methods - interface implementation
    void setOperatorParameters(uint8_t channel, uint8_t operator_num, 
                              uint8_t tl, uint8_t ar, uint8_t d1r, uint8_t d2r, 
                              uint8_t rr, uint8_t d1l, uint8_t ks, uint8_t mul, uint8_t dt1, uint8_t dt2) override;
    void setOperatorEnvelope(uint8_t channel, uint8_t operator_num, 
                            uint8_t ar, uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l) override;
    
    // Velocity and dynamics - interface implementation
    void setVelocitySensitivity(uint8_t channel, uint8_t operator_num, float sensitivity) override;
    void applyVelocityToChannel(uint8_t channel, uint8_t velocity) override;
    
    // Noise generator control - interface implementation
    void setNoiseEnable(bool enable) override;
    void setNoiseFrequency(uint8_t frequency) override;
    bool getNoiseEnable() const override;
    uint8_t getNoiseFrequency() const override;
    void setNoiseParameters(bool enable, uint8_t frequency) override;
    void testNoiseChannel() override;
    
    // Register access - interface implementation
    void writeRegister(int address, uint8_t data) override;
    uint8_t readCurrentRegister(int address) const override;
    
    // Batch operations for efficiency - interface implementation
    void batchUpdateChannelParameters(uint8_t channel, uint8_t algorithm, uint8_t feedback,
                                     const std::array<std::array<uint8_t, 10>, 4>& operatorParams) override;
    
    // Debug and monitoring - interface implementation
    EnvelopeDebugInfo getEnvelopeDebugInfo(uint8_t channel, uint8_t operator_num) const override;
    
    // ymfm_interface overrides
    uint8_t ymfm_external_read([[maybe_unused]] ymfm::access_class type, [[maybe_unused]] uint32_t address) override 
    { 
        return 0; 
    }
    
    void ymfm_external_write([[maybe_unused]] ymfm::access_class type, [[maybe_unused]] uint32_t address, [[maybe_unused]] uint8_t data) override 
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