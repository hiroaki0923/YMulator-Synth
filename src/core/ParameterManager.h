#pragma once

#include "../dsp/YmfmWrapperInterface.h"
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"
#include "../utils/PresetManager.h"
#include "../utils/GlobalPanPosition.h"
#include "PanProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

namespace ymulatorsynth {

// GlobalPanPosition enum moved to utils/GlobalPanPosition.h

/**
 * ParameterManager - Manages all audio parameter operations for YMulator-Synth
 * 
 * Extracted from PluginProcessor to separate parameter management concerns.
 * Handles parameter updates, JUCE parameter system integration, preset loading,
 * and specialized parameter logic like global pan management.
 * 
 * Follows dependency injection pattern for testability and modularity.
 */
class ParameterManager : public juce::AudioProcessorParameter::Listener {
public:
    /**
     * Constructor with dependency injection
     * @param ymfm YmfmWrapper interface for applying parameters to sound engine
     * @param processor AudioProcessor reference for parameter system integration
     * @param panProcessor PanProcessor for handling pan-related functionality
     */
    ParameterManager(YmfmWrapperInterface& ymfm, juce::AudioProcessor& processor, 
                    std::shared_ptr<PanProcessor> panProcessor);
    
    /**
     * Destructor - ensures proper listener cleanup
     */
    ~ParameterManager() override;
    
    // =========================================================================
    // Parameter System Setup
    // =========================================================================
    
    /**
     * Creates the complete JUCE parameter layout for YMulator-Synth
     * @return ParameterLayout with all operator, channel, and global parameters
     */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    /**
     * Initializes the parameter manager with the AudioProcessorValueTreeState
     * Must be called after PluginProcessor creates the parameters ValueTree
     * @param parameters Reference to the main parameter ValueTree
     */
    void initializeParameters(juce::AudioProcessorValueTreeState& parameters);
    
    /**
     * Enables or disables parameter change listeners
     * Used during preset loading to prevent feedback loops
     * @param enable true to enable listeners, false to disable
     */
    void setupParameterListeners(bool enable);
    
    // =========================================================================
    // Core Parameter Management
    // =========================================================================
    
    /**
     * Main parameter synchronization - applies all current parameter values to ymfm
     * Called periodically from audio processing loop with rate limiting
     */
    void updateYmfmParameters();
    
    /**
     * AudioProcessorParameter::Listener implementation
     * Handles parameter value changes from UI or DAW automation
     * @param parameterIndex Index of changed parameter
     * @param newValue New parameter value (0.0-1.0)
     */
    void parameterValueChanged(int parameterIndex, float newValue) override;
    
    /**
     * AudioProcessorParameter::Listener implementation  
     * Tracks gesture state for custom preset detection
     * @param parameterIndex Index of parameter
     * @param gestureIsStarting true when gesture starts, false when ends
     */
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    
    // =========================================================================
    // Preset Parameter Management
    // =========================================================================
    
    /**
     * Loads preset parameter values into JUCE parameter system
     * Temporarily disables listeners to prevent feedback during batch loading
     * @param preset Preset to load parameters from
     * @param preservedGlobalPan Reference to store current global pan value
     */
    void loadPresetParameters(const Preset* preset, float& preservedGlobalPan);
    
    /**
     * Optimized preset application directly to ymfm engine
     * Bypasses JUCE parameter system for faster preset switching
     * @param preset Preset to apply to sound engine
     */
    void applyPresetToYmfm(const Preset* preset);
    
    /**
     * Extracts current parameter values into a preset structure
     * Used for saving current state to OPM files or user presets
     * @param preset Preset structure to populate with current values
     */
    void extractCurrentParameterValues(Preset& preset) const;
    
    // =========================================================================
    // Global Pan Management (Specialized Parameter Handling)
    // =========================================================================
    
    /**
     * Applies global pan setting to a specific channel (delegated to PanProcessor)
     * Handles LEFT/CENTER/RIGHT/RANDOM pan modes with proper register values
     * @param channel Channel number (0-7)
     */
    void applyGlobalPan(int channel);
    
    /**
     * Applies global pan setting to all 8 channels (delegated to PanProcessor)
     * Used during parameter updates and global pan changes
     */
    void applyGlobalPanToAllChannels();
    
    /**
     * Sets random pan value for a specific channel (delegated to PanProcessor)
     * Used in RANDOM global pan mode to vary stereo positioning
     * @param channel Channel number (0-7)
     */
    void setChannelRandomPan(int channel);
    
    // getChannelRandomPanBits moved to PanProcessor
    
    // =========================================================================
    // Custom Preset State Management
    // =========================================================================
    
    /**
     * Check if currently in custom preset mode
     * @return true if user has modified a factory preset
     */
    bool isInCustomMode() const { return isCustomPreset; }
    
    /**
     * Set custom preset mode state
     * @param custom true to enable custom mode
     * @param name Custom preset name (default: "Custom")
     */
    void setCustomMode(bool custom, const juce::String& name = "Custom");
    
    /**
     * Get custom preset name
     * @return Current custom preset name
     */
    const juce::String& getCustomPresetName() const { return customPresetName; }
    
    /**
     * Check if user gesture is in progress
     * @return true if user is currently interacting with parameters
     */
    bool isUserGestureInProgress() const { return userGestureInProgress; }
    
    /**
     * Set user gesture state
     * @param inProgress true if user gesture is starting, false if ending
     */
    void setUserGestureInProgress(bool inProgress) { userGestureInProgress = inProgress; }
    
    // =========================================================================
    // Parameter Access and Utilities
    // =========================================================================
    
    /**
     * Get reference to parameter ValueTree
     * @return Reference to AudioProcessorValueTreeState
     */
    juce::AudioProcessorValueTreeState& getParameters() { return *parametersPtr; }
    const juce::AudioProcessorValueTreeState& getParameters() const { return *parametersPtr; }
    
    
private:
    // =========================================================================
    // Dependencies (Injected)
    // =========================================================================
    
    YmfmWrapperInterface& ymfmWrapper;
    juce::AudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState* parametersPtr = nullptr;
    std::shared_ptr<PanProcessor> panProcessor;
    
    // =========================================================================
    // Parameter Management State
    // =========================================================================
    
    
    /// Custom preset detection and management
    bool isCustomPreset = false;
    juce::String customPresetName = "Custom";
    bool userGestureInProgress = false;
    
    // channelRandomPanBits moved to PanProcessor
    
    // =========================================================================
    // Internal Helper Methods
    // =========================================================================
    
    /**
     * Updates a single channel's parameters to ymfm
     * @param channel Channel number (0-7)
     */
    void updateChannelParameters(int channel);
    
    /**
     * Updates global parameters (LFO, noise, etc.) to ymfm
     */
    void updateGlobalParameters();
    
    /**
     * Validates parameter range and logs warnings if out of bounds
     * @param value Parameter value to validate
     * @param min Minimum allowed value
     * @param max Maximum allowed value
     * @param paramName Parameter name for logging
     */
    void validateParameterRange(float value, float min, float max, const juce::String& paramName) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterManager)
};

} // namespace ymulatorsynth