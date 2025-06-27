#pragma once

#include <cstdint>

/**
 * Interface for voice allocation and management
 * Enables dependency injection and mocking for tests
 */
class VoiceManagerInterface
{
public:
    virtual ~VoiceManagerInterface() = default;
    
    // Voice allocation
    virtual int allocateVoice(uint8_t note, uint8_t velocity) = 0;
    virtual int allocateVoiceWithNoisePriority(uint8_t note, uint8_t velocity, bool needsNoise) = 0;
    virtual void releaseVoice(uint8_t note) = 0;
    virtual void releaseAllVoices() = 0;
    
    // Voice state queries
    virtual bool isVoiceActive(int channel) const = 0;
    virtual uint8_t getNoteForChannel(int channel) const = 0;
    virtual uint8_t getVelocityForChannel(int channel) const = 0;
    virtual int getChannelForNote(uint8_t note) const = 0;
    
    // Voice stealing policy
    enum class StealingPolicy {
        OLDEST,     // Steal the oldest playing voice
        QUIETEST,   // Steal the voice with lowest velocity
        LOWEST      // Steal the lowest pitched voice
    };
    
    virtual void setStealingPolicy(StealingPolicy policy) = 0;
    
    // Reset functionality for testing
    virtual void reset() = 0;
};