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
    , internalSampleRate(62500)
{
    // Initialize register cache
    std::memset(currentRegisters, 0, sizeof(currentRegisters));
    
    // Initialize velocity sensitivity to default values (1.0 = no scaling)
    for (auto& channel : velocitySensitivity) {
        for (auto& op : channel) {
            op = 1.0f;
        }
    }
}

void YmfmWrapper::initialize(ChipType type, uint32_t outputSampleRate)
{
    chipType = type;
    this->outputSampleRate = outputSampleRate;
    
    if (type == ChipType::OPM) {
        // Use proper OPM clock like S98Player
        uint32_t opm_clock = YM2151Regs::OPM_DEFAULT_CLOCK; // Default YM2151 clock from S98Player
        internalSampleRate = 0; // Will be calculated properly
        initializeOPM();
        
        // Calculate the actual internal sample rate like S98Player does
        if (opmChip) {
            uint32_t ymfm_internal_rate = opmChip->sample_rate(opm_clock);
            // IMPORTANT: Always use the DAW's output sample rate for consistency
            // ymfm internal rate is only used for timing calculations
            internalSampleRate = outputSampleRate; // Use DAW sample rate, not ymfm rate
            CS_DBG("OPM clock=" + juce::String(opm_clock) + ", ymfm_rate=" + juce::String(ymfm_internal_rate) + ", using_output_rate=" + juce::String(outputSampleRate));
        }
    } else {
        internalSampleRate = YM2151Regs::OPNA_INTERNAL_RATE;  // OPNA internal rate  
        initializeOPNA();
    }
    
    initialized = true;
}

void YmfmWrapper::reset()
{
    if (chipType == ChipType::OPM && opmChip) {
        opmChip->reset();
        initializeOPM();
    } else if (chipType == ChipType::OPNA && opnaChip) {
        opnaChip->reset();
        initializeOPNA();
    }
}

void YmfmWrapper::initializeOPM()
{
    CS_DBG("Creating OPM chip instance");
    CS_LOG("Creating OPM chip instance");
    
    opmChip = std::make_unique<ymfm::ym2151>(*this);
    
    CS_DBG("Resetting OPM chip");
    CS_LOG("Resetting OPM chip");
    opmChip->reset();
    
    CS_DBG("OPM chip reset complete, setting up voice");
    CS_LOG("OPM chip reset complete, setting up voice");
    
    // Setup basic piano voice on all 8 channels
    for (int channel = 0; channel < YM2151Regs::MAX_OPM_CHANNELS; ++channel) {
        setupBasicPianoVoice(channel);
    }
    
    CS_DBG("OPM initialization complete");
    CS_LOG("OPM initialization complete");
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
    
    if (chipType == ChipType::OPM && opmChip) {
        CS_DBG(" Writing register 0x" + juce::String::toHexString(addr) + " = 0x" + juce::String::toHexString(data));
        CS_LOGF(" Writing register 0x%02X = 0x%02X", addr, data);
        
        // Use write_address and write_data like sample code
        opmChip->write_address(addr);
        opmChip->write_data(data);
    } else if (chipType == ChipType::OPNA && opnaChip) {
        CS_DBG(" OPNA Writing register 0x" + juce::String::toHexString(addr) + " = 0x" + juce::String::toHexString(data));
        
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

void YmfmWrapper::generateSamples(float* leftBuffer, float* rightBuffer, int numSamples)
{
    CS_ASSERT_BUFFER_SIZE(numSamples);
    CS_ASSERT(leftBuffer != nullptr);
    CS_ASSERT(rightBuffer != nullptr);
    
    if (chipType == ChipType::OPM && opmChip) {
        for (int i = 0; i < numSamples; i++) {
            // Generate 1 sample like the sample code
            opmChip->generate(&opmOutput, 1);
            
            // Convert to float and store stereo
            leftBuffer[i] = opmOutput.data[0] / YM2151Regs::SAMPLE_SCALE_FACTOR;
            rightBuffer[i] = opmOutput.data[1] / YM2151Regs::SAMPLE_SCALE_FACTOR;
        }
        
    } else if (chipType == ChipType::OPNA && opnaChip) {
        for (int i = 0; i < numSamples; i++) {
            // Generate internal samples - ymfm generate() doesn't take a count parameter
            opnaChip->generate(&opnaOutput);
            
            // Convert to float and store stereo
            leftBuffer[i] = opnaOutput.data[0] / YM2151Regs::SAMPLE_SCALE_FACTOR;
            rightBuffer[i] = opnaOutput.data[1] / YM2151Regs::SAMPLE_SCALE_FACTOR;
        }
    }
}

void YmfmWrapper::noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_NOTE(note);
    CS_ASSERT_VELOCITY(velocity);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;  // Limit to 8 channels
    
    CS_DBG(" noteOn - channel=" + juce::String((int)channel) + ", note=" + juce::String((int)note) + ", velocity=" + juce::String((int)velocity));
    CS_LOGF(" noteOn - channel=%d, note=%d, velocity=%d", channel, note, velocity);
    
    // Store the base note for this channel
    channelStates[channel].baseNote = note;
    channelStates[channel].active = true;
    
    if (chipType == ChipType::OPM) {
        // Calculate frequency from MIDI note with current pitch bend
        uint16_t fnum = noteToFnumWithPitchBend(note, channelStates[channel].pitchBend);
        
        // Extract KC and KF from FNUM
        // YM2151 FNUM format: KC (key code) and KF (key fraction)
        uint8_t kc = (fnum >> YM2151Regs::SHIFT_KEY_CODE) & YM2151Regs::MASK_KEY_CODE;  // Upper 7 bits
        uint8_t kf = (fnum & YM2151Regs::MASK_KEY_FRACTION) << YM2151Regs::SHIFT_KEY_FRACTION;  // Lower 6 bits, shifted for register format
        
        CS_DBG(" MIDI Note " + juce::String((int)note) + " with pitch bend " + juce::String(channelStates[channel].pitchBend) +
            " -> FNUM=0x" + juce::String::toHexString(fnum) + ", KC=0x" + juce::String::toHexString(kc) + ", KF=0x" + juce::String::toHexString(kf));
        CS_LOGF(" OPM noteOn - KC=0x%02X, KF=0x%02X", kc, kf);
        
        // Write KC and KF
        writeRegister(YM2151Regs::REG_KEY_CODE_BASE + channel, kc);
        writeRegister(YM2151Regs::REG_KEY_FRACTION_BASE + channel, kf);
        
        // Apply velocity sensitivity to channel before key on
        applyVelocityToChannel(channel, velocity);
        
        // Key On (all operators enabled)
        writeRegister(YM2151Regs::REG_KEY_ON_OFF, YM2151Regs::KEY_ON_ALL_OPS | channel);
        
        CS_DBG(" Key On register 0x" + juce::String::toHexString(YM2151Regs::REG_KEY_ON_OFF) + " = 0x" + juce::String::toHexString(YM2151Regs::KEY_ON_ALL_OPS | channel));
        CS_DBG(" OPM registers written for note on");
        CS_LOG(" OPM registers written for note on");
        
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
        CS_DBG(" Setting up sine wave timbre for OPM channel " + juce::String((int)channel));
        CS_LOGF(" Setting up sine wave timbre for OPM channel %d", channel);
        
        // Algorithm 7 (all operators parallel), FB=0, preserve current pan setting
        uint8_t currentReg = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        uint8_t currentPan = currentReg & YM2151Regs::MASK_PAN_LR;
        uint8_t algFbLr = 0x07 | (0x00 << YM2151Regs::SHIFT_FEEDBACK) | currentPan;
        writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, algFbLr);
        
        CS_DBG(" setupBasicPianoVoice preserving pan 0x" + juce::String::toHexString(currentPan) + " for channel " + juce::String((int)channel));
        
        // Configure all 4 operators like sample code
        for (int op = 0; op < YM2151Regs::MAX_OPERATORS_PER_VOICE; op++) {
            int base_addr = op * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
            
            writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, YM2151Regs::DEFAULT_DT1_MUL);     // DT1=0, MUL=1
            writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, YM2151Regs::DEFAULT_TOTAL_LEVEL);   // TL=32 (moderate volume)
            writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, YM2151Regs::DEFAULT_KS_AR);       // KS=0, AR=31
            writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, YM2151Regs::DEFAULT_AMS_D1R);     // AMS-EN=0, D1R=0
            writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, YM2151Regs::DEFAULT_DT2_D2R);     // DT2=0, D2R=0
            writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, YM2151Regs::DEFAULT_D1L_RR);      // D1L=15, RR=7
        }
        
        CS_DBG(" OPM voice setup complete (sine wave timbre)");
        CS_LOG(" OPM voice setup complete (sine wave timbre)");
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
        uint8_t base_addr = operator_num * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
        uint8_t currentValue;
        
        switch (param) {
            case OperatorParameter::TotalLevel:
                CS_ASSERT_PARAMETER_RANGE(value, 0, 127);  // TL is 7-bit (0-127)
                writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, value);
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
        }
    }
}

void YmfmWrapper::setAlgorithm(uint8_t channel, uint8_t algorithm)
{
    setChannelParameter(channel, ChannelParameter::Algorithm, algorithm);
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
    // Calculate the actual note with pitch bend applied
    float actualNote = note + pitchBendSemitones;
    
    // Calculate frequency from MIDI note
    float freq = YM2151Regs::REFERENCE_FREQUENCY * std::pow(YM2151Regs::SEMITONE_RATIO, (actualNote - YM2151Regs::MIDI_NOTE_A4) / YM2151Regs::NOTES_PER_OCTAVE);
    
    // Convert frequency to YM2151 KC/KF format
    float fnote = YM2151Regs::NOTES_PER_OCTAVE * log2f(freq / YM2151Regs::REFERENCE_FREQUENCY) + YM2151Regs::MIDI_NOTE_A4;
    int noteInt = (int)round(fnote);
    int octave = (noteInt / YM2151Regs::NOTES_PER_OCTAVE) - 1;
    int noteInOctave = noteInt % YM2151Regs::NOTES_PER_OCTAVE;
    
    // Clamp octave to valid range (0-7) for YM2151
    if (octave < YM2151Regs::MIN_OCTAVE) {
        octave = YM2151Regs::MIN_OCTAVE;
        noteInOctave = 0;
    } else if (octave > YM2151Regs::MAX_OCTAVE) {
        octave = YM2151Regs::MAX_OCTAVE;
        noteInOctave = YM2151Regs::NOTES_PER_OCTAVE - 1;
    }
    
    // YM2151 key code calculation
    const uint8_t noteCode[YM2151Regs::NOTES_PER_OCTAVE] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 14};
    uint8_t kc = ((octave & YM2151Regs::MASK_OCTAVE) << YM2151Regs::SHIFT_OCTAVE) | noteCode[noteInOctave];
    
    // Calculate fine tuning (KF) for the fractional part
    float fractionalPart = actualNote - noteInt;
    uint8_t kf = (uint8_t)(fractionalPart * YM2151Regs::KF_SCALE_FACTOR); // 6-bit KF value
    
    // Combine KC and KF into a 16-bit value for convenience
    return (kc << YM2151Regs::SHIFT_KEY_CODE) | (kf & YM2151Regs::MASK_KEY_FRACTION);
}

void YmfmWrapper::setPitchBend(uint8_t channel, float semitones)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_PARAMETER_RANGE(semitones, -12.0f, 12.0f);  // Reasonable pitch bend range
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    // Update the pitch bend state for this channel
    channelStates[channel].pitchBend = semitones;
    
    // If this channel is currently playing a note, update its frequency
    if (channelStates[channel].active && chipType == ChipType::OPM) {
        uint8_t baseNote = channelStates[channel].baseNote;
        uint16_t fnum = noteToFnumWithPitchBend(baseNote, semitones);
        
        // Extract KC and KF from FNUM
        uint8_t kc = (fnum >> YM2151Regs::SHIFT_KEY_CODE) & YM2151Regs::MASK_KEY_CODE;
        uint8_t kf = (fnum & YM2151Regs::MASK_KEY_FRACTION) << YM2151Regs::SHIFT_KEY_FRACTION;
        
        // Update the frequency registers
        writeRegister(YM2151Regs::REG_KEY_CODE_BASE + channel, kc);
        writeRegister(YM2151Regs::REG_KEY_FRACTION_BASE + channel, kf);
        
        CS_DBG(" Pitch bend updated - channel=" + juce::String((int)channel) + 
            ", semitones=" + juce::String(semitones, 3) + 
            ", KC=0x" + juce::String::toHexString(kc) + 
            ", KF=0x" + juce::String::toHexString(kf));
    }
}

void YmfmWrapper::setChannelPan(uint8_t channel, float panValue)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_PAN_RANGE(panValue);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    CS_DBG("Setting channel " + juce::String((int)channel) + " pan to " + juce::String(panValue, 3));
    
    if (chipType == ChipType::OPM) {
        // Read current register value
        uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        
        // Convert pan value to YM2151 pan bits
        uint8_t panBits = YM2151Regs::panValueToPanBits(panValue);
        
        // Clear L/R bits and set new pan
        uint8_t newValue = (currentValue & YM2151Regs::PRESERVE_ALG_FB) | panBits;
        
        writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, newValue);
        
        CS_DBG("Pan register updated - channel=" + juce::String((int)channel) + 
               ", pan=" + juce::String(panValue, 3) + 
               ", panBits=0x" + juce::String::toHexString(panBits) + 
               ", reg=0x" + juce::String::toHexString(newValue));
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
        
        // Write amplitude modulation depth (7-bit value)
        writeRegister(YM2151Regs::REG_LFO_AMD, amd & 0x7F);
        
        // Write phase modulation depth (7-bit value)
        writeRegister(YM2151Regs::REG_LFO_PMD, pmd & 0x7F);
        
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
    
    CS_DBG("Setting operator " + juce::String((int)operator_num) + 
           " on channel " + juce::String((int)channel) + 
           " AMS enable=" + juce::String(enable ? "true" : "false"));
    
    if (chipType == ChipType::OPM) {
        uint8_t base_addr = operator_num * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
        
        // Read current register value to preserve D1R bits
        uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr);
        
        // AMS enable is bit 7
        uint8_t newValue = enable ? 
            (currentValue | (YM2151Regs::MASK_AMS_ENABLE << YM2151Regs::SHIFT_AMS_ENABLE)) :
            (currentValue & ~(YM2151Regs::MASK_AMS_ENABLE << YM2151Regs::SHIFT_AMS_ENABLE));
        
        writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, newValue);
        
        CS_DBG("AMS enable register updated - operator=" + juce::String((int)operator_num) +
               ", channel=" + juce::String((int)channel) +
               ", value=0x" + juce::String::toHexString(newValue));
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
        uint8_t base_addr = operator_num * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
        
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
            
            uint8_t base_addr = op * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
            
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
        
        uint8_t base_addr = operator_num * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
        
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

void YmfmWrapper::setVelocitySensitivity(uint8_t channel, uint8_t operator_num, float sensitivity)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_OPERATOR(operator_num);
    CS_ASSERT_PARAMETER_RANGE(sensitivity, 0.0f, 2.0f);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || operator_num >= YM2151Regs::MAX_OPERATORS_PER_VOICE) return;
    
    velocitySensitivity[channel][operator_num] = sensitivity;
    
    CS_DBG("Set velocity sensitivity for channel " + juce::String((int)channel) + 
           ", operator " + juce::String((int)operator_num) + 
           " to " + juce::String(sensitivity, 3));
}

void YmfmWrapper::applyVelocityToChannel(uint8_t channel, uint8_t velocity)
{
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_VELOCITY(velocity);
    
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || chipType != ChipType::OPM) return;
    
    // Normalize velocity to 0.0-1.0 range
    float normalizedVelocity = velocity / 127.0f;
    
    CS_DBG("Applying velocity " + juce::String((int)velocity) + 
           " (normalized: " + juce::String(normalizedVelocity, 3) + 
           ") to channel " + juce::String((int)channel));
    
    // Apply velocity sensitivity to each operator's Total Level
    for (int op = 0; op < 4; ++op) {
        float sensitivity = velocitySensitivity[channel][op];
        
        // Only apply velocity if sensitivity is not 1.0 (default)
        if (std::abs(sensitivity - 1.0f) > 0.001f) {
            uint8_t base_addr = op * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
            
            // Read current TL value
            uint8_t currentTL = currentRegisters[YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr];
            
            // Calculate velocity-adjusted TL
            // Lower velocity = higher TL (quieter), Higher velocity = lower TL (louder)
            float velocityAdjustment = (1.0f - normalizedVelocity) * sensitivity * 32.0f; // Up to 32 TL steps
            uint8_t adjustedTL = juce::jlimit(0, 127, static_cast<int>(currentTL + velocityAdjustment));
            
            // Write adjusted TL
            writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, adjustedTL);
            
            CS_DBG("Operator " + juce::String(op) + 
                   " TL adjusted from " + juce::String((int)currentTL) + 
                   " to " + juce::String((int)adjustedTL) + 
                   " (sensitivity=" + juce::String(sensitivity, 2) + ")");
        }
    }
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
    uint8_t algFbLr = 0x07 | (0x00 << YM2151Regs::SHIFT_FEEDBACK) | YM2151Regs::PAN_CENTER;
    writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + noiseChannel, algFbLr);
    
    // Configure operators 1-3 to be silent (high TL values)
    for (int op = 0; op < 3; op++) {  // Operators 0, 1, 2
        int base_addr = op * YM2151Regs::OPERATOR_ADDRESS_STEP + noiseChannel;
        writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, 127);  // Maximum attenuation (silent)
    }
    
    // Configure operator 4 (the noise operator) with audible settings
    int op4_base_addr = 3 * YM2151Regs::OPERATOR_ADDRESS_STEP + noiseChannel;  // Operator 3 = index 3
    
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


