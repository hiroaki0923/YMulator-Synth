#pragma once

#include "../core/PresetManagerInterface.h"
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "VOPMParser.h"
#include <vector>
#include <memory>

namespace ymulatorsynth {

/**
 * Preset data structure for internal use
 */
struct Preset
{
    int id = 0;
    juce::String name;
    int algorithm = 0;
    int feedback = 0;
    
    // LFO parameters
    struct LFOData
    {
        int rate = 0;        // LFO frequency (0-255)
        int amd = 0;         // Amplitude modulation depth (0-127)
        int pmd = 0;         // Phase modulation depth (0-127)
        int waveform = 0;    // Waveform (0-3)
        int noiseFreq = 0;   // Noise frequency (0-31)
    } lfo;
    
    // Channel AMS/PMS settings (for all 8 channels)
    struct ChannelData
    {
        int ams = 0;         // Amplitude modulation sensitivity (0-3)
        int pms = 0;         // Phase modulation sensitivity (0-7)
        int noiseEnable = 0; // Noise enable (0-1)
    } channels[8];
    
    struct OperatorData
    {
        float totalLevel = 0.0f;
        float multiple = 1.0f;
        float detune1 = 3.0f;
        float detune2 = 0.0f;
        float keyScale = 0.0f;
        float attackRate = 31.0f;
        float decay1Rate = 0.0f;
        float decay2Rate = 0.0f;
        float releaseRate = 7.0f;
        float sustainLevel = 0.0f;
        bool amsEnable = false;  // AMS enable flag
        bool slotEnable = true;  // SLOT enable flag (default true for compatibility)
    } operators[4];
    
    /**
     * Convert from VOPM voice to internal preset format
     */
    static Preset fromVOPM(const VOPMVoice& voice);
    
    /**
     * Convert to VOPM voice format
     */
    VOPMVoice toVOPM() const;
};

/**
 * Bank information for organizing presets
 */
struct Bank
{
    std::string name;
    std::string fileName; // Original file name for imported banks
    std::vector<int> presetIndices; // Indices into the main presets vector
    
    Bank(const std::string& bankName, const std::string& file = "") 
        : name(bankName), fileName(file) {}
};

/**
 * Manages preset loading, saving, and organization
 */
class PresetManager : public PresetManagerInterface
{
public:
    PresetManager();
    ~PresetManager() = default;
    
    // Interface implementation - Initialization
    void initialize() override;
    
    // Interface implementation - File operations
    int loadOPMFile(const juce::File& file) override;
    int loadBundledPresets() override;
    bool saveOPMFile(const juce::File& file) const override;
    bool savePresetAsOPM(const juce::File& file, const Preset& preset) const override;
    
    // Interface implementation - Preset access
    const Preset* getPreset(int id) const override;
    const Preset* getPreset(const juce::String& name) const override;
    juce::StringArray getPresetNames() const override;
    int getNumPresets() const override { return static_cast<int>(presets.size()); }
    
    // Interface implementation - Bank management
    const std::vector<Bank>& getBanks() const override { return banks; }
    juce::StringArray getPresetsForBank(int bankIndex) const override;
    const Preset* getPresetInBank(int bankIndex, int presetIndex) const override;
    int getGlobalPresetIndex(int bankIndex, int presetIndex) const override;
    
    // Interface implementation - Preset modification
    void addPreset(const Preset& preset) override;
    void removePreset(int id) override;
    void clear() override;
    
    // Interface implementation - User presets and data persistence
    bool addUserPreset(const Preset& preset) override;
    bool saveUserData() override;
    int loadUserData() override;
    juce::File getUserDataDirectory() const override;
    
    // Interface implementation - Factory presets
    std::vector<Preset> getFactoryPresets() override;
    
    // Static factory method (for backward compatibility)
    static std::vector<Preset> createFactoryPresets();
    
    // Interface implementation - Reset functionality for testing
    void reset() override;

private:
    std::vector<Preset> presets;
    std::vector<Bank> banks;
    int userBankIndex = -1;  // Index of the User bank
    
    void loadFactoryPresets();
    void initializeBanks();
    juce::File getPresetsDirectory() const;
    void validatePreset(Preset& preset) const;
    void ensureUserBank();
    bool saveUserPresets();
    bool loadUserPresets();
    bool saveImportedBanks();
    bool loadImportedBanks();
};

} // namespace ymulatorsynth