#pragma once

#include "ymfm_opm.h"
#include "ymfm_opn.h"
#include <array>
#include <cstdint>
#include <memory>

// ymfm interface implementation
class ChipSynthInterface : public ymfm::ymfm_interface
{
public:
    // External memory reads/writes (not used for OPM/OPNA)
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override 
    { 
        return 0; 
    }
    
    void ymfm_external_write(ymfm::access_class type, uint32_t address, uint8_t data) override 
    {
        // Not used for OPM/OPNA
    }
};

// Minimal ymfm wrapper for ChipSynth AU
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
    
    // Register access
    void writeRegister(uint8_t address, uint8_t data);
    
    // Audio generation
    void generateSamples(float* outputBuffer, int numSamples);
    
    // Simple note interface for testing
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t note);
    
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
    
    // ymfm interface - no longer needed since we inherit from ymfm_interface
    // ChipSynthInterface interface;
    
    // ymfm chip instances
    std::unique_ptr<ymfm::ym2151> opmChip;
    std::unique_ptr<ymfm::ym2608> opnaChip;
    
    // Output data holders
    ymfm::ym2151::output_data opmOutput;
    ymfm::ym2608::output_data opnaOutput;
    
    // Helper methods
    void initializeOPM();
    void initializeOPNA();
    uint16_t noteToFnum(uint8_t note);
    void setupBasicPianoVoice(uint8_t channel);
    void playTestNote();
};