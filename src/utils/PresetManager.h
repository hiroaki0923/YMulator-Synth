#pragma once

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
class PresetManager
{
public:
    PresetManager();
    ~PresetManager() = default;
    
    /**
     * Initialize with factory presets and load external presets
     */
    void initialize();
    
    /**
     * Load presets from OPM file
     * @param file The .opm file to load
     * @return Number of presets loaded
     */
    int loadOPMFile(const juce::File& file);
    
    /**
     * Load presets from bundled resources
     * @return Number of presets loaded
     */
    int loadBundledPresets();
    
    /**
     * Get preset by ID
     * @param id Preset ID (0-based)
     * @return Preset pointer or nullptr if not found
     */
    const Preset* getPreset(int id) const;
    
    /**
     * Get preset by name
     * @param name Preset name
     * @return Preset pointer or nullptr if not found
     */
    const Preset* getPreset(const juce::String& name) const;
    
    /**
     * Get all preset names
     * @return Array of preset names
     */
    juce::StringArray getPresetNames() const;
    
    /**
     * Get number of available presets
     */
    int getNumPresets() const { return static_cast<int>(presets.size()); }
    
    /**
     * Get all banks
     */
    const std::vector<Bank>& getBanks() const { return banks; }
    
    /**
     * Get presets for a specific bank
     * @param bankIndex Index of the bank
     * @return Array of preset names in the bank
     */
    juce::StringArray getPresetsForBank(int bankIndex) const;
    
    /**
     * Get preset by bank and preset index
     * @param bankIndex Index of the bank
     * @param presetIndex Index within the bank
     * @return Preset pointer or nullptr if not found
     */
    const Preset* getPresetInBank(int bankIndex, int presetIndex) const;
    
    /**
     * Get global preset index from bank/preset indices
     * @param bankIndex Index of the bank
     * @param presetIndex Index within the bank
     * @return Global preset index or -1 if not found
     */
    int getGlobalPresetIndex(int bankIndex, int presetIndex) const;
    
    /**
     * Add a new preset
     * @param preset The preset to add
     */
    void addPreset(const Preset& preset);
    
    /**
     * Remove preset by ID
     * @param id Preset ID to remove
     */
    void removePreset(int id);
    
    /**
     * Save current presets to OPM file
     * @param file Target file
     * @return True if successful
     */
    bool saveOPMFile(const juce::File& file) const;
    
    /**
     * Save a single preset as OPM file
     * @param file Target file
     * @param preset Preset to save
     * @return True if successful
     */
    bool savePresetAsOPM(const juce::File& file, const Preset& preset) const;
    
    /**
     * Clear all presets
     */
    void clear();
    
    /**
     * Get factory presets (built-in)
     */
    static std::vector<Preset> getFactoryPresets();
    
    /**
     * Add a preset to the User bank
     * @param preset The preset to add
     * @return True if successful
     */
    bool addUserPreset(const Preset& preset);
    
    /**
     * Save all user data (user presets and imported banks) to persistent storage
     * @return True if successful
     */
    bool saveUserData();
    
    /**
     * Load user data from persistent storage
     * @return Number of user presets/banks loaded
     */
    int loadUserData();
    
    /**
     * Get the user data directory
     * @return User data directory path
     */
    juce::File getUserDataDirectory() const;

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