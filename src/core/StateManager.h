#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../core/PresetManagerInterface.h"

namespace ymulatorsynth {

// Forward declarations
class ParameterManager;

/**
 * Handles all plugin state management and preset operations.
 * 
 * Responsibilities:
 * - Plugin state serialization/deserialization
 * - JUCE program interface implementation (preset switching)
 * - Current preset tracking and custom preset state
 * - Integration with PresetManager for preset loading
 * 
 * This class extracts state management logic from PluginProcessor
 * to improve separation of concerns and testability.
 */
class StateManager {
public:
    /**
     * Construct StateManager with required dependencies.
     * @param parameters JUCE parameter tree for state management
     * @param presetManager Preset management interface
     * @param parameterManager Parameter management for custom state tracking
     */
    StateManager(juce::AudioProcessorValueTreeState& parameters,
                PresetManagerInterface& presetManager,
                ParameterManager& parameterManager);
    
    ~StateManager() = default;
    
    // JUCE AudioProcessor interface implementation
    void getStateInformation(juce::MemoryBlock& destData);
    void setStateInformation(const void* data, int sizeInBytes);
    
    // JUCE program interface implementation
    int getNumPrograms();
    int getCurrentProgram();
    void setCurrentProgram(int index);
    const juce::String getProgramName(int index);
    void changeProgramName(int index, const juce::String& newName);
    
    // State management utilities
    void loadPreset(int index);
    void saveCurrentState();
    void restoreLastState();
    
    // Preset state queries
    bool hasUnsavedChanges() const;
    int getCurrentPresetIndex() const { return currentPreset; }
    
private:
    // Dependencies
    juce::AudioProcessorValueTreeState& parameters;
    PresetManagerInterface& presetManager;
    ParameterManager& parameterManager;
    
    // Current state tracking
    int currentPreset{7}; // Default to init preset (index 7)
    bool hasUnsavedState{false};
    
    // State backup for undo functionality
    juce::ValueTree lastSavedState;
    
    /**
     * Internal helper to load preset and update state tracking.
     * @param index Preset index to load
     * @param updateCurrentPreset Whether to update currentPreset variable
     */
    void loadPresetInternal(int index, bool updateCurrentPreset = true);
    
    /**
     * Update state tracking after parameter changes.
     */
    void markStateChanged();
    
    /**
     * Validate preset index is in valid range.
     * @param index Preset index to validate
     * @return true if index is valid
     */
    bool isValidPresetIndex(int index) const;
};

} // namespace ymulatorsynth