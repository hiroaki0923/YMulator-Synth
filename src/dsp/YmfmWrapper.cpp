#include "YmfmWrapper.h"
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
        uint32_t opm_clock = 3579545; // Default YM2151 clock from S98Player
        internalSampleRate = 0; // Will be calculated properly
        initializeOPM();
        
        // Calculate the actual internal sample rate like S98Player does
        if (opmChip) {
            internalSampleRate = opmChip->sample_rate(opm_clock);
            DBG("YmfmWrapper: OPM clock=" + juce::String(opm_clock) + ", chip_rate=" + juce::String(internalSampleRate) + ", output_rate=" + juce::String(outputSampleRate));
        }
    } else {
        internalSampleRate = 55466;  // OPNA internal rate  
        initializeOPNA();
    }
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
    DBG("YmfmWrapper: Creating OPM chip instance");
    std::cout << "YmfmWrapper: Creating OPM chip instance" << std::endl;
    
    opmChip = std::make_unique<ymfm::ym2151>(*this);
    
    DBG("YmfmWrapper: Resetting OPM chip");
    std::cout << "YmfmWrapper: Resetting OPM chip" << std::endl;
    opmChip->reset();
    
    DBG("YmfmWrapper: OPM chip reset complete, setting up voice");
    std::cout << "YmfmWrapper: OPM chip reset complete, setting up voice" << std::endl;
    
    // Setup basic piano voice on all 8 channels
    for (int channel = 0; channel < 8; ++channel) {
        setupBasicPianoVoice(channel);
    }
    
    // Auto-play a note for testing (like sample code) - delay it a bit
    // playTestNote(); // Disable for now, will test via MIDI
    
    DBG("YmfmWrapper: OPM initialization complete");
    std::cout << "YmfmWrapper: OPM initialization complete" << std::endl;
}

void YmfmWrapper::initializeOPNA()
{
    opnaChip = std::make_unique<ymfm::ym2608>(*this);
    opnaChip->reset();
    
    // Enable extended mode (required for OPNA)
    writeRegister(0x29, 0x9f);
    
    // Setup basic piano voice on all 6 FM channels (OPNA has 6 FM channels)
    for (int channel = 0; channel < 6; ++channel) {
        setupBasicPianoVoice(channel);
    }
}

void YmfmWrapper::writeRegister(int address, uint8_t data)
{
    uint8_t addr = static_cast<uint8_t>(address);
    
    // Update register cache
    currentRegisters[addr] = data;
    
    if (chipType == ChipType::OPM && opmChip) {
        DBG("YmfmWrapper: Writing register 0x" + juce::String::toHexString(addr) + " = 0x" + juce::String::toHexString(data));
        std::cout << "YmfmWrapper: Writing register 0x" << std::hex << (int)addr 
                  << " = 0x" << (int)data << std::dec << std::endl;
        
        // Use write_address and write_data like sample code
        opmChip->write_address(addr);
        opmChip->write_data(data);
    } else if (chipType == ChipType::OPNA && opnaChip) {
        DBG("YmfmWrapper: OPNA Writing register 0x" + juce::String::toHexString(addr) + " = 0x" + juce::String::toHexString(data));
        
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
        const float frequency = 440.0f; // A4
        const float amplitude = 0.3f;
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
        if ((debugCounter % 1000 == 0) || hasNonZero) {
            DBG("YmfmWrapper: TEST MODE - generateSamples call #" + juce::String(debugCounter) + ", hasNonZero=" + juce::String(hasNonZero ? 1 : 0) + ", samples=" + juce::String(numSamples));
            std::cout << "YmfmWrapper: TEST MODE - generateSamples call #" << debugCounter << ", hasNonZero=" << hasNonZero 
                      << ", samples=" << numSamples << std::endl;
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
            outputBuffer[i] = opmOutput.data[0] / 32768.0f;
            
            
            if (opmOutput.data[0] != 0) {
                hasNonZero = true;
                lastNonZeroSample = opmOutput.data[0];
                if (abs(opmOutput.data[0]) > abs(maxSample)) {
                    maxSample = opmOutput.data[0];
                }
            }
            
            // Extra detailed debug for first few samples when we might have audio
            if (debugCounter <= 5 && i < 10) {
                DBG("YmfmWrapper: Sample[" + juce::String(i) + "] = " + juce::String(opmOutput.data[0]) + " (0x" + juce::String::toHexString((uint16_t)opmOutput.data[0]) + ")");
            }
        }
        
        // Debug output every few calls to check if function is being called
        debugCounter++;
        if ((debugCounter % 1000 == 0) || hasNonZero) {
            DBG("YmfmWrapper: generateSamples call #" + juce::String(debugCounter) + ", hasNonZero=" + juce::String(hasNonZero ? 1 : 0) + ", maxValue=" + juce::String(maxSample) + ", samples=" + juce::String(numSamples));
            std::cout << "YmfmWrapper: generateSamples call #" << debugCounter << ", hasNonZero=" << hasNonZero 
                      << ", maxValue=" << maxSample << ", samples=" << numSamples << std::endl;
        }
        
    } else if (chipType == ChipType::OPNA && opnaChip) {
        for (int i = 0; i < numSamples; i++) {
            // Generate internal samples - ymfm generate() doesn't take a count parameter
            opnaChip->generate(&opnaOutput);
            
            // Convert to float and store mono (left channel)
            outputBuffer[i] = opnaOutput.data[0] / 32768.0f;
        }
    }
}

void YmfmWrapper::noteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (channel >= 8) return;  // Limit to 8 channels
    
    DBG("YmfmWrapper: noteOn - channel=" + juce::String((int)channel) + ", note=" + juce::String((int)note) + ", velocity=" + juce::String((int)velocity));
    std::cout << "YmfmWrapper: noteOn - channel=" << (int)channel << ", note=" << (int)note << ", velocity=" << (int)velocity << std::endl;
    
    if (chipType == ChipType::OPM) {
        // Calculate frequency from MIDI note
        float freq = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
        
        // Calculate KC and KF using sample code method
        float fnote = 12.0f * log2f(freq / 440.0f) + 69.0f;
        int noteInt = (int)round(fnote);
        int octave = (noteInt / 12) - 1;
        int noteInOctave = noteInt % 12;
        
        // Clamp octave to valid range (0-7) for YM2151
        if (octave < 0) {
            DBG("YmfmWrapper: Low note - original octave=" + juce::String(octave) + ", clamping to 0");
            octave = 0;
        } else if (octave > 7) {
            DBG("YmfmWrapper: High note - original octave=" + juce::String(octave) + ", clamping to 7");
            octave = 7;
        }
        
        // YM2151 key code calculation
        const uint8_t noteCode[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 14};
        uint8_t kc = ((octave & 0x07) << 4) | noteCode[noteInOctave];
        uint8_t kf = 0;  // No fine tuning for now
        
        DBG("YmfmWrapper: MIDI Note " + juce::String((int)note) + " -> freq=" + juce::String(freq) + 
            "Hz, octave=" + juce::String(octave) + ", noteInOctave=" + juce::String(noteInOctave));
        DBG("YmfmWrapper: OPM noteOn - KC=0x" + juce::String::toHexString(kc) + ", KF=0x" + juce::String::toHexString(kf << 2));
        std::cout << "YmfmWrapper: OPM noteOn - KC=0x" << std::hex << (int)kc << ", KF=0x" << (int)(kf << 2) << std::dec << std::endl;
        
        // Write KC and KF
        writeRegister(0x28 + channel, kc);
        writeRegister(0x30 + channel, kf << 2);
        
        // Key On (all operators enabled)
        writeRegister(0x08, 0x78 | channel);
        
        DBG("YmfmWrapper: Key On register 0x08 = 0x" + juce::String::toHexString(0x78 | channel));
        DBG("YmfmWrapper: OPM registers written for note on");
        std::cout << "YmfmWrapper: OPM registers written for note on" << std::endl;
        
    } else if (chipType == ChipType::OPNA) {
        uint8_t block = note / 12 - 1;
        uint16_t fnum = noteToFnum(note);
        
        // F-Number registers
        writeRegister(0xA0 + channel, fnum & 0xFF);
        writeRegister(0xA4 + channel, ((block & 0x07) << 3) | ((fnum >> 8) & 0x07));
        
        // Apply velocity to TL (operator 2)
        uint8_t tl = 127 - velocity;
        writeRegister(0x44 + channel, tl);
        
        // Key On (all operators)
        writeRegister(0x28, 0xF0 | channel);
    }
}

void YmfmWrapper::noteOff(uint8_t channel, uint8_t note)
{
    if (channel >= 8) return;
    
    if (chipType == ChipType::OPM) {
        // Key Off - use sample code format
        writeRegister(0x08, 0x00 | channel);
    } else if (chipType == ChipType::OPNA) {
        // Key Off
        writeRegister(0x28, channel);
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
        DBG("YmfmWrapper: Setting up sine wave timbre for OPM channel " + juce::String((int)channel));
        std::cout << "YmfmWrapper: Setting up sine wave timbre for OPM channel " << (int)channel << std::endl;
        
        // Algorithm 7 (all operators parallel), L/R both output, FB=0
        writeRegister(0x20 + channel, 0xC7);
        
        // Configure all 4 operators like sample code
        for (int op = 0; op < 4; op++) {
            int base_addr = op * 8 + channel;
            
            writeRegister(0x40 + base_addr, 0x01);  // DT1=0, MUL=1
            writeRegister(0x60 + base_addr, 32);    // TL=32 (moderate volume)
            writeRegister(0x80 + base_addr, 0x1F);  // KS=0, AR=31
            writeRegister(0xA0 + base_addr, 0x00);  // AMS-EN=0, D1R=0
            writeRegister(0xC0 + base_addr, 0x00);  // DT2=0, D2R=0
            writeRegister(0xE0 + base_addr, 0xF7);  // D1L=15, RR=7
        }
        
        DBG("YmfmWrapper: OPM voice setup complete (sine wave timbre)");
        std::cout << "YmfmWrapper: OPM voice setup complete (sine wave timbre)" << std::endl;
    }
}

void YmfmWrapper::playTestNote()
{
    if (chipType == ChipType::OPM && opmChip) {
        DBG("YmfmWrapper: Playing test note (C4) for debugging");
        std::cout << "YmfmWrapper: Playing test note (C4) for debugging" << std::endl;
        
        // Play middle C (C4, note 60) with full velocity
        noteOn(0, 60, 127);
    }
}

void YmfmWrapper::setOperatorParameter(uint8_t channel, uint8_t operator_num, OperatorParameter param, uint8_t value)
{
    if (channel >= 8 || operator_num >= 4) return;
    
    if (chipType == ChipType::OPM) {
        int base_addr = operator_num * 8 + channel;
        uint8_t currentValue;
        
        switch (param) {
            case OperatorParameter::TotalLevel:
                writeRegister(0x60 + base_addr, value);
                break;
                
            case OperatorParameter::AttackRate:
                // Keep existing KS bits, update AR
                currentValue = readCurrentRegister(0x80 + base_addr);
                writeRegister(0x80 + base_addr, (currentValue & 0xC0) | (value & 0x1F));
                break;
                
            case OperatorParameter::Decay1Rate:
                // Keep existing AMS-EN bit, update D1R
                currentValue = readCurrentRegister(0xA0 + base_addr);
                writeRegister(0xA0 + base_addr, (currentValue & 0x80) | (value & 0x1F));
                break;
                
            case OperatorParameter::Decay2Rate:
                // Keep existing DT2 bits, update D2R
                currentValue = readCurrentRegister(0xC0 + base_addr);
                writeRegister(0xC0 + base_addr, (currentValue & 0xC0) | (value & 0x1F));
                break;
                
            case OperatorParameter::ReleaseRate:
                // Keep existing D1L bits, update RR
                currentValue = readCurrentRegister(0xE0 + base_addr);
                writeRegister(0xE0 + base_addr, (currentValue & 0xF0) | (value & 0x0F));
                break;
                
            case OperatorParameter::SustainLevel:
                // Keep existing RR bits, update D1L
                currentValue = readCurrentRegister(0xE0 + base_addr);
                writeRegister(0xE0 + base_addr, ((value & 0x0F) << 4) | (currentValue & 0x0F));
                break;
                
            case OperatorParameter::Multiple:
                // Keep existing DT1 bits, update MUL
                currentValue = readCurrentRegister(0x40 + base_addr);
                writeRegister(0x40 + base_addr, (currentValue & 0x70) | (value & 0x0F));
                break;
                
            case OperatorParameter::Detune1:
                // Keep existing MUL bits, update DT1
                currentValue = readCurrentRegister(0x40 + base_addr);
                writeRegister(0x40 + base_addr, ((value & 0x07) << 4) | (currentValue & 0x0F));
                break;
                
            case OperatorParameter::Detune2:
                // Keep existing D2R bits, update DT2
                currentValue = readCurrentRegister(0xC0 + base_addr);
                writeRegister(0xC0 + base_addr, ((value & 0x03) << 6) | (currentValue & 0x1F));
                break;
                
            case OperatorParameter::KeyScale:
                // Keep existing AR bits, update KS
                currentValue = readCurrentRegister(0x80 + base_addr);
                writeRegister(0x80 + base_addr, ((value & 0x03) << 6) | (currentValue & 0x1F));
                break;
        }
    }
}

void YmfmWrapper::setChannelParameter(uint8_t channel, ChannelParameter param, uint8_t value)
{
    if (channel >= 8) return;
    
    if (chipType == ChipType::OPM) {
        uint8_t currentValue = readCurrentRegister(0x20 + channel);
        
        switch (param) {
            case ChannelParameter::Algorithm:
                // Keep existing L/R/FB bits, update ALG
                writeRegister(0x20 + channel, (currentValue & 0xF8) | (value & 0x07));
                break;
                
            case ChannelParameter::Feedback:
                // Keep existing L/R/ALG bits, update FB
                writeRegister(0x20 + channel, (currentValue & 0xC7) | ((value & 0x07) << 3));
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

