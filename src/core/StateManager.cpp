#include "StateManager.h"
#include "ParameterManager.h"
#include "../utils/Debug.h"

namespace ymulatorsynth {

StateManager::StateManager(juce::AudioProcessorValueTreeState& parameters,
                          PresetManagerInterface& presetManager,
                          ParameterManager& parameterManager)
    : parameters(parameters)
    , presetManager(presetManager)
    , parameterManager(parameterManager)
{
    CS_DBG("StateManager created");
}

// ============================================================================
// JUCE AudioProcessor State Interface
// ============================================================================

void StateManager::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    
    // Add current preset number and custom state to state
    state.setProperty("currentPreset", currentPreset, nullptr);
    state.setProperty("isCustomPreset", parameterManager.isInCustomMode(), nullptr);
    state.setProperty("customPresetName", parameterManager.getCustomPresetName(), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    juce::AudioProcessor::copyXmlToBinary(*xml, destData);
    
    CS_DBG("State saved - currentPreset: " + juce::String(currentPreset) + 
           ", isCustom: " + juce::String(parameterManager.isInCustomMode()));
}

void StateManager::setStateInformation(const void* data, int sizeInBytes)
{
    CS_DBG("setStateInformation called - size: " + juce::String(sizeInBytes));
    
    std::unique_ptr<juce::XmlElement> xmlState(juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
    {
        CS_DBG("XML state parsed successfully");
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(newState);
            
            // Restore preset state
            if (newState.hasProperty("currentPreset")) {
                currentPreset = newState.getProperty("currentPreset", 7);
                CS_DBG("Restored currentPreset: " + juce::String(currentPreset));
            }
            
            // Restore custom preset state
            if (newState.hasProperty("isCustomPreset")) {
                bool isCustom = newState.getProperty("isCustomPreset", false);
                juce::String customName = newState.getProperty("customPresetName", "Custom");
                parameterManager.setCustomMode(isCustom, customName);
                CS_DBG("Restored custom preset state - isCustom: " + juce::String(isCustom) + 
                       ", name: " + customName);
            }
            
            CS_DBG("State restored successfully");
        } else {
            CS_DBG("XML state has wrong tag name");
        }
    } else {
        CS_DBG("Failed to parse XML state");
    }
}

// ============================================================================
// JUCE Program Interface
// ============================================================================

int StateManager::getNumPrograms()
{
    // Add 1 for custom preset if active
    return presetManager.getNumPresets() + (parameterManager.isInCustomMode() ? 1 : 0);
}

int StateManager::getCurrentProgram()
{
    if (parameterManager.isInCustomMode()) {
        return presetManager.getNumPresets(); // Custom preset index
    }
    return currentPreset;
}

void StateManager::setCurrentProgram(int index)
{
    CS_DBG("setCurrentProgram called with index: " + juce::String(index) + 
        ", current isCustomPreset: " + juce::String(parameterManager.isInCustomMode() ? "true" : "false"));
    
    // Check if this is the custom preset index
    if (index == presetManager.getNumPresets() && parameterManager.isInCustomMode()) {
        // Stay in custom mode, don't change anything
        CS_DBG("Staying in custom preset mode");
        return;
    }
    
    // Validate preset index
    if (!isValidPresetIndex(index)) {
        CS_DBG("Invalid preset index: " + juce::String(index));
        return;
    }
    
    // Load the preset
    loadPresetInternal(index, true);
    
    CS_DBG("setCurrentProgram completed - new currentPreset: " + juce::String(currentPreset));
}

const juce::String StateManager::getProgramName(int index)
{
    // Handle custom preset case
    if (index == presetManager.getNumPresets() && parameterManager.isInCustomMode()) {
        return parameterManager.getCustomPresetName();
    }
    
    // Validate preset index
    if (!isValidPresetIndex(index)) {
        return "Invalid";
    }
    
    auto preset = presetManager.getPreset(index);
    return preset ? preset->name : "Unknown";
}

void StateManager::changeProgramName(int index, const juce::String& newName)
{
    // JUCE requires this method but we don't support renaming factory presets
    juce::ignoreUnused(index, newName);
    CS_DBG("changeProgramName called but not implemented (factory presets are read-only)");
}

// ============================================================================
// Preset Management
// ============================================================================

void StateManager::loadPreset(int index)
{
    loadPresetInternal(index, true);
}

void StateManager::loadPresetInternal(int index, bool updateCurrentPreset)
{
    if (!isValidPresetIndex(index)) {
        CS_DBG("Cannot load invalid preset index: " + juce::String(index));
        return;
    }
    
    auto preset = presetManager.getPreset(index);
    if (!preset) {
        CS_DBG("Failed to get preset at index: " + juce::String(index));
        return;
    }
    
    CS_DBG("Loading preset " + juce::String(index) + ": " + preset->name);
    
    // Backup current state before loading (for potential undo)
    lastSavedState = parameters.copyState();
    
    // Preserve global pan setting during preset loading
    float preservedGlobalPan = 0.0f;
    
    // Load preset parameters through ParameterManager
    parameterManager.loadPresetParameters(preset, preservedGlobalPan);
    
    // Apply preset to sound generation engine
    parameterManager.applyPresetToYmfm(preset);
    
    // Exit custom mode when loading factory preset
    parameterManager.setCustomMode(false);
    
    // Update current preset tracking
    if (updateCurrentPreset) {
        currentPreset = index;
        hasUnsavedState = false;
    }
    
    CS_DBG("Preset loaded successfully: " + preset->name);
}

void StateManager::saveCurrentState()
{
    lastSavedState = parameters.copyState();
    hasUnsavedState = false;
    CS_DBG("Current state saved");
}

void StateManager::restoreLastState()
{
    if (lastSavedState.isValid()) {
        parameters.replaceState(lastSavedState);
        hasUnsavedState = false;
        CS_DBG("Last state restored");
    } else {
        CS_DBG("No saved state to restore");
    }
}

// ============================================================================
// State Queries
// ============================================================================

bool StateManager::hasUnsavedChanges() const
{
    return hasUnsavedState || parameterManager.isInCustomMode();
}

// ============================================================================
// Internal Helpers
// ============================================================================

void StateManager::markStateChanged()
{
    hasUnsavedState = true;
}

bool StateManager::isValidPresetIndex(int index) const
{
    return index >= 0 && index < presetManager.getNumPresets();
}

} // namespace ymulatorsynth