#include "PanProcessor.h"
#include "../dsp/YM2151Registers.h"
#include <juce_core/juce_core.h>

namespace ymulatorsynth {

PanProcessor::PanProcessor(YmfmWrapperInterface& ymfm)
    : ymfmWrapper(ymfm)
{
    // Initialize all channels to center pan (YM2151 default)
    for (int i = 0; i < 8; ++i) {
        channelRandomPanBits[i] = YM2151Regs::PAN_CENTER;
    }
    
    CS_DBG("PanProcessor initialized with center pan for all channels");
}

void PanProcessor::applyGlobalPan(int channel, float globalPanValue)
{
    CS_ASSERT_CHANNEL(channel);
    
    ymulatorsynth::GlobalPanPosition position = convertParameterToPosition(globalPanValue);
    float panValue = 0.5f; // Default to center
    
    switch (position) {
        case ymulatorsynth::GlobalPanPosition::LEFT:
            panValue = 0.0f;  // Fully left
            break;
        case ymulatorsynth::GlobalPanPosition::CENTER:
            panValue = 0.5f;  // Center
            break;
        case ymulatorsynth::GlobalPanPosition::RIGHT:
            panValue = 1.0f;  // Fully right
            break;
        case ymulatorsynth::GlobalPanPosition::RANDOM:
            // Convert YM2151 register bits to normalized values
            if (channelRandomPanBits[channel] == YM2151Regs::PAN_LEFT_ONLY) {
                panValue = 0.0f;
            } else if (channelRandomPanBits[channel] == YM2151Regs::PAN_RIGHT_ONLY) {
                panValue = 1.0f;
            } else {
                panValue = 0.5f;  // PAN_CENTER
            }
            break;
    }
    
    ymfmWrapper.setChannelPan(channel, panValue);
    
    CS_FILE_DBG("applyGlobalPan - Channel " + juce::String(channel) + 
                " pan mode " + juce::String(static_cast<int>(position)) + 
                " value " + juce::String(panValue, 3));
}

void PanProcessor::applyGlobalPanToAllChannels(float globalPanValue)
{
    CS_FILE_DBG("applyGlobalPanToAllChannels - Applying to all 8 channels");
    
    for (int channel = 0; channel < 8; ++channel) {
        applyGlobalPan(channel, globalPanValue);
    }
}

void PanProcessor::setChannelRandomPan(int channel)
{
    CS_ASSERT_CHANNEL(channel);
    
    // Generate random pan: LEFT (0x40), CENTER (0xC0), or RIGHT (0x80)
    static const uint8_t panValues[] = {
        YM2151Regs::PAN_LEFT_ONLY,   // 0x40
        YM2151Regs::PAN_CENTER,      // 0xC0  
        YM2151Regs::PAN_RIGHT_ONLY   // 0x80
    };
    
    // Get current value to ensure we generate a different one
    uint8_t currentValue = channelRandomPanBits[channel];
    uint8_t newValue;
    
    // Generate a different value from the current one to ensure variation
    do {
        std::uniform_int_distribution<int> distribution(0, 2);
        int randomIndex = distribution(randomGenerator);
        newValue = panValues[randomIndex];
    } while (newValue == currentValue && std::uniform_real_distribution<float>(0.0f, 1.0f)(randomGenerator) < 0.8f);
    // 20% chance to allow same value (prevents infinite loop in edge cases)
    
    channelRandomPanBits[channel] = newValue;
    
    CS_FILE_DBG("setChannelRandomPan - Channel " + juce::String(channel) + 
                " random pan: 0x" + juce::String::toHexString(newValue) + 
                " (changed from 0x" + juce::String::toHexString(currentValue) + ")");
}

uint8_t PanProcessor::getChannelRandomPanBits(int channel) const
{
    CS_ASSERT_CHANNEL(channel);
    return channelRandomPanBits[channel];
}

void PanProcessor::resetChannelRandomPanBits()
{
    for (int i = 0; i < 8; ++i) {
        channelRandomPanBits[i] = YM2151Regs::PAN_CENTER;
    }
    
    CS_DBG("Reset all channel random pan bits to center");
}

uint8_t PanProcessor::convertPanPositionToRegisterValue(ymulatorsynth::GlobalPanPosition position, uint8_t randomBits) const
{
    switch (position) {
        case ymulatorsynth::GlobalPanPosition::LEFT:
            return YM2151Regs::PAN_LEFT_ONLY;
        case ymulatorsynth::GlobalPanPosition::CENTER:
            return YM2151Regs::PAN_CENTER;
        case ymulatorsynth::GlobalPanPosition::RIGHT:
            return YM2151Regs::PAN_RIGHT_ONLY;
        case ymulatorsynth::GlobalPanPosition::RANDOM:
            return randomBits; // Use pre-calculated random bits
        default:
            return YM2151Regs::PAN_CENTER;
    }
}

ymulatorsynth::GlobalPanPosition PanProcessor::convertParameterToPosition(float panValue) const
{
    // Convert 0.0-1.0 parameter value to enum
    // Based on JUCE AudioParameterChoice mapping:
    // 0 = LEFT, 1 = CENTER, 2 = RIGHT, 3 = RANDOM
    
    if (panValue <= 0.25f) {
        return ymulatorsynth::GlobalPanPosition::LEFT;
    } else if (panValue <= 0.5f) {
        return ymulatorsynth::GlobalPanPosition::CENTER;
    } else if (panValue <= 0.75f) {
        return ymulatorsynth::GlobalPanPosition::RIGHT;
    } else {
        return ymulatorsynth::GlobalPanPosition::RANDOM;
    }
}

} // namespace ymulatorsynth