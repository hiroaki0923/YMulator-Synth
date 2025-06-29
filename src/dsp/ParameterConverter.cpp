#include "ParameterConverter.h"
#include "YM2151Registers.h"
#include "../utils/Debug.h"
#include <algorithm>
#include <cmath>

using namespace ymulatorsynth;

uint8_t ParameterConverter::convertOperatorParameter(YmfmWrapperInterface::OperatorParameter param, uint8_t value)
{
    uint8_t convertedValue = 0;
    
    switch (param) {
        case YmfmWrapperInterface::OperatorParameter::AttackRate:
            validateParameterRange(value, 0, 31);
            convertedValue = value & YM2151Regs::MASK_ATTACK_RATE;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::Decay1Rate:
            validateParameterRange(value, 0, 31);
            convertedValue = value & YM2151Regs::MASK_DECAY1_RATE;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::Decay2Rate:
            validateParameterRange(value, 0, 31);
            convertedValue = value & YM2151Regs::MASK_DECAY2_RATE;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::ReleaseRate:
            validateParameterRange(value, 0, 15);
            convertedValue = value & YM2151Regs::MASK_RELEASE_RATE;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::SustainLevel:
            validateParameterRange(value, 0, 15);
            convertedValue = value & YM2151Regs::MASK_SUSTAIN_LEVEL;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::TotalLevel:
            validateParameterRange(value, 0, 127);
            convertedValue = value & 0x7F; // 7-bit value
            break;
            
        case YmfmWrapperInterface::OperatorParameter::KeyScale:
            validateParameterRange(value, 0, 3);
            convertedValue = value & YM2151Regs::MASK_KEY_SCALE;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::Multiple:
            validateParameterRange(value, 0, 15);
            convertedValue = value & YM2151Regs::MASK_MULTIPLE;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::Detune1:
            validateParameterRange(value, 0, 7);
            convertedValue = value & YM2151Regs::MASK_DETUNE1;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::Detune2:
            validateParameterRange(value, 0, 3);
            convertedValue = value & YM2151Regs::MASK_DETUNE2;
            break;
            
        case YmfmWrapperInterface::OperatorParameter::AmsEnable:
            validateParameterRange(value, 0, 1);
            convertedValue = value & 0x01;
            break;
            
        default:
            CS_DBG("ParameterConverter::convertOperatorParameter - Unknown parameter type");
            convertedValue = value;
            break;
    }
    
    CS_DBG("ParameterConverter::convertOperatorParameter - param: " + 
           juce::String(static_cast<int>(param)) + ", input: " + juce::String(value) + 
           ", output: " + juce::String(convertedValue));
    
    return convertedValue;
}

uint8_t ParameterConverter::convertChannelParameter(YmfmWrapperInterface::ChannelParameter param, uint8_t value)
{
    uint8_t convertedValue = 0;
    
    switch (param) {
        case YmfmWrapperInterface::ChannelParameter::Algorithm:
            validateParameterRange(value, 0, 7);
            convertedValue = value & YM2151Regs::MASK_ALGORITHM;
            break;
            
        case YmfmWrapperInterface::ChannelParameter::Feedback:
            validateParameterRange(value, 0, 7);
            convertedValue = value & YM2151Regs::MASK_FEEDBACK;
            break;
            
        case YmfmWrapperInterface::ChannelParameter::Pan:
            validateParameterRange(value, 0, 3);
            // Convert pan value to L/R enable bits
            switch (value) {
                case 0: convertedValue = YM2151Regs::PAN_OFF; break;
                case 1: convertedValue = YM2151Regs::PAN_LEFT_ONLY; break;
                case 2: convertedValue = YM2151Regs::PAN_CENTER; break;
                case 3: convertedValue = YM2151Regs::PAN_RIGHT_ONLY; break;
                default: convertedValue = YM2151Regs::PAN_CENTER; break;
            }
            break;
            
        case YmfmWrapperInterface::ChannelParameter::AMS:
            validateParameterRange(value, 0, 3);
            convertedValue = value & YM2151Regs::MASK_LFO_AMS;
            break;
            
        case YmfmWrapperInterface::ChannelParameter::PMS:
            validateParameterRange(value, 0, 7);
            convertedValue = value & YM2151Regs::MASK_LFO_PMS;
            break;
            
        default:
            CS_DBG("ParameterConverter::convertChannelParameter - Unknown parameter type");
            convertedValue = value;
            break;
    }
    
    CS_DBG("ParameterConverter::convertChannelParameter - param: " + 
           juce::String(static_cast<int>(param)) + ", input: " + juce::String(value) + 
           ", output: " + juce::String(convertedValue));
    
    return convertedValue;
}

void ParameterConverter::validateParameterRange(uint8_t value, uint8_t min, uint8_t max)
{
    CS_ASSERT_PARAMETER_RANGE(value, min, max);
    
    if (value < min || value > max) {
        CS_DBG("ParameterConverter::validateParameterRange - Value " + juce::String(value) + 
               " out of range [" + juce::String(min) + ", " + juce::String(max) + "]");
    }
}

uint8_t ParameterConverter::getOperatorParameterMax(YmfmWrapperInterface::OperatorParameter param) const
{
    switch (param) {
        case YmfmWrapperInterface::OperatorParameter::AttackRate:
        case YmfmWrapperInterface::OperatorParameter::Decay1Rate:
        case YmfmWrapperInterface::OperatorParameter::Decay2Rate:
            return 31;
            
        case YmfmWrapperInterface::OperatorParameter::ReleaseRate:
        case YmfmWrapperInterface::OperatorParameter::SustainLevel:
        case YmfmWrapperInterface::OperatorParameter::Multiple:
            return 15;
            
        case YmfmWrapperInterface::OperatorParameter::TotalLevel:
            return 127;
            
        case YmfmWrapperInterface::OperatorParameter::KeyScale:
        case YmfmWrapperInterface::OperatorParameter::Detune2:
            return 3;
            
        case YmfmWrapperInterface::OperatorParameter::Detune1:
            return 7;
            
        case YmfmWrapperInterface::OperatorParameter::AmsEnable:
            return 1;
            
        default:
            return 255; // Default maximum
    }
}

uint8_t ParameterConverter::getChannelParameterMax(YmfmWrapperInterface::ChannelParameter param) const
{
    switch (param) {
        case YmfmWrapperInterface::ChannelParameter::Algorithm:
        case YmfmWrapperInterface::ChannelParameter::Feedback:
        case YmfmWrapperInterface::ChannelParameter::PMS:
            return 7;
            
        case YmfmWrapperInterface::ChannelParameter::Pan:
        case YmfmWrapperInterface::ChannelParameter::AMS:
            return 3;
            
        default:
            return 255; // Default maximum
    }
}

uint8_t ParameterConverter::applyLogarithmicScaling(uint8_t linearValue, uint8_t maxValue) const
{
    if (linearValue == 0) return 0;
    
    // Apply logarithmic curve: output = maxValue * (log(1 + input/255) / log(2))
    float normalizedInput = static_cast<float>(linearValue) / 255.0f;
    float logValue = std::log(1.0f + normalizedInput) / std::log(2.0f);
    uint8_t result = static_cast<uint8_t>(logValue * static_cast<float>(maxValue));
    
    return std::min(result, maxValue);
}

uint8_t ParameterConverter::applyLinearScaling(uint8_t value, uint8_t maxValue) const
{
    // Simple linear scaling: output = (input * maxValue) / 255
    uint32_t scaled = (static_cast<uint32_t>(value) * static_cast<uint32_t>(maxValue)) / 255;
    return static_cast<uint8_t>(std::min(scaled, static_cast<uint32_t>(maxValue)));
}