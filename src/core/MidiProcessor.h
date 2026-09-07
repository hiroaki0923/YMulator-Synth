#pragma once

#include "MidiProcessorInterface.h"
#include "VoiceManagerInterface.h"
#include "../dsp/YmfmWrapperInterface.h"
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <unordered_map>
#include "HeldNotes.h"
#include <atomic>

namespace ymulatorsynth {

// Forward declaration
class ParameterManager;

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
     * @param parameterManager Parameter management for pan operations
     */
    MidiProcessor(VoiceManagerInterface& voiceManager,
                 YmfmWrapperInterface& ymfmWrapper,
                 juce::AudioProcessorValueTreeState& parameters,
                 ParameterManager& parameterManager);
    
    ~MidiProcessor() override = default;
    
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
    
    /** Notes held while mono or arpeggio mode is on; read by the motion engine. */
    const ymulatorsynth::HeldNotes& getHeldNotes() const { return held; }
    
private:
    ymulatorsynth::HeldNotes held;
    int keysDown = 0;            // keys physically held, as opposed to notes latched into the arpeggio
    bool monoModeOn() const;
    bool arpeggioOn() const;
    bool arpLatchOn() const;
public:
    /** Latch was switched off: let go of a chord nobody is holding any more. */
    void releaseLatchedNotes();
private:
    // Dependencies (interfaces for testability)
    VoiceManagerInterface& voiceManager;
    YmfmWrapperInterface& ymfmWrapper;
    juce::AudioProcessorValueTreeState& parameters;
    ParameterManager& parameterManager;
    
    // MIDI CC to parameter mapping (VOPMex compatibility)
    struct CcTarget {
        juce::RangedAudioParameter* param = nullptr;
        bool reversed = false;   // TL / AR / D1R / D2R / D1L / RR run opposite to the register in natural mode
        bool normalized = false; // Quick / motion amounts: the CC is a position in the range, not a register value
    };
    bool expressiveMode() const;
    void resetMacros();
    std::unordered_map<int, CcTarget> ccToParameterMap;
    
    // VOPMex CC value interpretation: natural (scaled, some reversed) by default,
    // register values after NRPN 126/127 (or 126/0) data 127
    bool registerValueMode = false;
    int nrpnMsb = -1;
    int nrpnLsb = -1;
    int lfoRateLsbBit = 0;
    
    void applyCcToParameter(int ccNumber, int value, const CcTarget& target);
    
    // Current pitch bend value (0-16383, center=8192)
    std::atomic<int> currentPitchBend{8192};
    
    /**
     * Check if current preset requires noise for voice allocation priority.
     * @return true if noise is enabled in current preset
     */
    bool currentPresetNeedsNoise() const;
};

} // namespace ymulatorsynth