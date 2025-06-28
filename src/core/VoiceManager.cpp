#include "VoiceManager.h"
#include "../utils/Debug.h"
#include <juce_core/juce_core.h>

VoiceManager::VoiceManager()
{
    // Initialize all voices as inactive
    for (auto& voice : voices) {
        voice.active = false;
        voice.note = 0;
        voice.velocity = 0;
        voice.timestamp = 0;
    }
}

int VoiceManager::allocateVoice(uint8_t note, uint8_t velocity)
{
    CS_ASSERT_NOTE(note);
    CS_ASSERT_VELOCITY(velocity);
    // First check if this note is already playing (retriggering)
    int existingChannel = getChannelForNote(note);
    if (existingChannel >= 0) {
        // Update the existing voice
        voices[existingChannel].velocity = velocity;
        voices[existingChannel].timestamp = ++currentTimestamp;
        CS_FILE_DBG(" Retriggering note " + juce::String(note) + " on channel " + juce::String(existingChannel));
        CS_DBG(" Retriggering note " + juce::String(note) + " on channel " + juce::String(existingChannel));
        return existingChannel;
    }
    
    // Find an available voice
    int channel = findAvailableVoice();
    
    // Allocate the voice
    voices[channel].active = true;
    voices[channel].note = note;
    voices[channel].velocity = velocity;
    voices[channel].timestamp = ++currentTimestamp;
    
    CS_FILE_DBG(" Allocated note " + juce::String(note) + " to channel " + juce::String(channel));
    CS_DBG(" Allocated note " + juce::String(note) + " to channel " + juce::String(channel));
    return channel;
}

int VoiceManager::allocateVoiceWithNoisePriority(uint8_t note, uint8_t velocity, bool needsNoise)
{
    CS_ASSERT_NOTE(note);
    CS_ASSERT_VELOCITY(velocity);
    
    // First check if this note is already playing (retriggering)
    int existingChannel = getChannelForNote(note);
    if (existingChannel >= 0) {
        // Update the existing voice
        voices[existingChannel].velocity = velocity;
        voices[existingChannel].timestamp = ++currentTimestamp;
        CS_DBG(" Retriggering note " + juce::String(note) + " on channel " + juce::String(existingChannel) + 
               (needsNoise ? " (noise-enabled)" : ""));
        return existingChannel;
    }
    
    // Find an available voice with noise priority
    int channel = findAvailableVoiceWithNoisePriority(needsNoise);
    
    // Allocate the voice
    voices[channel].active = true;
    voices[channel].note = note;
    voices[channel].velocity = velocity;
    voices[channel].timestamp = ++currentTimestamp;
    
    CS_FILE_DBG(" Allocated note " + juce::String(note) + " to channel " + juce::String(channel) + 
           (needsNoise ? " (noise-enabled preset)" : ""));
    CS_DBG(" Allocated note " + juce::String(note) + " to channel " + juce::String(channel) + 
           (needsNoise ? " (noise-enabled preset)" : ""));
    return channel;
}

void VoiceManager::releaseVoice(uint8_t note)
{
    CS_ASSERT_NOTE(note);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == note) {
            voices[i].active = false;
            CS_FILE_DBG(" Released note " + juce::String(note) + " from channel " + juce::String(i));
            CS_DBG(" Released note " + juce::String(note) + " from channel " + juce::String(i));
            return;
        }
    }
    CS_DBG(" Note " + juce::String(note) + " not found for release");
}

void VoiceManager::releaseAllVoices()
{
    for (auto& voice : voices) {
        voice.active = false;
    }
    CS_DBG(" Released all voices");
}

bool VoiceManager::isVoiceActive(int channel) const
{
    if (channel < 0 || channel >= MAX_VOICES) return false;
    return voices[channel].active;
}

uint8_t VoiceManager::getNoteForChannel(int channel) const
{
    if (channel < 0 || channel >= MAX_VOICES) return 0;
    return voices[channel].note;
}

uint8_t VoiceManager::getVelocityForChannel(int channel) const
{
    if (channel < 0 || channel >= MAX_VOICES) return 0;
    return voices[channel].velocity;
}

int VoiceManager::getChannelForNote(uint8_t note) const
{
    CS_ASSERT_NOTE(note);
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == note) {
            return i;
        }
    }
    return -1;  // Note not found
}

int VoiceManager::findAvailableVoice()
{
    // First, look for an inactive voice, starting from channel 7 (noise-capable)
    // This prioritizes noise-capable channel 7 for rhythm instruments
    for (int i = MAX_VOICES - 1; i >= 0; --i) {
        CS_FILE_DBG("Checking voice " + juce::String(i) + " - active: " + juce::String(voices[i].active ? "true" : "false"));
        if (!voices[i].active) {
            CS_FILE_DBG("Found available voice: " + juce::String(i));
            return i;
        }
    }
    
    // All voices are active, need to steal one
    int victimChannel = 0;
    
    switch (stealingPolicy) {
        case StealingPolicy::OLDEST:
            // Find the voice with the smallest timestamp
            for (int i = 1; i < MAX_VOICES; ++i) {
                if (voices[i].timestamp < voices[victimChannel].timestamp) {
                    victimChannel = i;
                }
            }
            CS_DBG(" Stealing oldest voice on channel " + juce::String(victimChannel));
            break;
            
        case StealingPolicy::QUIETEST:
            // Find the voice with the lowest velocity
            for (int i = 1; i < MAX_VOICES; ++i) {
                if (voices[i].velocity < voices[victimChannel].velocity) {
                    victimChannel = i;
                }
            }
            CS_DBG(" Stealing quietest voice on channel " + juce::String(victimChannel));
            break;
            
        case StealingPolicy::LOWEST:
            // Find the voice with the lowest note
            for (int i = 1; i < MAX_VOICES; ++i) {
                if (voices[i].note < voices[victimChannel].note) {
                    victimChannel = i;
                }
            }
            CS_DBG(" Stealing lowest voice on channel " + juce::String(victimChannel));
            break;
    }
    
    return victimChannel;
}

int VoiceManager::findAvailableVoiceWithNoisePriority(bool needsNoise)
{
    CS_FILE_DBG("findAvailableVoiceWithNoisePriority - needsNoise: " + juce::String(needsNoise ? "true" : "false"));
    
    if (needsNoise) {
        // For noise-enabled presets, ONLY use channel 7
        CS_FILE_DBG("Noise preset: checking only channel 7 - active: " + juce::String(voices[7].active ? "true" : "false"));
        
        if (!voices[7].active) {
            CS_FILE_DBG("Allocating channel 7 for noise preset");
            CS_DBG(" Allocating channel 7 for noise preset");
            return 7;
        }
        
        // If channel 7 is busy, steal it for noise presets (noise presets ONLY use channel 7)
        CS_FILE_DBG("Channel 7 busy, stealing it for noise preset (noise ONLY uses channel 7)");
        CS_DBG(" Channel 7 busy, stealing it for noise preset");
        return 7;
    } else {
        // For non-noise presets, start from channel 7 and work downward (7→6→5→4→3→2→1→0)
        CS_FILE_DBG("Non-noise preset: checking channels 7-0 in order");
        for (int i = MAX_VOICES - 1; i >= 0; --i) {  // Start from 7, go down to 0
            CS_FILE_DBG("Checking channel " + juce::String(i) + " for non-noise preset - active: " + juce::String(voices[i].active ? "true" : "false"));
            if (!voices[i].active) {
                CS_FILE_DBG("Found available channel " + juce::String(i) + " for non-noise preset");
                return i;
            }
        }
        
        // All voices active, use normal stealing policy
        CS_FILE_DBG("All channels busy, using stealing policy");
        return findAvailableVoice();
    }
}

void VoiceManager::reset()
{
    // Reset all voices to inactive state
    for (auto& voice : voices) {
        voice.active = false;
        voice.note = 0;
        voice.velocity = 0;
        voice.timestamp = 0;
    }
    
    // Reset timestamp counter
    currentTimestamp = 0;
    
    // Reset stealing policy to default
    stealingPolicy = StealingPolicy::OLDEST;
    
    CS_DBG("VoiceManager reset to initial state");
}