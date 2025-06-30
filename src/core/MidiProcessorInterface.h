#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace ymulatorsynth {

/**
 * Interface for MIDI message processing and routing.
 * 
 * Handles MIDI events including note on/off, control changes, and pitch bend.
 * Provides abstraction for MIDI processing to enable testing and modular design.
 */
class MidiProcessorInterface {
public:
    virtual ~MidiProcessorInterface() = default;
    
    /**
     * Process all MIDI messages in the buffer for the current audio block.
     * @param midiMessages Buffer containing MIDI events to process
     */
    virtual void processMidiMessages(juce::MidiBuffer& midiMessages) = 0;
    
    /**
     * Process a MIDI note on message.
     * @param message MIDI note on message containing note number and velocity
     */
    virtual void processMidiNoteOn(const juce::MidiMessage& message) = 0;
    
    /**
     * Process a MIDI note off message.
     * @param message MIDI note off message containing note number
     */
    virtual void processMidiNoteOff(const juce::MidiMessage& message) = 0;
    
    /**
     * Handle MIDI control change messages (CC).
     * @param ccNumber Control change number (0-127)
     * @param value Control change value (0-127)
     */
    virtual void handleMidiCC(int ccNumber, int value) = 0;
    
    /**
     * Handle MIDI pitch bend messages.
     * @param pitchBendValue 14-bit pitch bend value (0-16383, center=8192)
     */
    virtual void handlePitchBend(int pitchBendValue) = 0;
    
    /**
     * Setup MIDI CC to parameter mapping for VOPMex compatibility.
     * Called during initialization to establish CC routing.
     */
    virtual void setupCCMapping() = 0;
};

} // namespace ymulatorsynth