#pragma once

#include "YmfmWrapperInterface.h"
#include "YM2151Registers.h"
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
    void setChannelSlotMask(uint8_t channel, uint8_t voiceOrderMask) override;
    
    // Parameter control methods - interface implementation
    void setOperatorParameter(uint8_t channel, uint8_t operator_num, OperatorParameter param, uint8_t value) override;
    void setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value) override;
    void setAlgorithm(uint8_t channel, uint8_t algorithm) override;
    void setFeedback(uint8_t channel, uint8_t feedback) override;
    
    // Advanced features - interface implementation
    void setPitchBend(uint8_t channel, float semitones) override;
    void setChannelPitchOffset(uint8_t channel, float semitones) override;
    void setChannelLevelMotion(uint8_t channel, int carrierSteps, int modulatorSteps) override;
    void setWide(bool enabled, float detuneCents, WidePan pan) override;
    bool isWideEnabled() const override { return wideEnabled; }
    uint8_t readShadowRegister(int address) const override { return shadowRegisters[static_cast<uint8_t>(address)]; }
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
    uint64_t getRegisterWriteCount() const override { return registerWriteCount; }
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
    uint64_t registerWriteCount = 0;
    
    // ymfm interface - no longer needed since we inherit from ymfm_interface
    // YMulatorSynthInterface interface;
    
    // ymfm chip instances
    std::unique_ptr<ymfm::ym2151> opmChip;
    // Second OPM that mirrors every register write; sounds only while Wide is on
    std::unique_ptr<ymfm::ym2151> shadowChip;
    ymfm::ym2151::output_data shadowOutput;
    uint8_t shadowRegisters[256] = {};
    bool wideEnabled = false;
    float wideDetuneSemitones = 0.0f;
    WidePan widePan = WidePan::LeftRight;
    std::unique_ptr<ymfm::ym2608> opnaChip;
    
    // Output data holders
    ymfm::ym2151::output_data opmOutput;
    ymfm::ym2608::output_data opnaOutput;

    // Resampler state. ymfm renders at the chip's native rate (clock / 64, about
    // 55.9 kHz for OPM); output is interpolated to the host rate with 4-point
    // cubic Hermite interpolation between history[1] and history[2].
    double resampleStep = 1.0;    // native samples per output sample
    double resamplePhase = 1.0;   // fractional position between history[1] and history[2]
    std::array<float, 4> historyLeft {};
    std::array<float, 4> historyRight {};
    
    // Current register values (for read-modify-write operations)
    uint8_t currentRegisters[256];
    
    // Pitch bend state per channel
    struct ChannelState {
        uint8_t baseNote = 0;      // Original MIDI note
        float pitchBend = 0.0f;    // Current pitch bend in semitones
        float motionOffset = 0.0f; // Vibrato / pitch envelope, semitones
        bool active = false;       // Is this channel playing a note
        uint8_t slotMask = YM2151Regs::MASK_SLOT_ENABLE;  // Operators keyed on, voice order
    };
    std::array<ChannelState, 8> channelStates;
    
    // TL as set by the parameters, and the velocity attenuation of the note on each channel.
    // Carriers are written as base + attenuation so velocity survives parameter rewrites.
    std::array<std::array<uint8_t, 4>, 8> baseTotalLevel {};
    std::array<uint8_t, 8> velocityAttenuation {};
    std::array<int, 8> carrierMotion {};      // tremolo, TL steps
    std::array<int, 8> modulatorMotion {};    // timbre LFO, TL steps (may be negative)
    bool isCarrier(uint8_t channel, uint8_t operator_num) const;
    void writeTotalLevel(uint8_t channel, uint8_t operator_num);
    
    // Helper methods
    void initializeOPM();
    void initializeOPNA();
    uint16_t noteToFnum(uint8_t note);
    uint16_t noteToFnumWithPitchBend(uint8_t note, float pitchBendSemitones);
    void writePitch(uint8_t channel);   // KC/KF from base note + bend + motion offset
    void writeShadow(uint8_t address, uint8_t data);
    uint8_t panForChip(uint8_t address, uint8_t data, bool shadow) const;
    void refreshPansForWide();
    void setupBasicPianoVoice(uint8_t channel);
    void playTestNote();
    void updateRegisterCache(uint8_t address, uint8_t value);
    void resetResampler();
    void renderNativeSample();
};