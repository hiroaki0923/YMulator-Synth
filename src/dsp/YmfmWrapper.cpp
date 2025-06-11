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
            internalSampleRate = opmChip->sample_rate(opm_clock);
            CS_DBG("OPM clock=" + juce::String(opm_clock) + ", chip_rate=" + juce::String(internalSampleRate) + ", output_rate=" + juce::String(outputSampleRate));
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
    
    // Auto-play a note for testing (like sample code) - delay it a bit
    // playTestNote(); // Disable for now, will test via MIDI
    
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

uint8_t YmfmWrapper::readCurrentRegister(int address)
{
    return currentRegisters[static_cast<uint8_t>(address)];
}

void YmfmWrapper::updateRegisterCache(uint8_t address, uint8_t value)
{
    currentRegisters[address] = value;
}

void YmfmWrapper::generateSamples(float* outputBuffer, int numSamples)
{
    static int debugCounter = 0;
    static bool testModeEnabled = false; // Temporary test mode - now disabled to test ymfm
    
    if (testModeEnabled) {
        // Generate a simple 440 Hz test tone instead of using ymfm
        static float phase = 0.0f;
        const float frequency = YM2151Regs::TEST_FREQUENCY; // A4
        const float amplitude = YM2151Regs::TEST_AMPLITUDE;
        const float phaseIncrement = (2.0f * M_PI * frequency) / outputSampleRate;
        
        bool hasNonZero = false;
        for (int i = 0; i < numSamples; i++) {
            outputBuffer[i] = amplitude * std::sin(phase);
            phase += phaseIncrement;
            if (phase > 2.0f * M_PI) {
                phase -= 2.0f * M_PI;
            }
            if (outputBuffer[i] != 0.0f) {
                hasNonZero = true;
            }
        }
        
        debugCounter++;
        if ((debugCounter % YM2151Regs::DEBUG_COUNTER_INTERVAL == 0) || hasNonZero) {
            CS_DBG(" TEST MODE - generateSamples call #" + juce::String(debugCounter) + ", hasNonZero=" + juce::String(hasNonZero ? 1 : 0) + ", samples=" + juce::String(numSamples));
            CS_LOGF(" TEST MODE - generateSamples call #%d, hasNonZero=%d, samples=%d", 
                      debugCounter, hasNonZero ? 1 : 0, numSamples);
        }
        return;
    }
    
    if (chipType == ChipType::OPM && opmChip) {
        static int lastNonZeroSample = 0;
        bool hasNonZero = false;
        int maxSample = 0;
        
        for (int i = 0; i < numSamples; i++) {
            // Generate 1 sample like the sample code
            opmChip->generate(&opmOutput, 1);
            
            // Convert to float and store mono (left channel)
            outputBuffer[i] = opmOutput.data[0] / YM2151Regs::SAMPLE_SCALE_FACTOR;
            
            
            if (opmOutput.data[0] != 0) {
                hasNonZero = true;
                lastNonZeroSample = opmOutput.data[0];
                if (abs(opmOutput.data[0]) > abs(maxSample)) {
                    maxSample = opmOutput.data[0];
                }
            }
            
            // Extra detailed debug for first few samples when we might have audio
            if (debugCounter <= YM2151Regs::MAX_DEBUG_CALLS && i < YM2151Regs::DEBUG_SAMPLE_COUNT) {
                CS_DBG(" Sample[" + juce::String(i) + "] = " + juce::String(opmOutput.data[0]) + " (0x" + juce::String::toHexString((uint16_t)opmOutput.data[0]) + ")");
            }
        }
        
        // Debug output every few calls to check if function is being called
        debugCounter++;
        if ((debugCounter % YM2151Regs::DEBUG_COUNTER_INTERVAL == 0) || hasNonZero) {
            CS_DBG(" generateSamples call #" + juce::String(debugCounter) + ", hasNonZero=" + juce::String(hasNonZero ? 1 : 0) + ", maxValue=" + juce::String(maxSample) + ", samples=" + juce::String(numSamples));
            CS_LOGF(" generateSamples call #%d, hasNonZero=%d, maxValue=%d, samples=%d",
                      debugCounter, hasNonZero ? 1 : 0, maxSample, numSamples);
        }
        
    } else if (chipType == ChipType::OPNA && opnaChip) {
        for (int i = 0; i < numSamples; i++) {
            // Generate internal samples - ymfm generate() doesn't take a count parameter
            opnaChip->generate(&opnaOutput);
            
            // Convert to float and store mono (left channel)
            outputBuffer[i] = opnaOutput.data[0] / YM2151Regs::SAMPLE_SCALE_FACTOR;
        }
    }
}

void YmfmWrapper::noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
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
        
        // Algorithm 7 (all operators parallel), L/R both output, FB=0
        writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, YM2151Regs::DEFAULT_ALGORITHM_FB_LR);
        
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
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS || operator_num >= YM2151Regs::MAX_OPERATORS_PER_VOICE) return;
    
    if (chipType == ChipType::OPM) {
        uint8_t base_addr = operator_num * YM2151Regs::OPERATOR_ADDRESS_STEP + channel;
        uint8_t currentValue;
        
        switch (param) {
            case OperatorParameter::TotalLevel:
                writeRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + base_addr, value);
                break;
                
            case OperatorParameter::AttackRate:
                // Keep existing KS bits, update AR
                currentValue = readCurrentRegister(YM2151Regs::REG_KS_AR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_KS_AR_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_KS) | (value & YM2151Regs::MASK_ATTACK_RATE));
                break;
                
            case OperatorParameter::Decay1Rate:
                // Keep existing AMS-EN bit, update D1R
                currentValue = readCurrentRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr);
                writeRegister(YM2151Regs::REG_AMS_D1R_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_AMS) | (value & YM2151Regs::MASK_DECAY1_RATE));
                break;
                
            case OperatorParameter::Decay2Rate:
                // Keep existing DT2 bits, update D2R
                currentValue = readCurrentRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_DT2) | (value & YM2151Regs::MASK_DECAY2_RATE));
                break;
                
            case OperatorParameter::ReleaseRate:
                // Keep existing D1L bits, update RR
                currentValue = readCurrentRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_D1L) | (value & YM2151Regs::MASK_RELEASE_RATE));
                break;
                
            case OperatorParameter::SustainLevel:
                // Keep existing RR bits, update D1L
                currentValue = readCurrentRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr);
                writeRegister(YM2151Regs::REG_D1L_RR_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_SUSTAIN_LEVEL) << YM2151Regs::SHIFT_SUSTAIN_LEVEL) | 
                            (currentValue & YM2151Regs::PRESERVE_RR));
                break;
                
            case OperatorParameter::Multiple:
                // Keep existing DT1 bits, update MUL
                currentValue = readCurrentRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, 
                            (currentValue & YM2151Regs::PRESERVE_MUL) | (value & YM2151Regs::MASK_MULTIPLE));
                break;
                
            case OperatorParameter::Detune1:
                // Keep existing MUL bits, update DT1
                currentValue = readCurrentRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT1_MUL_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_DETUNE1) << YM2151Regs::SHIFT_DETUNE1) | 
                            (currentValue & YM2151Regs::PRESERVE_DT1));
                break;
                
            case OperatorParameter::Detune2:
                // Keep existing D2R bits, update DT2
                currentValue = readCurrentRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr);
                writeRegister(YM2151Regs::REG_DT2_D2R_BASE + base_addr, 
                            ((value & YM2151Regs::MASK_DETUNE2) << YM2151Regs::SHIFT_DETUNE2) | 
                            (currentValue & YM2151Regs::PRESERVE_D2R));
                break;
                
            case OperatorParameter::KeyScale:
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
    if (channel >= YM2151Regs::MAX_OPM_CHANNELS) return;
    
    if (chipType == ChipType::OPM) {
        uint8_t currentValue = readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        
        switch (param) {
            case ChannelParameter::Algorithm:
                // Keep existing L/R/FB bits, update ALG
                writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, 
                            (currentValue & YM2151Regs::PRESERVE_ALG_FB_LR) | (value & YM2151Regs::MASK_ALGORITHM));
                break;
                
            case ChannelParameter::Feedback:
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

