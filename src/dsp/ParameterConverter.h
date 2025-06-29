#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>
#include "YmfmWrapperInterface.h"

namespace ymulatorsynth {

/**
 * @brief Converts and validates parameters for YM2151/YM2608 chips
 * 
 * This class handles parameter conversion, validation, and range checking
 * for both operator and channel parameters. Extracted from YmfmWrapper
 * as part of Phase 3 Enhanced Abstraction refactoring.
 */
class ParameterConverter {
public:
    ParameterConverter() = default;
    ~ParameterConverter() = default;
    
    /**
     * @brief Convert and validate operator parameter value
     * @param param Operator parameter type
     * @param value Raw parameter value (0-255)
     * @return Converted value suitable for hardware register
     */
    uint8_t convertOperatorParameter(YmfmWrapperInterface::OperatorParameter param, uint8_t value);
    
    /**
     * @brief Convert and validate channel parameter value
     * @param param Channel parameter type
     * @param value Raw parameter value (0-255) 
     * @return Converted value suitable for hardware register
     */
    uint8_t convertChannelParameter(YmfmWrapperInterface::ChannelParameter param, uint8_t value);
    
    /**
     * @brief Validate parameter value is within specified range
     * @param value Value to validate
     * @param min Minimum allowed value (inclusive)
     * @param max Maximum allowed value (inclusive)
     * @throws Assertion failure if value is out of range
     */
    void validateParameterRange(uint8_t value, uint8_t min, uint8_t max);
    
    /**
     * @brief Get maximum value for a specific operator parameter
     * @param param Operator parameter type
     * @return Maximum allowed value for this parameter
     */
    uint8_t getOperatorParameterMax(YmfmWrapperInterface::OperatorParameter param) const;
    
    /**
     * @brief Get maximum value for a specific channel parameter
     * @param param Channel parameter type
     * @return Maximum allowed value for this parameter
     */
    uint8_t getChannelParameterMax(YmfmWrapperInterface::ChannelParameter param) const;
    
private:
    /**
     * @brief Apply logarithmic scaling to linear parameter value
     * @param linearValue Linear input value (0-255)
     * @param maxValue Maximum output value
     * @return Logarithmically scaled value
     */
    uint8_t applyLogarithmicScaling(uint8_t linearValue, uint8_t maxValue) const;
    
    /**
     * @brief Apply linear scaling to parameter value
     * @param value Input value (0-255)
     * @param maxValue Maximum output value
     * @return Linearly scaled value
     */
    uint8_t applyLinearScaling(uint8_t value, uint8_t maxValue) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterConverter)
};

} // namespace ymulatorsynth