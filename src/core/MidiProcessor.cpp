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
        } else if (message.isChannelPressure()) {
            // Expressive mode: pressure opens the sound up through the Brightness macro
            if (expressiveMode())
                if (auto* p = parameters.getParameter(ParamID::Macro::Brightness))
                    p->setValueNotifyingHost(0.5f + message.getChannelPressureValue() / 254.0f);
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
    
    // Mono / arpeggio: held notes share one channel; a second note retunes it (legato) or joins the arpeggio
    const bool mono = monoModeOn(), arp = arpeggioOn();
    if (mono || arp) {
        // A latched chord is replaced by the first note of the next one; the channel keeps sounding
        if (arp && arpLatchOn() && keysDown == 0 && held.count > 0) { held.count = 0; ++held.version; }
        ++keysDown;
        held.add(static_cast<uint8_t>(message.getNoteNumber()));
        if (held.channel >= 0 && voiceManager.isVoiceActive(held.channel)) {
            if (!arp) {
                ymfmWrapper.retuneChannel(static_cast<uint8_t>(held.channel), static_cast<uint8_t>(message.getNoteNumber()));
                voiceManager.setNoteForChannel(held.channel, static_cast<uint8_t>(message.getNoteNumber()));
            }
            return;
        }
    } else if (held.count > 0) {
        held.clear();
        keysDown = 0;
    }
    
    // Check if current preset needs noise (has noise enabled)
    bool needsNoise = currentPresetNeedsNoise();
    
    // Allocate a voice for this note with noise priority consideration
    int channel = voiceManager.allocateVoiceWithNoisePriority(message.getNoteNumber(), message.getVelocity(), needsNoise);
    
    // Tell ymfm to play this note on the allocated channel
    ymfmWrapper.noteOn(channel, message.getNoteNumber(), message.getVelocity());
    if (mono || arp) held.channel = channel;
}

bool MidiProcessor::monoModeOn() const
{
    auto* p = parameters.getParameter(ParamID::Motion::Mono);
    return p != nullptr && p->getValue() > 0.5f;
}

bool MidiProcessor::arpeggioOn() const
{
    auto* p = parameters.getParameter(ParamID::Motion::ArpMode);
    return p != nullptr && juce::roundToInt(p->convertFrom0to1(p->getValue())) > 0;
}

bool MidiProcessor::arpLatchOn() const
{
    auto* p = parameters.getParameter(ParamID::Motion::ArpLatch);
    return p != nullptr && p->getValue() > 0.5f;
}

void MidiProcessor::releaseLatchedNotes()
{
    if (keysDown > 0 || held.channel < 0) return;
    const int ch = held.channel;
    if (voiceManager.isVoiceActive(ch)) {
        const uint8_t sounding = voiceManager.getNoteForChannel(ch);
        ymfmWrapper.noteOff(static_cast<uint8_t>(ch), sounding);
        voiceManager.releaseVoice(sounding);
    }
    held.clear();
}

void MidiProcessor::processMidiNoteOff(const juce::MidiMessage& message)
{
    // Assert valid MIDI note
    CS_ASSERT_NOTE(message.getNoteNumber());
    
    CS_FILE_DBG("MidiProcessor::processMidiNoteOff - Note: " + juce::String(message.getNoteNumber()));
    CS_DBG(" Note OFF - Note: " + juce::String(message.getNoteNumber()));
    
    // Mono / arpeggio: only the last held note releases the channel; otherwise fall back to an earlier note
    if (held.channel >= 0 && held.contains(static_cast<uint8_t>(message.getNoteNumber()))) {
        const int ch = held.channel;
        keysDown = juce::jmax(0, keysDown - 1);
        // Latched: the chord stays in the arpeggio until the next chord starts
        if (arpeggioOn() && arpLatchOn()) return;
        held.remove(static_cast<uint8_t>(message.getNoteNumber()));
        if (held.count == 0) {
            const uint8_t sounding = voiceManager.getNoteForChannel(ch);
            ymfmWrapper.noteOff(static_cast<uint8_t>(ch), sounding);
            voiceManager.releaseVoice(sounding);
            held.channel = -1;
        } else if (!arpeggioOn() && voiceManager.getNoteForChannel(ch) == message.getNoteNumber()) {
            ymfmWrapper.retuneChannel(static_cast<uint8_t>(ch), held.last());
            voiceManager.setNoteForChannel(ch, held.last());
        }
        return;
    }
    
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
    
    // NRPN: VOPMex uses MSB 126 / LSB 127 (all channels) or 0 (this channel) with
    // data 127 to switch to register-value input, data 0 back to natural
    if (ccNumber == ParamID::MIDI_CC::NrpnMsb) { nrpnMsb = value; return; }
    if (ccNumber == ParamID::MIDI_CC::NrpnLsb) { nrpnLsb = value; return; }
    if (ccNumber == ParamID::MIDI_CC::DataEntry) {
        if (nrpnMsb == ParamID::MIDI_CC::NrpnCcDirectionMsb && (nrpnLsb == 127 || nrpnLsb == 0))
            registerValueMode = (value != 0);
        return;
    }
    if (ccNumber == ParamID::MIDI_CC::ResetAllControllers) {
        registerValueMode = false;
        nrpnMsb = nrpnLsb = -1;
        resetMacros();
        return;
    }
    if (ccNumber == ParamID::MIDI_CC::LfoRateLsb) { lfoRateLsbBit = value >> 6; return; }
    if (ccNumber == ParamID::MIDI_CC::ModWheel && expressiveMode()) {
        if (auto* p = parameters.getParameter(ParamID::Motion::VibratoDepth)) p->setValueNotifyingHost(value / 127.0f);
        return;
    }
    
    auto it = ccToParameterMap.find(ccNumber);
    if (it != ccToParameterMap.end() && it->second.param != nullptr)
        applyCcToParameter(ccNumber, value, it->second);
}

void MidiProcessor::applyCcToParameter(int ccNumber, int value, const CcTarget& target)
{
    if (target.normalized) {
        target.param->setValueNotifyingHost(value / 127.0f);
        return;
    }
    const auto& range = target.param->getNormalisableRange();
    const int maxValue = static_cast<int>(range.end);
    float registerValue;
    
    if (ccNumber == ParamID::MIDI_CC::LfoRate) {
        // 8-bit LFRQ: CC 1 carries the upper 7 bits, CC 33 the lowest bit
        registerValue = static_cast<float>((value << 1) | lfoRateLsbBit);
    } else if (registerValueMode) {
        registerValue = static_cast<float>(value & maxValue);
    } else {
        // Natural mode: scale the 7-bit CC to the parameter's step count
        // (value >> (7 - bits(max))), then reverse the envelope-type parameters
        int bits = 0;
        for (int m = maxValue; m > 0; m >>= 1) ++bits;
        const int shift = juce::jmax(0, 7 - bits);
        int scaled = value >> shift;
        if (target.reversed) scaled = maxValue - scaled;
        registerValue = static_cast<float>(scaled);
    }
    
    registerValue = juce::jlimit(range.start, range.end, registerValue);
    target.param->setValueNotifyingHost(range.convertTo0to1(registerValue));
    
    CS_DBG(" MIDI CC " + juce::String(ccNumber) + " = " + juce::String(value) + 
        " -> " + target.param->name + " = " + juce::String(registerValue));
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
    using namespace ParamID;
    auto direct = [&](int cc, const juce::String& id) { ccToParameterMap[cc] = { parameters.getParameter(id), false, false }; };
    auto reversed = [&](int cc, const juce::String& id) { ccToParameterMap[cc] = { parameters.getParameter(id), true, false }; };
    auto position = [&](int cc, const juce::String& id) { ccToParameterMap[cc] = { parameters.getParameter(id), false, true }; };
    
    direct(MIDI_CC::Algorithm, Global::Algorithm);
    direct(MIDI_CC::Feedback, Global::Feedback);
    
    // Hardware LFO and channel sensitivity, VOPMex numbers
    direct(MIDI_CC::LfoRate, Global::LfoRate);
    direct(MIDI_CC::LfoAmd, Global::LfoAmd);
    direct(MIDI_CC::LfoPmd, Global::LfoPmd);
    direct(MIDI_CC::LfoWaveform, Global::LfoWaveform);
    direct(MIDI_CC::LfoPms, Global::LfoPms);
    direct(MIDI_CC::LfoAms, Global::LfoAms);
    
    // Quick view: macros and motion amounts as positions
    position(MIDI_CC::QuickBrightness, Macro::Brightness);
    position(MIDI_CC::QuickHarmonics, Macro::Harmonics);
    position(MIDI_CC::QuickAttack, Macro::Attack);
    position(MIDI_CC::QuickDecay, Macro::Decay);
    position(MIDI_CC::QuickRelease, Macro::Release);
    position(MIDI_CC::QuickSpread, Macro::Spread);
    position(MIDI_CC::MotionWide, Motion::Wide);
    position(MIDI_CC::MotionVibrato, Motion::VibratoDepth);
    position(MIDI_CC::MotionTimbre, Motion::TimbreDepth);
    position(MIDI_CC::MotionEcho, Motion::EchoLevel);
    position(MIDI_CC::MotionSweep, Motion::SweepAmount);
    position(MIDI_CC::MotionSwell, Motion::LevelAttack);
    position(MIDI_CC::MotionPorta, Motion::PortaTime);
    position(MIDI_CC::MotionPitch, Motion::PitchEnv);
    position(MIDI_CC::MotionVelBright, Motion::VelBright);
    position(MIDI_CC::ArpChord, Motion::ArpChord);
    position(MIDI_CC::ArpOctaves, Motion::ArpOctaves);
    position(MIDI_CC::ArpGate, Motion::ArpGate);
    
    // Noise
    direct(MIDI_CC::NoiseEnable, Global::NoiseEnable);
    for (int cc : {MIDI_CC::NoiseFrequency, MIDI_CC::LegacyNoiseFrequency}) direct(cc, Global::NoiseFrequency);
    
    // Operator parameters: four consecutive CCs per parameter (OP1-OP4).
    // Level and envelope rates/levels are reversed in natural mode, as in VOPMex.
    for (int op = 1; op <= 4; ++op) {
        reversed(MIDI_CC::getOpCC(op, Op::TotalLevel),   Op::tl(op));
        direct  (MIDI_CC::getOpCC(op, Op::Multiple),     Op::mul(op));
        direct  (MIDI_CC::getOpCC(op, Op::Detune1),      Op::dt1(op));
        direct  (MIDI_CC::getOpCC(op, Op::Detune2),      Op::dt2(op));
        direct  (MIDI_CC::getOpCC(op, Op::KeyScale),     Op::ks(op));
        reversed(MIDI_CC::getOpCC(op, Op::AttackRate),   Op::ar(op));
        reversed(MIDI_CC::getOpCC(op, Op::Decay1Rate),   Op::d1r(op));
        reversed(MIDI_CC::getOpCC(op, Op::Decay2Rate),   Op::d2r(op));
        reversed(MIDI_CC::getOpCC(op, Op::SustainLevel), Op::d1l(op));
        reversed(MIDI_CC::getOpCC(op, Op::ReleaseRate),  Op::rr(op));
        direct  (MIDI_CC::getOpCC(op, Op::AmsEnable),    Op::ams_en(op));
    }
    
}

bool MidiProcessor::expressiveMode() const
{
    auto* p = parameters.getParameter(ParamID::Global::Expressive);
    return p != nullptr && p->getValue() > 0.5f;
}

void MidiProcessor::resetMacros()
{
    for (const char* id : { ParamID::Macro::Brightness, ParamID::Macro::Harmonics, ParamID::Macro::Attack,
                            ParamID::Macro::Decay, ParamID::Macro::Release, ParamID::Macro::Spread })
        if (auto* p = parameters.getParameter(id)) p->setValueNotifyingHost(p->getDefaultValue());
}

bool MidiProcessor::currentPresetNeedsNoise() const
{
    return *parameters.getRawParameterValue(ParamID::Global::NoiseEnable) >= 0.5f;
}

} // namespace ymulatorsynth