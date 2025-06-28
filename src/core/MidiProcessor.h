#pragma once

#include "MidiProcessorInterface.h"
#include "VoiceManagerInterface.h"
#include "../dsp/YmfmWrapperInterface.h"
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include <atomic>

namespace ymulatorsynth {

/**
 * Handles MIDI message processing and routing for YMulator-Synth.
 * 
 * Responsibilities:
 * - Process MIDI note on/off messages and route to voice manager
 * - Handle MIDI CC messages with VOPMex-compatible mapping
 * - Process pitch bend messages and apply to active voices
 * - Manage CC-to-parameter mapping configuration
 * 
 * This class extracts MIDI processing logic from PluginProcessor to improve
 * testability and maintain single responsibility principle.
 */
class MidiProcessor : public MidiProcessorInterface {
public:
    /**
     * Construct MidiProcessor with required dependencies.
     * @param voiceManager Voice management interface for note allocation
     * @param ymfmWrapper FM synthesis interface for sound generation
     * @param parameters JUCE parameter tree for CC mapping
     */
    MidiProcessor(VoiceManagerInterface& voiceManager,
                 YmfmWrapperInterface& ymfmWrapper,
                 juce::AudioProcessorValueTreeState& parameters);
    
    virtual ~MidiProcessor() = default;
    
    // MidiProcessorInterface implementation
    void processMidiMessages(juce::MidiBuffer& midiMessages) override;
    void processMidiNoteOn(const juce::MidiMessage& message) override;
    void processMidiNoteOff(const juce::MidiMessage& message) override;
    void handleMidiCC(int ccNumber, int value) override;
    void handlePitchBend(int pitchBendValue) override;
    void setupCCMapping() override;
    
    /**
     * Set channel random pan for global pan randomization feature.
     * @param channel Channel number (0-7)
     */
    void setChannelRandomPan(int channel);
    
    /**
     * Apply global pan setting to specified channel.
     * @param channel Channel number (0-7)  
     */
    void applyGlobalPan(int channel);
    
private:
    // Dependencies (interfaces for testability)
    VoiceManagerInterface& voiceManager;
    YmfmWrapperInterface& ymfmWrapper;
    juce::AudioProcessorValueTreeState& parameters;
    
    // MIDI CC to parameter mapping (VOPMex compatibility)
    std::unordered_map<int, juce::RangedAudioParameter*> ccToParameterMap;
    
    // Current pitch bend value (0-16383, center=8192)
    std::atomic<int> currentPitchBend{8192};
    
    // Channel random pan bits for randomization
    uint8_t channelRandomPanBits[8] = {0};
    
    /**
     * Check if current preset requires noise for voice allocation priority.
     * @return true if noise is enabled in current preset
     */
    bool currentPresetNeedsNoise() const;
};

} // namespace ymulatorsynth