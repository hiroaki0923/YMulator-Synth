#include "YmfmWrapper.h"
#include <memory>
#include <cmath>
#include <iostream>
#include <os/log.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

YmfmWrapper::YmfmWrapper()
    : chipType(ChipType::OPM)
    , outputSampleRate(44100)
    , internalSampleRate(62500)
{
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
            os_log_t logger = os_log_create("com.vendor.chipsynth", "ymfm");
            os_log(logger, "YmfmWrapper: OPM clock=%u, chip_rate=%u, output_rate=%u", 
                   opm_clock, internalSampleRate, outputSampleRate);
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
    os_log_t logger = os_log_create("com.vendor.chipsynth", "ymfm");
    os_log(logger, "YmfmWrapper: Creating OPM chip instance");
    std::cout << "YmfmWrapper: Creating OPM chip instance" << std::endl;
    
    opmChip = std::make_unique<ymfm::ym2151>(*this);
    
    os_log(logger, "YmfmWrapper: Resetting OPM chip");
    std::cout << "YmfmWrapper: Resetting OPM chip" << std::endl;
    opmChip->reset();
    
    os_log(logger, "YmfmWrapper: OPM chip reset complete, setting up voice");
    std::cout << "YmfmWrapper: OPM chip reset complete, setting up voice" << std::endl;
    
    // Setup basic piano voice on channel 0
    setupBasicPianoVoice(0);
    
    // Auto-play a note for testing (like sample code) - delay it a bit
    // playTestNote(); // Disable for now, will test via MIDI
    
    os_log(logger, "YmfmWrapper: OPM initialization complete");
    std::cout << "YmfmWrapper: OPM initialization complete" << std::endl;
}

void YmfmWrapper::initializeOPNA()
{
    opnaChip = std::make_unique<ymfm::ym2608>(*this);
    opnaChip->reset();
    
    // Enable extended mode (required for OPNA)
    writeRegister(0x29, 0x9f);
    
    // Setup basic piano voice on channel 0
    setupBasicPianoVoice(0);
}

void YmfmWrapper::writeRegister(uint8_t address, uint8_t data)
{
    if (chipType == ChipType::OPM && opmChip) {
        os_log_t logger = os_log_create("com.vendor.chipsynth", "ymfm");
        os_log(logger, "YmfmWrapper: Writing register 0x%02X = 0x%02X", address, data);
        std::cout << "YmfmWrapper: Writing register 0x" << std::hex << (int)address 
                  << " = 0x" << (int)data << std::dec << std::endl;
        
        // Use write_address and write_data like sample code
        opmChip->write_address(address);
        opmChip->write_data(data);
    } else if (chipType == ChipType::OPNA && opnaChip) {
        os_log_t logger = os_log_create("com.vendor.chipsynth", "ymfm");
        os_log(logger, "YmfmWrapper: OPNA Writing register 0x%02X = 0x%02X", address, data);
        
        opnaChip->write_address(address);
        opnaChip->write_data(data);
    }
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
            os_log_t logger = os_log_create("com.vendor.chipsynth", "audio");
            os_log(logger, "YmfmWrapper: TEST MODE - generateSamples call #%d, hasNonZero=%d, samples=%d", 
                   debugCounter, hasNonZero ? 1 : 0, numSamples);
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
                os_log_t logger = os_log_create("com.vendor.chipsynth", "ymfm");
                os_log(logger, "YmfmWrapper: Sample[%d] = %d (0x%04X)", i, opmOutput.data[0], (uint16_t)opmOutput.data[0]);
            }
        }
        
        // Debug output every few calls to check if function is being called
        debugCounter++;
        if ((debugCounter % 1000 == 0) || hasNonZero) {
            os_log_t logger = os_log_create("com.vendor.chipsynth", "audio");
            os_log(logger, "YmfmWrapper: generateSamples call #%d, hasNonZero=%d, maxValue=%d, samples=%d", 
                   debugCounter, hasNonZero ? 1 : 0, maxSample, numSamples);
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
    
    os_log_t logger = os_log_create("com.vendor.chipsynth", "midi");
    os_log(logger, "YmfmWrapper: noteOn - channel=%d, note=%d, velocity=%d", (int)channel, (int)note, (int)velocity);
    std::cout << "YmfmWrapper: noteOn - channel=" << (int)channel << ", note=" << (int)note << ", velocity=" << (int)velocity << std::endl;
    
    if (chipType == ChipType::OPM) {
        // Calculate frequency from MIDI note
        float freq = 440.0f * std::pow(2.0f, (note - 69) / 12.0f);
        
        // Calculate KC and KF using sample code method
        float fnote = 12.0f * log2f(freq / 440.0f) + 69.0f;
        int noteInt = (int)round(fnote);
        int octave = (noteInt / 12) - 1;
        int noteInOctave = noteInt % 12;
        
        // YM2151 key code calculation
        const uint8_t noteCode[12] = {0, 1, 2, 4, 5, 6, 8, 9, 10, 11, 13, 14};
        uint8_t kc = ((octave & 0x07) << 4) | noteCode[noteInOctave];
        uint8_t kf = 0;  // No fine tuning for now
        
        os_log(logger, "YmfmWrapper: OPM noteOn - KC=0x%02X, KF=0x%02X", (int)kc, (int)(kf << 2));
        std::cout << "YmfmWrapper: OPM noteOn - KC=0x" << std::hex << (int)kc << ", KF=0x" << (int)(kf << 2) << std::dec << std::endl;
        
        // Write KC and KF
        writeRegister(0x28 + channel, kc);
        writeRegister(0x30 + channel, kf << 2);
        
        // Key On (all operators enabled)
        writeRegister(0x08, 0x78 | channel);
        
        os_log(logger, "YmfmWrapper: Key On register 0x08 = 0x%02X", 0x78 | channel);
        os_log(logger, "YmfmWrapper: OPM registers written for note on");
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
        os_log_t logger = os_log_create("com.vendor.chipsynth", "setup");
        os_log(logger, "YmfmWrapper: Setting up sine wave timbre for OPM channel %d", (int)channel);
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
        
        os_log(logger, "YmfmWrapper: OPM voice setup complete (sine wave timbre)");
        std::cout << "YmfmWrapper: OPM voice setup complete (sine wave timbre)" << std::endl;
    }
}

void YmfmWrapper::playTestNote()
{
    if (chipType == ChipType::OPM && opmChip) {
        os_log_t logger = os_log_create("com.vendor.chipsynth", "test");
        os_log(logger, "YmfmWrapper: Playing test note (C4) for debugging");
        std::cout << "YmfmWrapper: Playing test note (C4) for debugging" << std::endl;
        
        // Play middle C (C4, note 60) with full velocity
        noteOn(0, 60, 127);
    }
}