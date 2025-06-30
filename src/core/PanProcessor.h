#pragma once

#include "../dsp/YmfmWrapperInterface.h"
#include "../utils/Debug.h"
#include "../utils/GlobalPanPosition.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <random>

namespace ymulatorsynth {

/**
 * @class PanProcessor
 * @brief Handles pan-related audio processing and channel management
 * 
 * Extracted from PluginProcessor to improve modularity and separation of concerns.
 * Manages global pan settings (LEFT/CENTER/RIGHT/RANDOM) and applies them to 
 * individual channels in the YM2151 chip emulation.
 * 
 * Key Responsibilities:
 * - Apply global pan settings to specific channels
 * - Handle RANDOM pan mode with per-channel randomization
 * - Manage channel random pan bit storage
 * - Interface with YmfmWrapper for register-level pan control
 * 
 * Design Notes:
 * - Uses dependency injection for YmfmWrapperInterface (testable)
 * - Stateless except for random pan bits storage
 * - Thread-safe for real-time audio processing
 * - Follows single responsibility principle
 */
class PanProcessor {
public:
    /**
     * Constructor with dependency injection
     * @param ymfm YmfmWrapper interface for applying pan to sound engine
     */
    explicit PanProcessor(YmfmWrapperInterface& ymfm);
    
    /**
     * Destructor
     */
    ~PanProcessor() = default;
    
    // Disable copy construction and assignment
    PanProcessor(const PanProcessor&) = delete;
    PanProcessor& operator=(const PanProcessor&) = delete;
    
    // Enable move construction but delete move assignment (due to reference member)
    PanProcessor(PanProcessor&&) = default;
    PanProcessor& operator=(PanProcessor&&) = delete;
    
    // =========================================================================
    // Pan Processing Methods
    // =========================================================================
    
    /**
     * Applies global pan setting to a specific channel
     * Handles LEFT/CENTER/RIGHT/RANDOM pan modes with proper register values
     * @param channel Channel number (0-7)
     * @param globalPanValue Global pan parameter value (0.0-1.0)
     */
    void applyGlobalPan(int channel, float globalPanValue);
    
    /**
     * Applies global pan setting to all 8 channels
     * Used during parameter updates and global pan changes
     * @param globalPanValue Global pan parameter value (0.0-1.0)
     */
    void applyGlobalPanToAllChannels(float globalPanValue);
    
    /**
     * Sets random pan value for a specific channel
     * Used in RANDOM global pan mode to vary stereo positioning
     * @param channel Channel number (0-7)
     */
    void setChannelRandomPan(int channel);
    
    /**
     * Gets random pan bits for a specific channel (for testing/debugging)
     * @param channel Channel number (0-7)
     * @return Random pan bits for the specified channel
     */
    uint8_t getChannelRandomPanBits(int channel) const;
    
    /**
     * Resets all channel random pan bits
     * Used when switching away from RANDOM pan mode
     */
    void resetChannelRandomPanBits();
    
private:
    // =========================================================================
    // Dependencies
    // =========================================================================
    
    /// Reference to YmfmWrapper for applying pan settings to sound engine
    YmfmWrapperInterface& ymfmWrapper;
    
    // =========================================================================
    // State Storage
    // =========================================================================
    
    /// Random pan bits for each channel (used in RANDOM pan mode)
    uint8_t channelRandomPanBits[8] = {0};
    
    /// Random number generator for RANDOM pan mode
    mutable std::mt19937 randomGenerator{std::random_device{}()};
    
    // =========================================================================
    // Internal Helper Methods
    // =========================================================================
    
    /**
     * Converts global pan position enum to YM2151 register value
     * @param position Global pan position
     * @param randomBits Random bits for RANDOM mode (ignored for other modes)
     * @return YM2151 pan register value (0x00, 0x40, 0x80, 0xC0)
     */
    uint8_t convertPanPositionToRegisterValue(ymulatorsynth::GlobalPanPosition position, uint8_t randomBits = 0) const;
    
    /**
     * Converts float parameter value to GlobalPanPosition enum
     * @param panValue Parameter value (0.0-1.0)
     * @return Corresponding GlobalPanPosition
     */
    ymulatorsynth::GlobalPanPosition convertParameterToPosition(float panValue) const;
};

} // namespace ymulatorsynth