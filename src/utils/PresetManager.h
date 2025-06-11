#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include "VOPMParser.h"
#include <vector>
#include <memory>

namespace chipsynth {

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
     * Clear all presets
     */
    void clear();
    
    /**
     * Get factory presets (built-in)
     */
    static std::vector<Preset> getFactoryPresets();

private:
    std::vector<Preset> presets;
    
    void loadFactoryPresets();
    juce::File getPresetsDirectory() const;
    void validatePreset(Preset& preset) const;
};

} // namespace chipsynth