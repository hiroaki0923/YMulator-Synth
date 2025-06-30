#pragma once

namespace ymulatorsynth {

/**
 * @enum GlobalPanPosition
 * @brief Enumeration for global pan position settings
 * 
 * Defines the available global pan positions that can be applied to all channels.
 * Used in conjunction with JUCE AudioParameterChoice to provide user-selectable
 * pan modes in the YMulator-Synth interface.
 * 
 * Values correspond to the parameter choices in ParameterManager::createParameterLayout()
 */
enum class GlobalPanPosition {
    LEFT = 0,      ///< All channels panned fully left
    CENTER = 1,    ///< All channels panned to center (default)
    RIGHT = 2,     ///< All channels panned fully right
    RANDOM = 3     ///< Each channel gets random pan position (L/C/R)
};

} // namespace ymulatorsynth