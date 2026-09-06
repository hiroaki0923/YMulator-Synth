#include "YmfmWrapper.h"
#include "YM2151Registers.h"
#include "utils/Debug.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <cmath>
#include <iostream>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

YmfmWrapper::YmfmWrapper()
    : chipType(ChipType::OPM)
    , outputSampleRate(44100)
    , internalSampleRate(44100)
{
    // Initialize register cache
    std::memset(currentRegisters, 0, sizeof(currentRegisters));
    
    // Initialize velocity sensitivity to default values (1.0 = no scaling)
}

void YmfmWrapper::initialize(ChipType type, uint32_t outputSampleRate)
{
    CS_ASSERT_SAMPLE_RATE(outputSampleRate);
    CS_FILE_DBG("=== YmfmWrapper::initialize ===");
    CS_FILE_DBG("ChipType: " + juce::String(type == ChipType::OPM ? "OPM" : "OPNA"));
    CS_FILE_DBG("Received outputSampleRate: " + juce::String(outputSampleRate));
    
    chipType = type;
    this->outputSampleRate = outputSampleRate;
    
    if (type == ChipType::OPM) {
        initializeOPM();
        internalSampleRate = opmChip->sample_rate(YM2151Regs::OPM_DEFAULT_CLOCK);
    } else {
        initializeOPNA();
        internalSampleRate = opnaChip->sample_rate(YM2151Regs::OPNA_DEFAULT_CLOCK);
    }
    
    resetResampler();
    CS_FILE_DBG("Chip native rate=" + juce::String(internalSampleRate) + " Hz, resample step=" + juce::String(resampleStep, 6));
    
    initialized = true;
}

void YmfmWrapper::reset()
{
    // The chip forgets everything on reset; so must the register cache and the per-channel state
    std::memset(currentRegisters, 0, sizeof(currentRegisters));
    std::memset(shadowRegisters, 0, sizeof(shadowRegisters));
    for (auto& state : channelStates) state = ChannelState {};
    velocityAttenuation.fill(0);
    carrierMotion.fill(0);
    modulatorMotion.fill(0);
    if (shadowChip) shadowChip->reset();
    if (chipType == ChipType::OPM && opmChip) {
        opmChip->reset();
        initializeOPM();
    } else if (chipType == ChipType::OPNA && opnaChip) {
        opnaChip->reset();
        initializeOPNA();
    }
    resetResampler();
}

void YmfmWrapper::resetResampler()
{
    resampleStep = (outputSampleRate > 0)
        ? static_cast<double>(internalSampleRate) / static_cast<double>(outputSampleRate)
        : 1.0;
    resamplePhase = 1.0;  // Pull a fresh native sample on the first output sample
    historyLeft.fill(0.0f);
    historyRight.fill(0.0f);
}

void YmfmWrapper::renderNativeSample()
{
    constexpr float scaleFactor = 1.0f / YM2151Regs::SAMPLE_SCALE_FACTOR;
    float left = 0.0f;
    float right = 0.0f;
    
    if (chipType == ChipType::OPM && opmChip) {
        opmChip->generate(&opmOutput, 1);
        // ymfm output: data[0] = left, data[1] = right (NOT interleaved)
        left = static_cast<float>(opmOutput.data[0]) * scaleFactor;
        right = static_cast<float>(opmOutput.data[1]) * scaleFactor;
        if (wideEnabled && shadowChip) {
            shadowChip->generate(&shadowOutput, 1);
            // Panned apart each side carries one chip; centred, both share each side at -3 dB
            const float mix = widePan == WidePan::Centre ? 0.70710678f : 1.0f;
            left = left * mix + static_cast<float>(shadowOutput.data[0]) * scaleFactor * mix;
            right = right * mix + static_cast<float>(shadowOutput.data[1]) * scaleFactor * mix;
        }
    } else if (chipType == ChipType::OPNA && opnaChip) {
        opnaChip->generate(&opnaOutput, 1);
        left = static_cast<float>(opnaOutput.data[0]) * scaleFactor;
        right = static_cast<float>(opnaOutput.data[1]) * scaleFactor;
    }
    
    for (size_t i = 0; i + 1 < historyLeft.size(); ++i) {
        historyLeft[i] = historyLeft[i + 1];
        historyRight[i] = historyRight[i + 1];
    }
    historyLeft.back() = left;
    historyRight.back() = right;
}

void YmfmWrapper::initializeOPM()
{
    // Creating OPM chip instance
    
    opmChip = std::make_unique<ymfm::ym2151>(*this);
    shadowChip = std::make_unique<ymfm::ym2151>(*this);
    
    // Resetting OPM chip
    opmChip->reset();
    shadowChip->reset();
    std::memset(shadowRegisters, 0, sizeof(shadowRegisters));
    
    // OPM chip reset complete, setting up voice
    
    // Setup basic piano voice on all 8 channels
    for (int channel = 0; channel < YM2151Regs::MAX_OPM_CHANNELS; ++channel) {
        setupBasicPianoVoice(channel);
    }
    
    // OPM initialization complete
}

void YmfmWrapper::initializeOPNA()
{
    opnaChip = std::make_unique<ymfm::ym2608>(*this);
    opnaChip->reset();
    
    // Enable extended mode (required for OPNA)
    writeRegister(YM2151Regs::REG_OPNA_MODE, YM2151Regs::OPNA_MODE_VALUE);
    
    // Setup basic piano voice on all 6 FM channels (OPNA has 6 FM channels)
    for (int channel = 0; channel < YM2151Regs::MAX_OPNA_FM_CHANNELS; ++channel) {
        setupBasicPianoVoice(channel);
    }
}

void YmfmWrapper::writeRegister(int address, uint8_t data)
{
    uint8_t addr = static_cast<uint8_t>(address);
    
    // Update register cache
    currentRegisters[addr] = data;
    ++registerWriteCount;
    
    if (chipType == ChipType::OPM && opmChip) {
        opmChip->write_address(addr);
        opmChip->write_data(panForChip(addr, data, false));
        // Pitch registers are written per chip by writePitch (the two chips are detuned apart)
        const bool pitchRegister = addr >= YM2151Regs::REG_KEY_CODE_BASE && addr < YM2151Regs::REG_KEY_FRACTION_BASE + YM2151Regs::MAX_OPM_CHANNELS;
        if (!pitchRegister) writeShadow(addr, panForChip(addr, data, true));
    } else if (chipType == ChipType::OPNA && opnaChip) {
        // OPNA register write debug output disabled
        
        opnaChip->write_address(addr);
        opnaChip->write_data(data);
    }
}

uint8_t YmfmWrapper::readCurrentRegister(int address) const
{
    return currentRegisters[static_cast<uint8_t>(address)];
}

void YmfmWrapper::updateRegisterCache(uint8_t address, uint8_t value)
{
    currentRegisters[address] = value;
}

void YmfmWrapper::writeShadow(uint8_t address, uint8_t data)
{
    shadowRegisters[address] = data;
    if (shadowChip) {
        shadowChip->write_address(address);
        shadowChip->write_data(data);
    }
}

uint8_t YmfmWrapper::panForChip(uint8_t address, uint8_t data, bool shadow) const
{
    // With Wide panned apart, the main chip takes the left and the shadow the right
    const bool panRegister = address >= YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE
                          && address < YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + YM2151Regs::MAX_OPM_CHANNELS;
    if (!panRegister || !wideEnabled || widePan != WidePan::LeftRight) return data;
    return static_cast<uint8_t>((data & ~YM2151Regs::MASK_PAN_LR) | (shadow ? YM2151Regs::PAN_RIGHT_ONLY : YM2151Regs::PAN_LEFT_ONLY));
}

void YmfmWrapper::refreshPansForWide()
{
    for (uint8_t ch = 0; ch < YM2151Regs::MAX_OPM_CHANNELS; ++ch) {
        const uint8_t addr = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + ch;
        const uint8_t data = currentRegisters[addr];
        if (opmChip) { opmChip->write_address(addr); opmChip->write_data(panForChip(addr, data, false)); }
        writeShadow(addr, panForChip(addr, data, true));
    }
}

void YmfmWrapper::setWide(bool enabled, float detuneCents, WidePan pan)
{
    const float detune = detuneCents / 100.0f;
    if (enabled == wideEnabled && detune == wideDetuneSemitones && pan == widePan) return;
    wideEnabled = enabled;
    wideDetuneSemitones = detune;
    widePan = pan;
    if (chipType != ChipType::OPM) return;
    refreshPansForWide();
    for (uint8_t ch = 0; ch < YM2151Regs::MAX_OPM_CHANNELS; ++ch)
        if (channelStates[ch].active) writePitch(ch);
}

void YmfmWrapper::generateSamples(float* leftBuffer, float* rightBuffer, int numSamples)
{
    CS_ASSERT_BUFFER_SIZE(numSamples);
    CS_ASSERT(leftBuffer != nullptr);
    CS_ASSERT(rightBuffer != nullptr);
    
    // Clear output buffers first to prevent residual data
    std::memset(leftBuffer, 0, numSamples * sizeof(float));
    if (leftBuffer != rightBuffer) {
        std::memset(rightBuffer, 0, numSamples * sizeof(float));
    }
    
    if (!initialized) {
        return; // Buffers already cleared
    }
    
    // Catmull-Rom cubic between h[1] and h[2]; h[0] and h[3] are the neighbours.
    auto interpolate = [](const std::array<float, 4>& h, float t) {
        const float a = -0.5f * h[0] + 1.5f * h[1] - 1.5f * h[2] + 0.5f * h[3];
        const float b = h[0] - 2.5f * h[1] + 2.0f * h[2] - 0.5f * h[3];
        const float c = -0.5f * h[0] + 0.5f * h[2];
        return ((a * t + b) * t + c) * t + h[1];
    };
    
    for (int i = 0; i < numSamples; i++) {
        while (resamplePhase >= 1.0) {
            renderNativeSample();
            resamplePhase -= 1.0;
        }
        const float t = static_cast<float>(resamplePhase);
        leftBuffer[i] = interpolate(historyLeft, t);
        rightBuffer[i] = interpolate(historyRight, t);
        resamplePhase += resampleStep;
    }
}

void YmfmWrapper::noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_NOTE(note);
    CS_ASSERT_VELOCITY(velocity);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;  // Limit to 8 channels
    
    // noteOn called - debug output disabled for performance
    
    // Store the base note for this channel
    channelStates[channel].baseNote = note;
    channelStates[channel].active = true;
    
    if (chipType == ChipType::OPM) {
        writePitch(channel);
        
        // Apply velocity sensitivity to channel before key on
        applyVelocityToChannel(channel, velocity);
        
        writeRegister(YM2151Regs::REG_KEY_ON_OFF,
                      static_cast<uint8_t>(YM2151Regs::keyOnBitsForSlotMask(channelStates[channel].slotMask) | channel));
        
        // Key-on debug output disabled
        
    } else if (chipType == ChipType::OPNA) {
        uint8_t block = note / 12 - 1;
        uint16_t fnum = noteToFnum(note);
        
        // F-Number registers
        writeRegister(YM2151Regs::REG_OPNA_FNUM_LOW_BASE + channel, fnum & 0xFF);
        writeRegister(YM2151Regs::REG_OPNA_FNUM_HIGH_BASE + channel, ((block & YM2151Regs::MASK_OCTAVE) << YM2151Regs::SHIFT_OPNA_BLOCK) | ((fnum >> 8) & 0x07));
        
        // Apply velocity to TL (operator 2)
        uint8_t tl = YM2151Regs::VELOCITY_TO_TL_OFFSET - velocity;
        writeRegister(YM2151Regs::REG_OPNA_TL_OP2_BASE + channel, tl);
        
        // Key On (all operators)
        writeRegister(YM2151Regs::REG_OPNA_KEY_ON_OFF, YM2151Regs::OPNA_KEY_ON_ALL_OPS | channel);
    }
}

void YmfmWrapper::setChannelSlotMask(uint8_t channel, uint8_t voiceOrderMask)
{
    CS_ASSERT_CHANNEL(channel);
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    channelStates[channel].slotMask = static_cast<uint8_t>(voiceOrderMask & YM2151Regs::MASK_SLOT_ENABLE);
}

void YmfmWrapper::noteOff(uint8_t channel, uint8_t note)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_NOTE(note);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    // Mark channel as inactive
    channelStates[channel].active = false;
    channelStates[channel].baseNote = 0;
    
    if (chipType == ChipType::OPM) {
        // Key Off - use sample code format
        writeRegister(YM2151Regs::REG_KEY_ON_OFF, YM2151Regs::KEY_OFF_MASK | channel);
    } else if (chipType == ChipType::OPNA) {
        // Key Off
        writeRegister(YM2151Regs::REG_OPNA_KEY_ON_OFF, channel);
    }
}

uint16_t YmfmWrapper::noteToFnum(uint8_t note)
{
    // Basic frequency table for 12-TET
    static const uint16_t fnum_table[12] = {
        0x269, 0x28E, 0x2B5, 0x2DE, 0x30A, 0x338,
        0x369, 0x39D, 0x3D4, 0x40E, 0x44C, 0x48E
    };
    return fnum_table[note % 12];
}

void YmfmWrapper::setupBasicPianoVoice(uint8_t channel)
{
    if (chipType == ChipType::OPM) {
        // Setting up sine wave timbre for OPM channel (debug output disabled)
        
        // Algorithm 0 (simple FM), FB=0, preserve current pan setting
        uint8_t currentReg = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        uint8_t currentPan = currentReg & YM2151Regs::MASK_PAN_LR;
        uint8_t algFbLr = 0x00 | (0x00 << YM2151Regs::SHIFT_FEEDBACK) | currentPan;
        writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, algFbLr);
        
        // setupBasicPianoVoice debug output disabled
        
        // Configure all 4 operators for Algorithm 0 (simple FM)
        for (int op = 0; op < YM2151Regs::MAX_OPERATORS_PER_VOICE; op++) {
            int base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[op] + channel;
            
            writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, YM2151Regs::DEFAULT_DT1_MUL);     // DT1=0, MUL=1
            
            // In Algorithm 0: OP1->OP2->OP3->OP4 (OP4 is carrier, others are modulators)
            // Set carrier (OP4, index 3) to full volume, modulators to moderate level
            uint8_t totalLevel = (op == 3) ? 0 : 32;  // OP4 (carrier) loud, others moderate
            writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, totalLevel);
            
            writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, YM2151Regs::DEFAULT_KS_AR);       // KS=0, AR=31
            writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, YM2151Regs::DEFAULT_AMS_D1R);     // AMS-EN=0, D1R=0
            writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, YM2151Regs::DEFAULT_DT2_D2R);     // DT2=0, D2R=0
            writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, YM2151Regs::DEFAULT_D1L_RR);      // D1L=0, RR=7
        }
        
        // OPM voice setup complete (debug output disabled)
    }
}

void YmfmWrapper::playTestNote()
{
    if (chipType == ChipType::OPM && opmChip) {
        CS_DBG(" Playing test note (C4) for debugging");
        CS_LOG(" Playing test note (C4) for debugging");
        
        // Play middle C (C4, note 60) with full velocity
        noteOn(0, YM2151Regs::MIDI_NOTE_C4, YM2151Regs::MAX_VELOCITY);
    }
}

void YmfmWrapper::setOperatorParameter(uint8_t channel, uint8_t operator_num, OperatorParameter param, uint8_t value)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_OPERATOR(operator_num);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || operator_num >= YM2151Regs::MAX_OPERATORS_PER_VOICE) return;
    
    if (chipType == ChipType::OPM) {
        uint8_t base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[operator_num] + channel;
        uint8_t currentValue;
        
        switch (param) {
            case OperatorParameter::TotalLevel:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 127);  // TL is 7-bit (0-127)
                baseTotalLevel[channel][operator_num] = value;
                writeTotalLevel(channel, operator_num);
                break;
                
            case OperatorParameter::AttackRate:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 31);  // AR is 5-bit (0-31)
                // Keep existing KS bits, update AR
                currentValue = readCurrentRegister(YM2151Regs::REG_KS_AR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_KS) | (value & YM2151Regs::MASK_ATTACK_RATE));
                break;
                
            case OperatorParameter::Decay1Rate:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 31);  // D1R is 5-bit (0-31)
                // Keep existing AMS-EN bit, update D1R
                currentValue = readCurrentRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr);
                writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_AMS) | (value & YM2151Regs::MASK_DECAY1_RATE));
                break;
                
            case OperatorParameter::Decay2Rate:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 31);  // D2R is 5-bit (0-31)
                // Keep existing DT2 bits, update D2R
                currentValue = readCurrentRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_DT2) | (value & YM2151Regs::MASK_DECAY2_RATE));
                break;
                
            case OperatorParameter::ReleaseRate:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 15);  // RR is 4-bit (0-15)
                // Keep existing D1L bits, update RR
                currentValue = readCurrentRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_D1L) | (value & YM2151Regs::MASK_RELEASE_RATE));
                break;
                
            case OperatorParameter::SustainLevel:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 15);  // D1L is 4-bit (0-15)
                // Keep existing RR bits, update D1L
                currentValue = readCurrentRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_SUSTAIN_LEVEL) << YM2151Regs::SHIFT_SUSTAIN_LEVEL) | 
                            (currentValue & YM2151Regs::PRESERVE_RR));
                break;
                
            case OperatorParameter::Multiple:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 15);  // MUL is 4-bit (0-15)
                // Keep existing DT1 bits, update MUL
                currentValue = readCurrentRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_MUL) | (value & YM2151Regs::MASK_MULTIPLE));
                break;
                
            case OperatorParameter::Detune1:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 7);   // DT1 is 3-bit (0-7)
                // Keep existing MUL bits, update DT1
                currentValue = readCurrentRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_DETUNE1) << YM2151Regs::SHIFT_DETUNE1) | 
                            (currentValue & YM2151Regs::PRESERVE_DT1));
                break;
                
            case OperatorParameter::Detune2:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 3);   // DT2 is 2-bit (0-3)
                // Keep existing D2R bits, update DT2
                currentValue = readCurrentRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_DETUNE2) << YM2151Regs::SHIFT_DETUNE2) | 
                            (currentValue & YM2151Regs::PRESERVE_D2R));
                break;
                
            case OperatorParameter::KeyScale:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 3);   // KS is 2-bit (0-3)
                // Keep existing AR bits, update KS
                currentValue = readCurrentRegister(YM2151Regs::REG_KS_AR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_KEY_SCALE) << YM2151Regs::SHIFT_KEY_SCALE) | 
                            (currentValue & YM2151Regs::PRESERVE_AR));
                break;
                
            case OperatorParameter::AmsEnable:
                // AmsEnable is handled via dedicated setOperatorAmsEnable method
                // This switch case ensures enum completeness
                setOperatorAmsEnable(channel, operator_num, value != 0);
                break;
        }
    }
}

void YmfmWrapper::setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value)
{
    CS_ASSERT_CHANNEL(channel);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    if (chipType == ChipType::OPM) {
        uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        
        switch (param) {
            case ChannelParameter::Algorithm:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 7);   // Algorithm is 3-bit (0-7)
                // Keep existing L/R/FB bits, update ALG
                writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, 
                            (currentValue & YM2151Regs::PRESERVE_ALG_FB_LR) | (value & YM2151Regs::MASK_ALGORITHM));
                break;
                
            case ChannelParameter::Feedback:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 7);   // Feedback is 3-bit (0-7)
                // Keep existing L/R/ALG bits, update FB
                writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, 
                            (currentValue & YM2151Regs::PRESERVE_ALG_LR) | ((value & YM2151Regs::MASK_FEEDBACK) << YM2151Regs::SHIFT_FEEDBACK));
                break;
                
            case ChannelParameter::Pan:
                // Pan is handled via dedicated setChannelPan method
                // Value interpretation: 0=Left, 1=Center, 2=Right, 3=Off
                setChannelPan(channel, static_cast<float>(value) / 3.0f);
                break;
                
            case ChannelParameter::AMS:
                // AMS is handled via dedicated setChannelAmsPms method
                {
                    uint8_t regValue = readCurrentRegister(YM2151Regs::REG_LFO_AMS_PMS_BASE + channel);
                    uint8_t currentPms = (regValue >> YM2151Regs::SHIFT_LFO_PMS) & YM2151Regs::MASK_LFO_PMS;
                    setChannelAmsPms(channel, value, currentPms);
                }
                break;
                
            case ChannelParameter::PMS:
                // PMS is handled via dedicated setChannelAmsPms method  
                {
                    uint8_t regValue = readCurrentRegister(YM2151Regs::REG_LFO_AMS_PMS_BASE + channel);
                    uint8_t currentAms = regValue & YM2151Regs::MASK_LFO_AMS;
                    setChannelAmsPms(channel, currentAms, value);
                }
                break;
        }
    }
}

void YmfmWrapper::setAlgorithm(uint8_t channel, uint8_t algorithm)
{
    setChannelParameter(channel, ChannelParameter::Algorithm, algorithm);
    // The carrier set changed; a held note keeps its velocity and level motion on the right operators
    if (channel < YM2151Regs::MAX_OPM_CHANNELS && (velocityAttenuation[channel] != 0 || carrierMotion[channel] != 0 || modulatorMotion[channel] != 0))
        for (uint8_t op = 0; op < YM2151Regs::MAX_OPERATORS_PER_VOICE; ++op) writeTotalLevel(channel, op);
}

bool YmfmWrapper::isCarrier(uint8_t channel, uint8_t operator_num) const
{
    const uint8_t algorithm = currentRegisters[YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel] & YM2151Regs::MASK_ALGORITHM;
    return ((YM2151Regs::ALGORITHM_CARRIER_MASK[algorithm] >> operator_num) & 1) != 0;
}

void YmfmWrapper::setChannelLevelMotion(uint8_t channel, int carrierSteps, int modulatorSteps)
{
    CS_ASSERT_CHANNEL(channel);
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    if (carrierMotion[channel] == carrierSteps && modulatorMotion[channel] == modulatorSteps) return;
    const bool carriersChanged = carrierMotion[channel] != carrierSteps;
    const bool modulatorsChanged = modulatorMotion[channel] != modulatorSteps;
    carrierMotion[channel] = carrierSteps;
    modulatorMotion[channel] = modulatorSteps;
    for (uint8_t op = 0; op < YM2151Regs::MAX_OPERATORS_PER_VOICE; ++op)
        if (isCarrier(channel, op) ? carriersChanged : modulatorsChanged) writeTotalLevel(channel, op);
}

void YmfmWrapper::writeTotalLevel(uint8_t channel, uint8_t operator_num)
{
    const bool carrier = isCarrier(channel, operator_num);
    const int attenuation = carrier ? velocityAttenuation[channel] + carrierMotion[channel] : modulatorMotion[channel];
    const int tl = juce::jlimit(0, 127, baseTotalLevel[channel][operator_num] + attenuation);
    writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[operator_num] + channel,
                  static_cast<uint8_t>(tl));
}

void YmfmWrapper::setFeedback(uint8_t channel, uint8_t feedback)
{
    setChannelParameter(channel, ChannelParameter::Feedback, feedback);
}

void YmfmWrapper::setOperatorParameters(uint8_t channel, uint8_t operator_num, 
                                      uint8_t tl, uint8_t ar, uint8_t d1r, uint8_t d2r, 
                                      uint8_t rr, uint8_t d1l, uint8_t ks, uint8_t mul, uint8_t dt1, uint8_t dt2)
{
    setOperatorParameter(channel, operator_num, OperatorParameter::TotalLevel, tl);
    setOperatorParameter(channel, operator_num, OperatorParameter::AttackRate, ar);
    setOperatorParameter(channel, operator_num, OperatorParameter::Decay1Rate, d1r);
    setOperatorParameter(channel, operator_num, OperatorParameter::Decay2Rate, d2r);
    setOperatorParameter(channel, operator_num, OperatorParameter::ReleaseRate, rr);
    setOperatorParameter(channel, operator_num, OperatorParameter::SustainLevel, d1l);
    setOperatorParameter(channel, operator_num, OperatorParameter::KeyScale, ks);
    setOperatorParameter(channel, operator_num, OperatorParameter::Multiple, mul);
    setOperatorParameter(channel, operator_num, OperatorParameter::Detune1, dt1);
    setOperatorParameter(channel, operator_num, OperatorParameter::Detune2, dt2);
}

uint16_t YmfmWrapper::noteToFnumWithPitchBend(uint8_t note, float pitchBendSemitones)
{
    // YM2151 pitch = KC (octave + note code, see KEY_CODE_NOTE_TABLE) plus KF in
    // 1/64-semitone steps. The note code field starts at C#, so MIDI note 61 (C#4)
    // is octave 4 / code 0 and C4 is octave 3 / code 14.
    const float actualNote = static_cast<float>(note) + pitchBendSemitones;
    const int wholeNote = static_cast<int>(std::floor(actualNote));
    const float fraction = actualNote - static_cast<float>(wholeNote);
    
    const int semitonesFromCsharp4 = wholeNote - YM2151Regs::MIDI_NOTE_CSHARP4;
    int octave = YM2151Regs::KEY_CODE_OCTAVE_4
               + static_cast<int>(std::floor(static_cast<float>(semitonesFromCsharp4) / YM2151Regs::NOTES_PER_OCTAVE));
    int noteIndex = ((semitonesFromCsharp4 % YM2151Regs::NOTES_PER_OCTAVE) + YM2151Regs::NOTES_PER_OCTAVE)
                    % YM2151Regs::NOTES_PER_OCTAVE;
    uint8_t kf = static_cast<uint8_t>(fraction * YM2151Regs::KF_SCALE_FACTOR) & YM2151Regs::MASK_KEY_FRACTION;
    
    // Clamp to the chip's 8 octaves
    if (octave < YM2151Regs::MIN_OCTAVE) {
        octave = YM2151Regs::MIN_OCTAVE;
        noteIndex = 0;
        kf = 0;
    } else if (octave > YM2151Regs::MAX_OCTAVE) {
        octave = YM2151Regs::MAX_OCTAVE;
        noteIndex = YM2151Regs::NOTES_PER_OCTAVE - 1;
        kf = YM2151Regs::MASK_KEY_FRACTION;
    }
    
    const uint8_t kc = static_cast<uint8_t>(((octave & YM2151Regs::MASK_OCTAVE) << YM2151Regs::SHIFT_OCTAVE)
                                            | YM2151Regs::KEY_CODE_NOTE_TABLE[noteIndex]);
    return static_cast<uint16_t>((kc << YM2151Regs::SHIFT_KEY_CODE) | kf);
}

void YmfmWrapper::setPitchBend(uint8_t channel, float semitones)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_PARAMETER_RANGE(semitones, -12.0f, 12.0f);  // Reasonable pitch bend range
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    // Update the pitch bend state for this channel
    channelStates[channel].pitchBend = semitones;
    
    if (channelStates[channel].active) writePitch(channel);
}

void YmfmWrapper::setChannelPitchOffset(uint8_t channel, float semitones)
{
    CS_ASSERT_CHANNEL(channel);
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    if (channelStates[channel].motionOffset == semitones) return;
    channelStates[channel].motionOffset = semitones;
    if (channelStates[channel].active) writePitch(channel);
}

void YmfmWrapper::writePitch(uint8_t channel)
{
    if (chipType != ChipType::OPM) return;
    const auto& state = channelStates[channel];
    const float pitch = state.pitchBend + state.motionOffset;
    const float spread = wideEnabled ? wideDetuneSemitones : 0.0f;
    
    auto split = [](uint16_t fnum, uint8_t& kc, uint8_t& kf) {
        kc = (fnum >> YM2151Regs::SHIFT_KEY_CODE) & YM2151Regs::MASK_KEY_CODE;
        kf = static_cast<uint8_t>((fnum & YM2151Regs::MASK_KEY_FRACTION) << YM2151Regs::SHIFT_KEY_FRACTION);
    };
    uint8_t kc, kf;
    split(noteToFnumWithPitchBend(state.baseNote, pitch - spread), kc, kf);
    if (currentRegisters[YM2151Regs::REG_KEY_CODE_BASE + channel] != kc)
        writeRegister(YM2151Regs::REG_KEY_CODE_BASE + channel, kc);
    if (currentRegisters[YM2151Regs::REG_KEY_FRACTION_BASE + channel] != kf)
        writeRegister(YM2151Regs::REG_KEY_FRACTION_BASE + channel, kf);
    
    uint8_t shadowKc, shadowKf;
    split(noteToFnumWithPitchBend(state.baseNote, pitch + spread), shadowKc, shadowKf);
    if (shadowRegisters[YM2151Regs::REG_KEY_CODE_BASE + channel] != shadowKc)
        writeShadow(static_cast<uint8_t>(YM2151Regs::REG_KEY_CODE_BASE + channel), shadowKc);
    if (shadowRegisters[YM2151Regs::REG_KEY_FRACTION_BASE + channel] != shadowKf)
        writeShadow(static_cast<uint8_t>(YM2151Regs::REG_KEY_FRACTION_BASE + channel), shadowKf);
}

void YmfmWrapper::setChannelPan(uint8_t channel, float panValue)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_PAN_RANGE(panValue);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    // CS_FILE_DBG("YmfmWrapper::setChannelPan - Setting channel " + juce::String((int)channel) + " pan to " + juce::String(panValue, 3));
    
    if (chipType == ChipType::OPM) {
        // Read current register value
        uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        
        // Convert pan value to YM2151 pan bits
        uint8_t panBits = YM2151Regs::panValueToPanBits(panValue);
        
        // Clear L/R bits and set new pan
        uint8_t newValue = (currentValue & YM2151Regs::PRESERVE_ALG_FB) | panBits;
        
        writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, newValue);
        
        // CS_FILE_DBG("YmfmWrapper::setChannelPan - channel=" + juce::String((int)channel) + 
        //            ", pan=" + juce::String(panValue, 3) + 
        //            ", panBits=0x" + juce::String::toHexString(panBits) + 
        //            ", reg=0x" + juce::String::toHexString(newValue));
    } else if (chipType == ChipType::OPNA) {
        // OPNA uses different pan control mechanism
        // For now, just debug log - OPNA pan would need separate implementation
        CS_DBG("OPNA pan control not yet implemented for channel " + juce::String((int)channel));
    }
}

void YmfmWrapper::setLfoParameters(uint8_t rate, uint8_t amd, uint8_t pmd, uint8_t waveform)
{
    CS_ASSERT_PARAMETER_RANGE(rate, 0, 255);
    CS_ASSERT_PARAMETER_RANGE(amd, 0, 127);
    CS_ASSERT_PARAMETER_RANGE(pmd, 0, 127);
    CS_ASSERT_PARAMETER_RANGE(waveform, 0, 3);
    
    CS_DBG("Setting LFO parameters - rate=" + juce::String((int)rate) + 
           ", amd=" + juce::String((int)amd) + 
           ", pmd=" + juce::String((int)pmd) + 
           ", waveform=" + juce::String((int)waveform));
    
    if (chipType == ChipType::OPM) {
        // Write LFO frequency
        writeRegister(YM2151Regs::REG_LFO_RATE, rate);
        
        // AMD and PMD share one register; bit 7 selects which depth the write sets
        writeRegister(YM2151Regs::REG_LFO_DEPTH, amd & YM2151Regs::MASK_LFO_DEPTH);
        writeRegister(YM2151Regs::REG_LFO_DEPTH, YM2151Regs::LFO_DEPTH_SELECT_PMD | (pmd & YM2151Regs::MASK_LFO_DEPTH));
        
        // Read current waveform register to preserve CT1/CT2 bits
        uint8_t currentWaveform = readCurrentRegister(YM2151Regs::REG_LFO_WAVEFORM);
        
        // Clear waveform bits and set new waveform (bits 0-1)
        uint8_t newWaveform = (currentWaveform & 0xFC) | (waveform & YM2151Regs::MASK_LFO_WAVEFORM);
        
        writeRegister(YM2151Regs::REG_LFO_WAVEFORM, newWaveform);
        
        CS_DBG("LFO registers updated - rate=0x" + juce::String::toHexString(rate) +
               ", amd=0x" + juce::String::toHexString(amd) +
               ", pmd=0x" + juce::String::toHexString(pmd) +
               ", waveform=0x" + juce::String::toHexString(newWaveform));
    }
}

void YmfmWrapper::setChannelAmsPms(uint8_t channel, uint8_t ams, uint8_t pms)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_PARAMETER_RANGE(ams, 0, 3);
    CS_ASSERT_PARAMETER_RANGE(pms, 0, 7);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    CS_DBG("Setting channel " + juce::String((int)channel) + 
           " AMS=" + juce::String((int)ams) + 
           ", PMS=" + juce::String((int)pms));
    
    if (chipType == ChipType::OPM) {
        // AMS is bits 0-1, PMS is bits 4-6
        uint8_t value = (ams & YM2151Regs::MASK_LFO_AMS) | 
                       ((pms & YM2151Regs::MASK_LFO_PMS) << YM2151Regs::SHIFT_LFO_PMS);
        
        writeRegister(YM2151Regs::REG_LFO_AMS_PMS_BASE + channel, value);
        
        CS_DBG("AMS/PMS register updated - channel=" + juce::String((int)channel) +
               ", value=0x" + juce::String::toHexString(value));
    }
}

void YmfmWrapper::setOperatorAmsEnable(uint8_t channel, uint8_t operator_num, bool enable)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_OPERATOR(operator_num);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || operator_num >= YM2151Regs::MAX_OPERATORS_PER_VOICE) return;
    
    //CS_DBG("Setting operator " + juce::String((int)operator_num) + 
    //       " on channel " + juce::String((int)channel) + 
    //       " AMS enable=" + juce::String(enable ? "true" : "false"));
    
    if (chipType == ChipType::OPM) {
        uint8_t base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[operator_num] + channel;
        
        // Read current register value to preserve D1R bits
        uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr);
        
        // AMS enable is bit 7
        uint8_t newValue = enable ? 
            (currentValue | (YM2151Regs::MASK_AMS_ENABLE << YM2151Regs::SHIFT_AMS_ENABLE)) :
            (currentValue & ~(YM2151Regs::MASK_AMS_ENABLE << YM2151Regs::SHIFT_AMS_ENABLE));
        
        writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, newValue);
        
        //CS_DBG("AMS enable register updated - operator=" + juce::String((int)operator_num) +
        //       ", channel=" + juce::String((int)channel) +
        //       ", value=0x" + juce::String::toHexString(newValue));
    }
}

// Envelope optimization methods implementation
void YmfmWrapper::setOperatorEnvelope(uint8_t channel, uint8_t operator_num, 
                                     uint8_t ar, uint8_t d1r, uint8_t d2r, uint8_t rr, uint8_t d1l)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_OPERATOR(operator_num);
    CS_ASSERT_PARAMETER_RANGE(ar, 0, 31);
    CS_ASSERT_PARAMETER_RANGE(d1r, 0, 31);
    CS_ASSERT_PARAMETER_RANGE(d2r, 0, 31);
    CS_ASSERT_PARAMETER_RANGE(rr, 0, 15);
    CS_ASSERT_PARAMETER_RANGE(d1l, 0, 15);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || operator_num >= YM2151Regs::MAX_OPERATORS_PER_VOICE) return;
    
    CS_DBG("Batch setting envelope for operator " + juce::String((int)operator_num) + 
           " on channel " + juce::String((int)channel) + 
           " AR=" + juce::String((int)ar) + 
           ", D1R=" + juce::String((int)d1r) +
           ", D2R=" + juce::String((int)d2r) +
           ", RR=" + juce::String((int)rr) +
           ", D1L=" + juce::String((int)d1l));
    
    if (chipType == ChipType::OPM) {
        uint8_t base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[operator_num] + channel;
        
        // Batch update all envelope registers for this operator
        writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, 
                     (readCurrentRegister(YM2151Regs::REG_KS_AR_BASE + base_addr) & YM2151Regs::MASK_KEY_SCALE_PRESERVE) | ar);
        
        writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, 
                     (readCurrentRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr) & YM2151Regs::MASK_AMS_PRESERVE) | d1r);
        
        writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, 
                     (readCurrentRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr) & YM2151Regs::MASK_DETUNE2_PRESERVE) | d2r);
        
        writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, 
                     (d1l << YM2151Regs::SHIFT_SUSTAIN_LEVEL) | rr);
    }
}

void YmfmWrapper::batchUpdateChannelParameters(uint8_t channel, uint8_t algorithm, uint8_t feedback,
                                              const std::array<std::array<uint8_t, 10>, 4>& operatorParams)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_PARAMETER_RANGE(algorithm, 0, 7);
    CS_ASSERT_PARAMETER_RANGE(feedback, 0, 7);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    CS_DBG("Batch updating channel " + juce::String((int)channel) + 
           " with algorithm=" + juce::String((int)algorithm) + 
           ", feedback=" + juce::String((int)feedback));
    
    if (chipType == ChipType::OPM) {
        // Update algorithm and feedback first, preserve current pan setting
        uint8_t currentReg = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        uint8_t currentPan = currentReg & YM2151Regs::MASK_PAN_LR;
        uint8_t conn_value = (feedback << YM2151Regs::SHIFT_FEEDBACK) | algorithm | currentPan;
        writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, conn_value);
        
        CS_DBG("batchUpdateChannelParameters preserving pan 0x" + juce::String::toHexString(currentPan) + " for channel " + juce::String((int)channel));
        
        // Batch update all operators for this channel
        for (int op = 0; op < 4; ++op) {
            const auto& params = operatorParams[op];
            // params order: TL, AR, D1R, D2R, RR, D1L, KS, MUL, DT1, DT2
            
            uint8_t tl = params[0];
            uint8_t ar = params[1];
            uint8_t d1r = params[2];
            uint8_t d2r = params[3];
            uint8_t rr = params[4];
            uint8_t d1l = params[5];
            uint8_t ks = params[6];
            uint8_t mul = params[7];
            uint8_t dt1 = params[8];
            uint8_t dt2 = params[9];
            
            uint8_t base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[op] + channel;
            
            // Batch write all operator registers
            writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, 
                         (dt1 << YM2151Regs::SHIFT_DETUNE1) | mul);
            writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, tl);
            writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, 
                         (ks << YM2151Regs::SHIFT_KEY_SCALE) | ar);
            writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, 
                         (readCurrentRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr) & YM2151Regs::MASK_AMS_PRESERVE) | d1r);
            writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, 
                         (dt2 << YM2151Regs::SHIFT_DETUNE2) | d2r);
            writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, 
                         (d1l << YM2151Regs::SHIFT_SUSTAIN_LEVEL) | rr);
        }
        
        CS_DBG("Batch update completed for channel " + juce::String((int)channel));
    }
}

YmfmWrapper::EnvelopeDebugInfo YmfmWrapper::getEnvelopeDebugInfo(uint8_t channel, uint8_t operator_num) const
{
    EnvelopeDebugInfo info = {0, 0, 0, false};
    
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_OPERATOR(operator_num);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || operator_num >= YM2151Regs::MAX_OPERATORS_PER_VOICE) {
        return info;
    }
    
    if (chipType == ChipType::OPM && opmChip) {
        // Note: This is a simplified implementation. In practice, we would need
        // to access ymfm's internal state to get actual envelope information.
        // For now, we return basic information based on register values.
        
        uint8_t base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[operator_num] + channel;
        
        // Read envelope-related registers to estimate state
        uint8_t ar_ks = currentRegisters[YM2151Regs::REG_KS_AR_BASE + base_addr];
        uint8_t d1l_rr = currentRegisters[YM2151Regs::REG_D1L_RR_BASE + base_addr];
        
        info.effectiveRate = ar_ks & YM2151Regs::MASK_ATTACK_RATE;
        info.currentLevel = (d1l_rr >> YM2151Regs::SHIFT_SUSTAIN_LEVEL) & YM2151Regs::MASK_SUSTAIN_LEVEL;
        info.isActive = channelStates[channel].active;
        
        // Estimate current state based on channel activity
        if (info.isActive) {
            info.currentState = 1; // Assume attack or decay state when active
        } else {
            info.currentState = 0; // Assume silent state
        }
    }
    
    return info;
}

void YmfmWrapper::applyVelocityToChannel(uint8_t channel, uint8_t velocity)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_VELOCITY(velocity);
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || chipType != ChipType::OPM) return;
    
    // Linear: velocity 127 plays the preset level, low velocities attenuate the carriers only,
    // so the timbre (modulator levels) does not change with velocity
    const float quiet = 1.0f - static_cast<float>(velocity) / static_cast<float>(YM2151Regs::MAX_VELOCITY);
    velocityAttenuation[channel] = static_cast<uint8_t>(juce::roundToInt(quiet * static_cast<float>(YM2151Regs::VELOCITY_TL_RANGE)));
    for (uint8_t op = 0; op < YM2151Regs::MAX_OPERATORS_PER_VOICE; ++op) writeTotalLevel(channel, op);
}

// =============================================================================
// Noise Generator Implementation
// =============================================================================

void YmfmWrapper::setNoiseEnable(bool enable)
{
    if (chipType != ChipType::OPM) {
        CS_DBG("Warning: Noise is only supported on OPM (YM2151) chip");
        return;
    }
    
    // Read current noise control register value
    uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_NOISE_CONTROL);
    
    // Update noise enable bit while preserving frequency
    uint8_t newValue = (currentValue & YM2151Regs::MASK_NOISE_FREQUENCY) | 
                       (enable ? YM2151Regs::MASK_NOISE_ENABLE : 0);
    
    writeRegister(YM2151Regs::REG_NOISE_CONTROL, newValue);
    
    CS_DBG("Noise " + juce::String(enable ? "enabled" : "disabled") + 
           " (register 0x0F = 0x" + juce::String::toHexString(newValue) + ")");
}

void YmfmWrapper::setNoiseFrequency(uint8_t frequency)
{
    CS_ASSERT_PARAMETER_RANGE(frequency, YM2151Regs::NOISE_FREQUENCY_MIN, YM2151Regs::NOISE_FREQUENCY_MAX);
    
    if (chipType != ChipType::OPM) {
        CS_DBG("Warning: Noise is only supported on OPM (YM2151) chip");
        return;
    }
    
    // Read current noise control register value
    uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_NOISE_CONTROL);
    
    // Update noise frequency while preserving enable bit
    uint8_t newValue = (currentValue & YM2151Regs::MASK_NOISE_ENABLE) | 
                       (frequency & YM2151Regs::MASK_NOISE_FREQUENCY);
    
    writeRegister(YM2151Regs::REG_NOISE_CONTROL, newValue);
    
    CS_DBG("Noise frequency set to " + juce::String((int)frequency) + 
           " (register 0x0F = 0x" + juce::String::toHexString(newValue) + ")");
}

bool YmfmWrapper::getNoiseEnable() const
{
    if (chipType != ChipType::OPM) {
        return false;
    }
    
    uint8_t noiseRegister = readCurrentRegister(YM2151Regs::REG_NOISE_CONTROL);
    return (noiseRegister & YM2151Regs::MASK_NOISE_ENABLE) != 0;
}

uint8_t YmfmWrapper::getNoiseFrequency() const
{
    if (chipType != ChipType::OPM) {
        return 0;
    }
    
    uint8_t noiseRegister = readCurrentRegister(YM2151Regs::REG_NOISE_CONTROL);
    return noiseRegister & YM2151Regs::MASK_NOISE_FREQUENCY;
}

void YmfmWrapper::setNoiseParameters(bool enable, uint8_t frequency)
{
    CS_ASSERT_PARAMETER_RANGE(frequency, YM2151Regs::NOISE_FREQUENCY_MIN, YM2151Regs::NOISE_FREQUENCY_MAX);
    
    if (chipType != ChipType::OPM) {
        CS_DBG("Warning: Noise is only supported on OPM (YM2151) chip");
        return;
    }
    
    // Combine enable and frequency into single register write for efficiency
    uint8_t noiseValue = (enable ? YM2151Regs::MASK_NOISE_ENABLE : 0) | 
                         (frequency & YM2151Regs::MASK_NOISE_FREQUENCY);
    
    writeRegister(YM2151Regs::REG_NOISE_CONTROL, noiseValue);
    
    CS_DBG("Noise parameters set - Enable: " + juce::String(enable ? "ON" : "OFF") + 
           ", Frequency: " + juce::String((int)frequency) + 
           " (register 0x0F = 0x" + juce::String::toHexString(noiseValue) + ")");
}

void YmfmWrapper::testNoiseChannel()
{
    if (chipType != ChipType::OPM) {
        CS_DBG("Warning: Noise test is only supported on OPM (YM2151) chip");
        return;
    }
    
    CS_DBG("Testing YM2151 noise on channel 7 (the only channel where noise works)");
    
    const uint8_t noiseChannel = 7;  // Channel 7 is the only channel where noise works
    
    // Set up channel 7 for noise output using algorithm 7 (all operators parallel)
    // Note: Noise works with any algorithm, but algorithm 7 makes it easiest to hear
    uint8_t currentReg = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + noiseChannel);
    uint8_t currentPan = currentReg & YM2151Regs::MASK_PAN_LR;
    uint8_t algFbLr = 0x07 | (0x00 << YM2151Regs::SHIFT_FEEDBACK) | currentPan;
    writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + noiseChannel, algFbLr);
    
    // Configure operators 1-3 to be silent (high TL values)
    for (int op = 0; op < 3; op++) {  // Operators 0, 1, 2
        int base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[op] + noiseChannel;
        writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, 127);  // Maximum attenuation (silent)
    }
    
    // Configure operator 4 (the noise operator) with audible settings
    int op4_base_addr = YM2151Regs::OPERATOR_SLOT_OFFSET[3] + noiseChannel;  // C2 slot of the noise channel
    
    writeRegister(YM2151Regs::REG_DT1_MUL_BASE + op4_base_addr, YM2151Regs::DEFAULT_DT1_MUL);      // DT1=0, MUL=1
    writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + op4_base_addr, 32);                           // Moderate volume for noise
    writeRegister(YM2151Regs::REG_KS_AR_BASE + op4_base_addr, YM2151Regs::DEFAULT_KS_AR);          // KS=0, AR=31
    writeRegister(YM2151Regs::REG_AMS_D1R_BASE + op4_base_addr, YM2151Regs::DEFAULT_AMS_D1R);      // AMS-EN=0, D1R=0
    writeRegister(YM2151Regs::REG_DT2_D2R_BASE + op4_base_addr, YM2151Regs::DEFAULT_DT2_D2R);      // DT2=0, D2R=0
    writeRegister(YM2151Regs::REG_D1L_RR_BASE + op4_base_addr, YM2151Regs::DEFAULT_D1L_RR);        // D1L=15, RR=7
    
    // Enable noise with medium frequency
    setNoiseParameters(true, YM2151Regs::NOISE_FREQUENCY_DEFAULT);
    
    // Play a note on channel 7 to trigger the noise
    noteOn(noiseChannel, YM2151Regs::MIDI_NOTE_C4, YM2151Regs::MAX_VELOCITY);
    
    CS_DBG("Noise test setup complete:");
    CS_DBG("- Channel: " + juce::String((int)noiseChannel) + " (only channel where noise works)");
    CS_DBG("- Algorithm: 7 (chosen for clarity, but noise works with any algorithm)");
    CS_DBG("- Operators 1-3: Silent (TL=127)");
    CS_DBG("- Operator 4: Configured for noise output (TL=32)");
    CS_DBG("- Noise: Enabled with frequency " + juce::String((int)YM2151Regs::NOISE_FREQUENCY_DEFAULT));
    CS_DBG("- Note: C4 triggered on channel 7");
    CS_DBG("IMPORTANT: YM2151 noise only works on channel 7, operator 4 due to hardware design!");
}


