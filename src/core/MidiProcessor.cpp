#include "MidiProcessor.h"
#include "ParameterManager.h"
#include "../dsp/YM2151Registers.h"

namespace ymulatorsynth {

MidiProcessor::MidiProcessor(VoiceManagerInterface& voiceManager,
                           YmfmWrapperInterface& ymfmWrapper,
                           juce::AudioProcessorValueTreeState& parameters,
                           ParameterManager& parameterManager)
    : voiceManager(voiceManager)
    , ymfmWrapper(ymfmWrapper)
    , parameters(parameters)
    , parameterManager(parameterManager)
{
    setupCCMapping();
}

void MidiProcessor::processMidiMessages(juce::MidiBuffer& midiMessages)
{
    // Debug MIDI events
    if (!midiMessages.isEmpty()) {
        CS_DBG(" Received " + juce::String(midiMessages.getNumEvents()) + " MIDI events");
    }
    
    // Process MIDI events
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        
        if (message.isNoteOn()) {
            processMidiNoteOn(message);
        } else if (message.isNoteOff()) {
            processMidiNoteOff(message);
        } else if (message.isController()) {
            CS_DBG(" MIDI CC - CC: " + juce::String(message.getControllerNumber()) + 
                ", Value: " + juce::String(message.getControllerValue()));
            handleMidiCC(message.getControllerNumber(), message.getControllerValue());
        } else if (message.isPitchWheel()) {
            CS_DBG(" Pitch Bend - Value: " + juce::String(message.getPitchWheelValue()));
            handlePitchBend(message.getPitchWheelValue());
        }
    }
}

void MidiProcessor::processMidiNoteOn(const juce::MidiMessage& message)
{
    // Assert valid MIDI note and velocity
    CS_ASSERT_NOTE(message.getNoteNumber());
    CS_ASSERT_VELOCITY(message.getVelocity());
    
    CS_FILE_DBG("MidiProcessor::processMidiNoteOn - Note: " + juce::String(message.getNoteNumber()) + 
        ", Velocity: " + juce::String(message.getVelocity()));
    
    CS_DBG(" Note ON - Note: " + juce::String(message.getNoteNumber()) + 
        ", Velocity: " + juce::String(message.getVelocity()));
    
    // Check if current preset needs noise (has noise enabled)
    bool needsNoise = currentPresetNeedsNoise();
    
    // Allocate a voice for this note with noise priority consideration
    int channel = voiceManager.allocateVoiceWithNoisePriority(message.getNoteNumber(), message.getVelocity(), needsNoise);
    
    // Apply global pan setting to the allocated channel (optimized for real-time)
    auto* panParam = static_cast<juce::AudioParameterChoice*>(parameters.getParameter(ParamID::Global::GlobalPan));
    if (panParam && panParam->getIndex() == static_cast<int>(GlobalPanPosition::RANDOM)) {
        // ALWAYS generate new random pan for each note (not just once per channel)
        setChannelRandomPan(channel);
    }
    applyGlobalPan(channel);
    
    // Tell ymfm to play this note on the allocated channel
    ymfmWrapper.noteOn(channel, message.getNoteNumber(), message.getVelocity());
}

void MidiProcessor::processMidiNoteOff(const juce::MidiMessage& message)
{
    // Assert valid MIDI note
    CS_ASSERT_NOTE(message.getNoteNumber());
    
    CS_FILE_DBG("MidiProcessor::processMidiNoteOff - Note: " + juce::String(message.getNoteNumber()));
    CS_DBG(" Note OFF - Note: " + juce::String(message.getNoteNumber()));
    
    // Find which channel is playing this note
    int channel = voiceManager.getChannelForNote(message.getNoteNumber());
    if (channel >= 0) {
        // Assert valid channel allocation
        CS_ASSERT_CHANNEL(channel);
        
        // Tell ymfm to stop this note
        ymfmWrapper.noteOff(channel, message.getNoteNumber());
        
        // Release the voice
        voiceManager.releaseVoice(message.getNoteNumber());
    }
}

void MidiProcessor::handleMidiCC(int ccNumber, int value)
{
    // Assert valid CC number and value ranges
    CS_ASSERT_PARAMETER_RANGE(ccNumber, 0, 127);
    CS_ASSERT_PARAMETER_RANGE(value, 0, 127);
    
    // Handle channel pan CCs (32-39)
    if (ccNumber >= ParamID::MIDI_CC::Ch0_Pan && ccNumber <= ParamID::MIDI_CC::Ch7_Pan)
    {
        int channel = ccNumber - ParamID::MIDI_CC::Ch0_Pan;
        if (auto* param = dynamic_cast<juce::AudioParameterFloat*>(parameters.getParameter(ParamID::Channel::pan(channel))))
        {
            // Normalize CC value (0-127) to parameter range (0.0-1.0)
            float normalizedValue = juce::jlimit(0.0f, 1.0f, value / 127.0f);
            param->setValueNotifyingHost(normalizedValue);
            
            CS_DBG(" MIDI CC " + juce::String(ccNumber) + " = " + juce::String(value) + 
                " -> Channel " + juce::String(channel) + " Pan = " + juce::String(normalizedValue, 3));
        }
        return;
    }
    
    auto it = ccToParameterMap.find(ccNumber);
    if (it != ccToParameterMap.end() && it->second != nullptr)
    {
        // Normalize CC value (0-127) to parameter range (0.0-1.0)
        float normalizedValue = juce::jlimit(0.0f, 1.0f, value / 127.0f);
        
        // Update parameter (thread-safe)
        it->second->setValueNotifyingHost(normalizedValue);
        
        CS_DBG(" MIDI CC " + juce::String(ccNumber) + " = " + juce::String(value) + 
            " -> " + it->second->name + " = " + juce::String(it->second->getValue()));
    }
}

void MidiProcessor::handlePitchBend(int pitchBendValue)
{
    // Assert valid pitch bend range (14-bit value)
    CS_ASSERT_PARAMETER_RANGE(pitchBendValue, 0, 16383);
    
    // Store the current pitch bend value (0-16383, center is 8192)
    currentPitchBend = pitchBendValue;
    
    // Get pitch bend range from parameter (1-12 semitones)
    int pitchBendRange = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::PitchBendRange));
    
    // Calculate pitch bend amount in semitones
    // MIDI pitch bend: 0-16383, center = 8192
    // Range: -range to +range semitones
    float pitchBendSemitones = ((pitchBendValue - 8192) / 8192.0f) * pitchBendRange;
    
    // Update all active voices with pitch bend
    for (int channel = 0; channel < 8; ++channel)
    {
        if (voiceManager.isVoiceActive(channel))
        {
            uint8_t note = voiceManager.getNoteForChannel(channel);
            uint8_t velocity = voiceManager.getVelocityForChannel(channel);
            
            // Apply pitch bend to the note frequency
            ymfmWrapper.setPitchBend(channel, pitchBendSemitones);
        }
    }
    
    CS_DBG(" Pitch bend applied - Value: " + juce::String(pitchBendValue) + 
        ", Range: " + juce::String(pitchBendRange) + " semitones" +
        ", Amount: " + juce::String(pitchBendSemitones, 3) + " semitones");
}

void MidiProcessor::setupCCMapping()
{
    // VOPMex compatible MIDI CC mapping
    
    // Global parameters
    ccToParameterMap[ParamID::MIDI_CC::Algorithm] = parameters.getParameter(ParamID::Global::Algorithm);
    ccToParameterMap[ParamID::MIDI_CC::Feedback] = parameters.getParameter(ParamID::Global::Feedback);
    ccToParameterMap[ParamID::MIDI_CC::LfoRate] = 
        parameters.getParameter(ParamID::Global::LfoRate);
    ccToParameterMap[ParamID::MIDI_CC::LfoAmd] = 
        parameters.getParameter(ParamID::Global::LfoAmd);
    ccToParameterMap[ParamID::MIDI_CC::LfoPmd] = 
        parameters.getParameter(ParamID::Global::LfoPmd);
    ccToParameterMap[ParamID::MIDI_CC::LfoWaveform] = 
        parameters.getParameter(ParamID::Global::LfoWaveform);
    
    // Noise parameters - AudioParameterBool needs special handling for MIDI CC
    ccToParameterMap[ParamID::MIDI_CC::NoiseEnable] = 
        parameters.getParameter(ParamID::Global::NoiseEnable);
    ccToParameterMap[ParamID::MIDI_CC::NoiseFrequency] = 
        parameters.getParameter(ParamID::Global::NoiseFrequency);
    
    // Operator parameters (Op1-Op4, all 4 operators)
    for (int op = 1; op <= 4; ++op) {
        int baseCC = ParamID::MIDI_CC::Op1_TL + (op - 1) * 11; // 11 CCs per operator
        
        ccToParameterMap[baseCC + 0] = parameters.getParameter(ParamID::Op::tl(op));        // TL
        ccToParameterMap[baseCC + 1] = parameters.getParameter(ParamID::Op::ar(op));        // AR
        ccToParameterMap[baseCC + 2] = parameters.getParameter(ParamID::Op::d1r(op));       // D1R
        ccToParameterMap[baseCC + 3] = parameters.getParameter(ParamID::Op::d2r(op));       // D2R
        ccToParameterMap[baseCC + 4] = parameters.getParameter(ParamID::Op::rr(op));        // RR
        ccToParameterMap[baseCC + 5] = parameters.getParameter(ParamID::Op::d1l(op));       // D1L
        ccToParameterMap[baseCC + 6] = parameters.getParameter(ParamID::Op::ks(op));        // KS
        ccToParameterMap[baseCC + 7] = parameters.getParameter(ParamID::Op::mul(op));       // MUL
        ccToParameterMap[baseCC + 8] = parameters.getParameter(ParamID::Op::dt1(op));       // DT1
        ccToParameterMap[baseCC + 9] = parameters.getParameter(ParamID::Op::dt2(op));       // DT2
        ccToParameterMap[baseCC + 10] = parameters.getParameter(ParamID::Op::ams_en(op));   // AMS-EN
    }
    
    // Note: Channel pan parameters are handled separately in handleMidiCC() 
    // for CCs 32-39 to allow direct channel mapping
}

void MidiProcessor::setChannelRandomPan(int channel)
{
    // Delegate to ParameterManager for consistent random pan handling
    parameterManager.setChannelRandomPan(channel);
}

void MidiProcessor::applyGlobalPan(int channel)
{
    // Delegate to ParameterManager for consistent pan handling
    parameterManager.applyGlobalPan(channel);
}

bool MidiProcessor::currentPresetNeedsNoise() const
{
    return *parameters.getRawParameterValue(ParamID::Global::NoiseEnable) >= 0.5f;
}

} // namespace ymulatorsynth