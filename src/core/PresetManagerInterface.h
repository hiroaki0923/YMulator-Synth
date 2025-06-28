#pragma once

#include <string>
#include <vector>
#include <memory>

// Forward declarations
namespace juce {
    class File;
    class String;
    class StringArray;
}

namespace ymulatorsynth {
    struct Preset;
    struct Bank;
}

/**
 * Interface for preset management
 * Enables dependency injection and mocking for tests
 * Matches the existing PresetManager API
 */
class PresetManagerInterface
{
public:
    virtual ~PresetManagerInterface() = default;
    
    // Initialization
    virtual void initialize() = 0;
    
    // File operations
    virtual int loadOPMFile(const juce::File& file) = 0;
    virtual int loadBundledPresets() = 0;
    virtual bool saveOPMFile(const juce::File& file) const = 0;
    virtual bool savePresetAsOPM(const juce::File& file, const ymulatorsynth::Preset& preset) const = 0;
    
    // Preset access
    virtual const ymulatorsynth::Preset* getPreset(int id) const = 0;
    virtual const ymulatorsynth::Preset* getPreset(const juce::String& name) const = 0;
    virtual juce::StringArray getPresetNames() const = 0;
    virtual int getNumPresets() const = 0;
    
    // Bank management
    virtual const std::vector<ymulatorsynth::Bank>& getBanks() const = 0;
    virtual juce::StringArray getPresetsForBank(int bankIndex) const = 0;
    virtual const ymulatorsynth::Preset* getPresetInBank(int bankIndex, int presetIndex) const = 0;
    virtual int getGlobalPresetIndex(int bankIndex, int presetIndex) const = 0;
    
    // Preset modification
    virtual void addPreset(const ymulatorsynth::Preset& preset) = 0;
    virtual void removePreset(int id) = 0;
    virtual void clear() = 0;
    
    // User presets and data persistence
    virtual bool addUserPreset(const ymulatorsynth::Preset& preset) = 0;
    virtual bool saveUserData() = 0;
    virtual int loadUserData() = 0;
    virtual juce::File getUserDataDirectory() const = 0;
    
    // Factory presets
    virtual std::vector<ymulatorsynth::Preset> getFactoryPresets() = 0;
    
    // Reset functionality for testing
    virtual void reset() = 0;
};