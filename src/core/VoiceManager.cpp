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
    // First check if this note is already playing (retriggering)
    int existingChannel = getChannelForNote(note);
    if (existingChannel >= 0) {
        // Update the existing voice
        voices[existingChannel].velocity = velocity;
        voices[existingChannel].timestamp = ++currentTimestamp;
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
    
    CS_DBG(" Allocated note " + juce::String(note) + " to channel " + juce::String(channel));
    return channel;
}

void VoiceManager::releaseVoice(uint8_t note)
{
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == note) {
            voices[i].active = false;
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
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (voices[i].active && voices[i].note == note) {
            return i;
        }
    }
    return -1;  // Note not found
}

int VoiceManager::findAvailableVoice()
{
    // First, look for an inactive voice
    for (int i = 0; i < MAX_VOICES; ++i) {
        if (!voices[i].active) {
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