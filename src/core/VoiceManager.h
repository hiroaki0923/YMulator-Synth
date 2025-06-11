#pragma once

#include <array>
#include <cstdint>
#include <algorithm>

/**
 * VoiceManager - Manages 8-channel polyphonic voice allocation for YM2151
 * 
 * YM2151 has 8 independent channels, allowing for 8-note polyphony.
 * This class handles voice allocation, voice stealing, and channel management.
 */
class VoiceManager
{
public:
    static constexpr int MAX_VOICES = 8;  // YM2151 has 8 channels
    
    VoiceManager();
    ~VoiceManager() = default;
    
    // Voice allocation
    int allocateVoice(uint8_t note, uint8_t velocity);
    int allocateVoiceWithNoisePriority(uint8_t note, uint8_t velocity, bool needsNoise);
    void releaseVoice(uint8_t note);
    void releaseAllVoices();
    
    // Voice state queries
    bool isVoiceActive(int channel) const;
    uint8_t getNoteForChannel(int channel) const;
    uint8_t getVelocityForChannel(int channel) const;
    int getChannelForNote(uint8_t note) const;
    
    // Voice stealing policy
    enum class StealingPolicy {
        OLDEST,     // Steal the oldest playing voice
        QUIETEST,   // Steal the voice with lowest velocity
        LOWEST      // Steal the lowest pitched voice
    };
    
    void setStealingPolicy(StealingPolicy policy) { stealingPolicy = policy; }
    
private:
    struct Voice {
        bool active = false;
        uint8_t note = 0;
        uint8_t velocity = 0;
        uint64_t timestamp = 0;  // For voice age tracking
    };
    
    std::array<Voice, MAX_VOICES> voices;
    StealingPolicy stealingPolicy = StealingPolicy::OLDEST;
    uint64_t currentTimestamp = 0;
    
    // Find a free voice or steal one according to policy
    int findAvailableVoice();
    int findAvailableVoiceWithNoisePriority(bool needsNoise);
};