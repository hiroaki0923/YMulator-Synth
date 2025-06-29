#pragma once

#include <cstdint>
#include <array>
#include <juce_core/juce_core.h>

namespace ymulatorsynth {

/**
 * @brief Manages hardware register operations for YM2151/YM2608 chips
 * 
 * This class handles direct register read/write operations and maintains
 * a cache of current register values. Extracted from YmfmWrapper as part
 * of Phase 3 Enhanced Abstraction refactoring.
 */
class RegisterManager {
public:
    RegisterManager() = default;
    ~RegisterManager() = default;
    
    /**
     * @brief Write data to a hardware register
     * @param address Register address (0-255)
     * @param data Data to write
     */
    void writeRegister(int address, uint8_t data);
    
    /**
     * @brief Read current value from register cache
     * @param address Register address (0-255)
     * @return Current cached register value
     */
    uint8_t readCurrentRegister(int address) const;
    
    /**
     * @brief Update register cache with new value
     * @param address Register address
     * @param value New value to cache
     */
    void updateRegisterCache(uint8_t address, uint8_t value);
    
    /**
     * @brief Reset all registers to default state
     */
    void reset();
    
private:
    static constexpr size_t REGISTER_COUNT = 256;
    std::array<uint8_t, REGISTER_COUNT> currentRegisters{};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RegisterManager)
};

} // namespace ymulatorsynth