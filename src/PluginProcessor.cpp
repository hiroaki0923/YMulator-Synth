#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/Debug.h"
#include "utils/ParameterIDs.h"

YMulatorSynthAudioProcessor::ChipSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("YMulatorSynth"), createParameterLayout())
{
    CS_DBG(" Constructor called");
    
    setupCCMapping();
    
    // Initialize preset manager
    presetManager.initialize();
    
    // Load default preset (Init) before adding listener
    setCurrentPreset(7); // Init preset
    
    // Add parameter change listener through ValueTree after initial setup
    parameters.state.addListener(this);
    
    // Add parameter listeners for user-initiated changes
    const auto& allParams = AudioProcessor::getParameters();
    for (auto* param : allParams) {
        param->addListener(this);
    }
    
    CS_DBG(" Constructor completed - default preset: " + juce::String(currentPreset));
}

YMulatorSynthAudioProcessor::~ChipSynthAudioProcessor()
{
    // Remove ValueTree listener
    parameters.state.removeListener(this);
    
    // Remove parameter listeners
    const auto& allParams = AudioProcessor::getParameters();
    for (auto* param : allParams) {
        param->removeListener(this);
    }
}

const juce::String YMulatorSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool YMulatorSynthAudioProcessor::acceptsMidi() const
{
    return true;
}

bool YMulatorSynthAudioProcessor::producesMidi() const
{
    return false;
}

bool YMulatorSynthAudioProcessor::isMidiEffect() const
{
    return false;
}

double YMulatorSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int YMulatorSynthAudioProcessor::getNumPrograms()
{
    // Add 1 for custom preset if active
    return presetManager.getNumPresets() + (isCustomPreset ? 1 : 0);
}

int YMulatorSynthAudioProcessor::getCurrentProgram()
{
    if (isCustomPreset) {
        return presetManager.getNumPresets(); // Custom preset index
    }
    return currentPreset;
}

void YMulatorSynthAudioProcessor::setCurrentProgram(int index)
{
    CS_DBG(" setCurrentProgram called with index: " + juce::String(index) + 
        ", current isCustomPreset: " + juce::String(isCustomPreset ? "true" : "false"));
    
    // Check if this is the custom preset index
    if (index == presetManager.getNumPresets() && isCustomPreset) {
        // Stay in custom mode, don't change anything
        CS_DBG(" Staying in custom preset mode");
        return;
    }
    
    // Reset custom state and load factory preset
    isCustomPreset = false;
    
    // Notify UI components of custom mode change
    parameters.state.setProperty("isCustomMode", false, nullptr);
    
    CS_DBG(" Reset isCustomPreset to false, calling setCurrentPreset");
    setCurrentPreset(index);
    
    // Notify host about program change
    updateHostDisplay();
    
    // Notify the UI to update the preset combo box
    // Send a special property change to trigger UI update without affecting custom state
    juce::MessageManager::callAsync([this]() {
        parameters.state.sendPropertyChangeMessage("presetIndexChanged");
    });
}

const juce::String YMulatorSynthAudioProcessor::getProgramName(int index)
{
    // Handle custom preset case
    if (index == presetManager.getNumPresets() && isCustomPreset) {
        return customPresetName;
    }
    
    if (index >= 0 && index < presetManager.getNumPresets())
    {
        auto presetNames = presetManager.getPresetNames();
        return presetNames[index];
    }
    
    return {};
}

void YMulatorSynthAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void YMulatorSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Assert valid sample rate and buffer size
    CS_ASSERT_SAMPLE_RATE(sampleRate);
    CS_ASSERT_BUFFER_SIZE(samplesPerBlock);
    
    juce::ignoreUnused(samplesPerBlock);
    
    // Debug output
    CS_DBG(" prepareToPlay called - sampleRate: " + juce::String(sampleRate) + 
        ", samplesPerBlock: " + juce::String(samplesPerBlock) + 
        ", currentPreset: " + juce::String(currentPreset));
    
    // Initialize ymfm wrapper with OPM for now
    ymfmWrapper.initialize(YmfmWrapper::ChipType::OPM, static_cast<uint32_t>(sampleRate));
    
    // Apply initial parameters from current preset
    updateYmfmParameters();
    
    // If a preset was set before ymfm was initialized, apply it now
    if (needsPresetReapply) {
        loadPreset(currentPreset);
        needsPresetReapply = false;
        CS_DBG(" Applied deferred preset " + juce::String(currentPreset));
    }
    
    CS_DBG(" ymfm initialization complete");
}

void YMulatorSynthAudioProcessor::releaseResources()
{
}

bool YMulatorSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void YMulatorSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
    // Assert buffer validity
    CS_ASSERT_BUFFER_SIZE(buffer.getNumSamples());
    CS_ASSERT(buffer.getNumChannels() >= 1 && buffer.getNumChannels() <= 2);
    
    juce::ScopedNoDenormals noDenormals;
    
    static int processBlockCallCounter = 0;
    static bool hasLoggedFirstCall = false;
    
    processBlockCallCounter++;
    
    if (!hasLoggedFirstCall) {
        CS_DBG(" processBlock FIRST CALL - channels: " + juce::String(buffer.getNumChannels()) + 
            ", samples: " + juce::String(buffer.getNumSamples()));
        hasLoggedFirstCall = true;
    }
    
    // Clear output buffer
    buffer.clear();
    
    // Debug MIDI events
    if (!midiMessages.isEmpty()) {
        CS_DBG(" Received " + juce::String(midiMessages.getNumEvents()) + " MIDI events");
    }
    
    // Process MIDI events
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        
        if (message.isNoteOn()) {
            // Assert valid MIDI note and velocity
            CS_ASSERT_NOTE(message.getNoteNumber());
            CS_ASSERT_VELOCITY(message.getVelocity());
            
            CS_DBG(" Note ON - Note: " + juce::String(message.getNoteNumber()) + 
                ", Velocity: " + juce::String(message.getVelocity()));
            
            // Check if current preset needs noise (has noise enabled)
            bool currentPresetNeedsNoise = *parameters.getRawParameterValue(ParamID::Global::NoiseEnable) >= 0.5f;
            
            // Allocate a voice for this note with noise priority consideration
            int channel = voiceManager.allocateVoiceWithNoisePriority(message.getNoteNumber(), message.getVelocity(), currentPresetNeedsNoise);
            
            // Tell ymfm to play this note on the allocated channel
            ymfmWrapper.noteOn(channel, message.getNoteNumber(), message.getVelocity());
            
        } else if (message.isNoteOff()) {
            // Assert valid MIDI note
            CS_ASSERT_NOTE(message.getNoteNumber());
            
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
        } else if (message.isController()) {
            CS_DBG(" MIDI CC - CC: " + juce::String(message.getControllerNumber()) + 
                ", Value: " + juce::String(message.getControllerValue()));
            handleMidiCC(message.getControllerNumber(), message.getControllerValue());
        } else if (message.isPitchWheel()) {
            CS_DBG(" Pitch Bend - Value: " + juce::String(message.getPitchWheelValue()));
            handlePitchBend(message.getPitchWheelValue());
        }
    }
    
    // Update parameters periodically
    if (++parameterUpdateCounter >= PARAMETER_UPDATE_RATE_DIVIDER)
    {
        parameterUpdateCounter = 0;
        updateYmfmParameters();
    }
    
    // Generate audio
    const int numSamples = buffer.getNumSamples();
    static int audioCallCounter = 0;
    audioCallCounter++;
    
    if (numSamples > 0) {
        if (audioCallCounter % 1000 == 0) {  // Log every 1000 calls to avoid spam
            CS_DBG(" processBlock audio generation - call #" + juce::String(audioCallCounter) + 
                ", numSamples: " + juce::String(numSamples));
        }
        
        // Generate true stereo output
        float* leftBuffer = buffer.getWritePointer(0);
        float* rightBuffer = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : leftBuffer;
        
        ymfmWrapper.generateSamples(leftBuffer, rightBuffer, numSamples);
        
        // Apply moderate gain to prevent clipping
        buffer.applyGain(0, 0, numSamples, 2.0f);
        if (buffer.getNumChannels() > 1) {
            buffer.applyGain(1, 0, numSamples, 2.0f);
        }
        
        // Check for non-zero audio (debug)
        static int debugCounter = 0;
        if (++debugCounter % 44100 == 0) { // Every ~1 second at 44.1kHz
            bool hasAudio = false;
            for (int i = 0; i < numSamples; i++) {
                if (std::abs(buffer.getSample(0, i)) > 0.0001f) {
                    hasAudio = true;
                    break;
                }
            }
            CS_DBG(" Audio check - " + juce::String(hasAudio ? "HAS AUDIO" : "SILENT"));
        }
    }
}

bool YMulatorSynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* YMulatorSynthAudioProcessor::createEditor()
{
    return new YMulatorSynthAudioProcessorEditor(*this);
}

void YMulatorSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    
    // Add current preset number and custom state to state
    state.setProperty("currentPreset", currentPreset, nullptr);
    state.setProperty("isCustomPreset", isCustomPreset, nullptr);
    state.setProperty("customPresetName", customPresetName, nullptr);
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
    
    CS_DBG(" State saved - preset: " + juce::String(currentPreset) + 
        ", custom: " + juce::String(isCustomPreset ? "true" : "false"));
}

void YMulatorSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    CS_DBG(" setStateInformation called - size: " + juce::String(sizeInBytes));
    
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
    {
        CS_DBG(" XML state parsed successfully");
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(newState);
            
            // Restore preset number and custom state
            currentPreset = newState.getProperty("currentPreset", 0);
            isCustomPreset = newState.getProperty("isCustomPreset", false);
            customPresetName = newState.getProperty("customPresetName", "Custom");
            
            CS_DBG(" State loaded - preset: " + juce::String(currentPreset) + 
                ", custom: " + juce::String(isCustomPreset ? "true" : "false"));
            
            // If not in custom mode and ymfm is initialized, apply the preset
            if (!isCustomPreset && ymfmWrapper.isInitialized()) {
                CS_DBG(" Applying preset after state restore");
                setCurrentPreset(currentPreset);
            } else if (!isCustomPreset) {
                CS_DBG(" Deferring preset application until ymfm init");
                needsPresetReapply = true;
            } else {
                CS_DBG(" Staying in custom mode after state restore");
            }
            
            // Update host display
            updateHostDisplay();
        } else {
            CS_DBG(" XML state tag mismatch");
        }
    } else {
        CS_DBG(" Failed to parse XML state");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout YMulatorSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Global parameters
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::Algorithm, "Algorithm", 0, 7, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::Feedback, "Feedback", 0, 7, 0));
    
    // Pitch bend parameters
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::PitchBendRange, "Pitch Bend Range", 1, 12, 2)); // Default 2 semitones
    
    // LFO parameters
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoRate, "LFO Rate", 0, 255, 0)); // 0-255
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoAmd, "LFO AMD", 0, 127, 0)); // 0-127
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoPmd, "LFO PMD", 0, 127, 0)); // 0-127
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoWaveform, "LFO Waveform", 0, 3, 0)); // 0=Saw, 1=Square, 2=Triangle, 3=Noise
    
    // Noise parameters
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::Global::NoiseEnable, "Noise Enable", false)); // Default OFF
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::NoiseFrequency, "Noise Frequency", 0, 31, 16)); // Default medium frequency
    
    // Channel pan parameters (channels 0-7)
    for (int ch = 0; ch < 8; ++ch)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            ParamID::Channel::pan(ch), "Channel " + juce::String(ch) + " Pan", 
            0.0f, 1.0f, 0.5f)); // Default 0.5 (center)
            
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Channel::ams(ch), "Channel " + juce::String(ch) + " AMS", 
            0, 3, 0)); // 0-3 AMS sensitivity
            
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Channel::pms(ch), "Channel " + juce::String(ch) + " PMS", 
            0, 7, 0)); // 0-7 PMS sensitivity
    }
    
    // Operator parameters (4 operators)
    for (int op = 1; op <= 4; ++op)
    {
        juce::String opId = "op" + juce::String(op);
        
        // Total Level (TL) - 0-127, default values for safe volume
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::tl(op).c_str(), "OP" + juce::String(op) + " TL", 
            0, 127, op == 1 ? 80 : 127)); // OP1=80, others=127 (quiet)
        
        // Attack Rate (AR) - 0-31, default 31
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::ar(op).c_str(), "OP" + juce::String(op) + " AR", 
            0, 31, 31));
        
        // Decay Rate (D1R) - 0-31, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::d1r(op).c_str(), "OP" + juce::String(op) + " D1R", 
            0, 31, 0));
        
        // Sustain Rate (D2R) - 0-31, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::d2r(op).c_str(), "OP" + juce::String(op) + " D2R", 
            0, 31, 0));
        
        // Release Rate (RR) - 0-15, default 7
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::rr(op).c_str(), "OP" + juce::String(op) + " RR", 
            0, 15, 7));
        
        // Sustain Level (D1L) - 0-15, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::d1l(op).c_str(), "OP" + juce::String(op) + " D1L", 
            0, 15, 0));
        
        // Multiple (MUL) - 0-15, default 1
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::mul(op).c_str(), "OP" + juce::String(op) + " MUL", 
            0, 15, 1));
        
        // Detune 1 (DT1) - 0-7, default 3 (center)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::dt1(op).c_str(), "OP" + juce::String(op) + " DT1", 
            0, 7, 3));
        
        // Detune 2 (DT2) - 0-3, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::dt2(op).c_str(), "OP" + juce::String(op) + " DT2", 
            0, 3, 0));
        
        // Key Scale (KS) - 0-3, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::ks(op).c_str(), "OP" + juce::String(op) + " KS", 
            0, 3, 0));
        
        // AM Enable (AMS-EN) - 0-1, default 0
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            ParamID::Op::ams_en(op).c_str(), "OP" + juce::String(op) + " AMS-EN", false));
        
        // SLOT Enable - 0-1, default 1 (enabled)
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            ParamID::Op::slot_en(op).c_str(), "OP" + juce::String(op) + " SLOT", true));
    }
    
    return { params.begin(), params.end() };
}

void YMulatorSynthAudioProcessor::setupCCMapping()
{
    // VOPMex compatible MIDI CC mapping
    
    // Global parameters
    ccToParameterMap[14] = parameters.getParameter(ParamID::Global::Algorithm);
    ccToParameterMap[15] = parameters.getParameter(ParamID::Global::Feedback);
    
    // LFO parameters (CC 76-79)
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
    
    // Operator parameters (4 operators)
    for (int op = 1; op <= 4; ++op)
    {
        juce::String opId = "op" + juce::String(op);
        int opIndex = op - 1;
        
        // Total Level (CC 16-19)
        ccToParameterMap[16 + opIndex] = 
            parameters.getParameter(ParamID::Op::tl(op));
        
        // Multiple (CC 20-23)
        ccToParameterMap[20 + opIndex] = 
            parameters.getParameter(ParamID::Op::mul(op));
        
        // Detune1 (CC 24-27)
        ccToParameterMap[24 + opIndex] = 
            parameters.getParameter(ParamID::Op::dt1(op));
        
        // Detune2 (CC 28-31)
        ccToParameterMap[28 + opIndex] = 
            parameters.getParameter(ParamID::Op::dt2(op));
        
        // Key Scale (CC 39-42)
        ccToParameterMap[39 + opIndex] = 
            parameters.getParameter(ParamID::Op::ks(op));
        
        // Attack Rate (CC 43-46)
        ccToParameterMap[43 + opIndex] = 
            parameters.getParameter(ParamID::Op::ar(op));
        
        // Decay1 Rate (CC 47-50)
        ccToParameterMap[47 + opIndex] = 
            parameters.getParameter(ParamID::Op::d1r(op));
        
        // Sustain Rate (CC 51-54)
        ccToParameterMap[51 + opIndex] = 
            parameters.getParameter(ParamID::Op::d2r(op));
        
        // Release Rate (CC 55-58)
        ccToParameterMap[55 + opIndex] = 
            parameters.getParameter(ParamID::Op::rr(op));
        
        // Sustain Level (CC 59-62)
        ccToParameterMap[59 + opIndex] = 
            parameters.getParameter(ParamID::Op::d1l(op));
    }
    
    // Note: Channel pan parameters are handled separately in handleMidiCC() 
    // since they are AudioParameterFloat, not AudioParameterInt
}

void YMulatorSynthAudioProcessor::handleMidiCC(int ccNumber, int value)
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

void YMulatorSynthAudioProcessor::handlePitchBend(int pitchBendValue)
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

void YMulatorSynthAudioProcessor::setCurrentPreset(int index)
{
    // Assert valid preset index range
    CS_ASSERT_PARAMETER_RANGE(index, 0, presetManager.getNumPresets() - 1);
    
    if (index >= 0 && index < presetManager.getNumPresets())
    {
        currentPreset = index;
        isCustomPreset = false; // Reset custom state when loading factory preset
        
        if (ymfmWrapper.isInitialized()) {
            loadPreset(index);
            CS_DBG(" Loaded preset " + juce::String(index) + ": " + getProgramName(index));
        } else {
            needsPresetReapply = true;
            CS_DBG(" Preset " + juce::String(index) + " will be applied when ymfm is initialized");
        }
        
        // Update host display
        updateHostDisplay();
    }
}

void YMulatorSynthAudioProcessor::loadPreset(int index)
{
    // Assert valid preset index range
    CS_ASSERT_PARAMETER_RANGE(index, 0, presetManager.getNumPresets() - 1);
    
    auto preset = presetManager.getPreset(index);
    if (preset != nullptr)
    {
        loadPreset(preset);
    }
}

void YMulatorSynthAudioProcessor::loadPreset(const ymulatorsynth::Preset* preset)
{
    if (preset == nullptr) return;
    
    CS_DBG(" Loading preset '" + preset->name + "' - Algorithm: " + 
        juce::String(preset->algorithm) + ", Feedback: " + juce::String(preset->feedback));
    
    // Temporarily remove all listeners to prevent any custom state triggering
    parameters.state.removeListener(this);
    const auto& allParams1 = AudioProcessor::getParameters();
    for (auto* param : allParams1) {
        param->removeListener(this);
    }
    
    // Set global parameters with UI notification
    if (auto* algorithmParam = parameters.getParameter(ParamID::Global::Algorithm)) {
        algorithmParam->setValueNotifyingHost(algorithmParam->convertTo0to1(preset->algorithm));
    }
    if (auto* feedbackParam = parameters.getParameter(ParamID::Global::Feedback)) {
        feedbackParam->setValueNotifyingHost(feedbackParam->convertTo0to1(preset->feedback));
    }
    
    // Set LFO parameters with UI notification
    if (auto* lfoRateParam = parameters.getParameter(ParamID::Global::LfoRate)) {
        lfoRateParam->setValueNotifyingHost(lfoRateParam->convertTo0to1(preset->lfo.rate));
    }
    if (auto* lfoAmdParam = parameters.getParameter(ParamID::Global::LfoAmd)) {
        lfoAmdParam->setValueNotifyingHost(lfoAmdParam->convertTo0to1(preset->lfo.amd));
    }
    if (auto* lfoPmdParam = parameters.getParameter(ParamID::Global::LfoPmd)) {
        lfoPmdParam->setValueNotifyingHost(lfoPmdParam->convertTo0to1(preset->lfo.pmd));
    }
    if (auto* lfoWaveformParam = parameters.getParameter(ParamID::Global::LfoWaveform)) {
        lfoWaveformParam->setValueNotifyingHost(lfoWaveformParam->convertTo0to1(preset->lfo.waveform));
    }
    
    // Set noise parameters with UI notification
    if (auto* noiseEnableParam = parameters.getParameter(ParamID::Global::NoiseEnable)) {
        noiseEnableParam->setValueNotifyingHost(preset->channels[0].noiseEnable > 0 ? 1.0f : 0.0f);
    }
    if (auto* noiseFreqParam = parameters.getParameter(ParamID::Global::NoiseFrequency)) {
        noiseFreqParam->setValueNotifyingHost(noiseFreqParam->convertTo0to1(preset->lfo.noiseFreq));
    }
    
    // Set channel AMS/PMS parameters with UI notification
    for (int ch = 0; ch < 8; ++ch) {
        if (auto* amsParam = parameters.getParameter(ParamID::Channel::ams(ch))) {
            amsParam->setValueNotifyingHost(amsParam->convertTo0to1(preset->channels[ch].ams));
        }
        if (auto* pmsParam = parameters.getParameter(ParamID::Channel::pms(ch))) {
            pmsParam->setValueNotifyingHost(pmsParam->convertTo0to1(preset->channels[ch].pms));
        }
    }
    
    // Set operator parameters with UI notification
    for (int op = 0; op < 4; ++op)
    {
        juce::String opId = "op" + juce::String(op + 1);
        const auto& opData = preset->operators[op];
        
        if (auto* param = parameters.getParameter(ParamID::Op::ar(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.attackRate)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d1r(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.decay1Rate)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d2r(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.decay2Rate)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::rr(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.releaseRate)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d1l(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.sustainLevel)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::tl(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.totalLevel)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::ks(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.keyScale)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::mul(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.multiple)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::dt1(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.detune1)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::dt2(op + 1))) {
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.detune2)));
        }
        if (auto* param = parameters.getParameter(ParamID::Op::ams_en(op + 1))) {
            param->setValueNotifyingHost(preset->operators[op].amsEnable ? 1.0f : 0.0f);
        }
        if (auto* param = parameters.getParameter(ParamID::Op::slot_en(op + 1))) {
            // Use the actual SLOT enable state from the preset
            param->setValueNotifyingHost(preset->operators[op].slotEnable ? 1.0f : 0.0f);
        }
    }
    
    // Re-add all listeners immediately - all preset loading is done
    parameters.state.addListener(this);
    const auto& allParams2 = AudioProcessor::getParameters();
    for (auto* param : allParams2) {
        param->addListener(this);
    }
    
    // Debug: print one operator's parameters for verification
    CS_DBG(" OP1 loaded - TL: " + juce::String(preset->operators[0].totalLevel) + 
        ", AR: " + juce::String(preset->operators[0].attackRate) +
        ", MUL: " + juce::String(preset->operators[0].multiple));
    
    // Use optimized batch update for preset loading (more efficient than updateYmfmParameters)
    if (ymfmWrapper.isInitialized()) {
        // Prepare batch parameter data for all channels
        for (int channel = 0; channel < 8; ++channel) {
            std::array<std::array<uint8_t, 10>, 4> operatorParams;
            
            for (int op = 0; op < 4; ++op) {
                operatorParams[op][0] = static_cast<uint8_t>(preset->operators[op].totalLevel);
                operatorParams[op][1] = static_cast<uint8_t>(preset->operators[op].attackRate);
                operatorParams[op][2] = static_cast<uint8_t>(preset->operators[op].decay1Rate);
                operatorParams[op][3] = static_cast<uint8_t>(preset->operators[op].decay2Rate);
                operatorParams[op][4] = static_cast<uint8_t>(preset->operators[op].releaseRate);
                operatorParams[op][5] = static_cast<uint8_t>(preset->operators[op].sustainLevel);
                operatorParams[op][6] = static_cast<uint8_t>(preset->operators[op].keyScale);
                operatorParams[op][7] = static_cast<uint8_t>(preset->operators[op].multiple);
                operatorParams[op][8] = static_cast<uint8_t>(preset->operators[op].detune1);
                operatorParams[op][9] = static_cast<uint8_t>(preset->operators[op].detune2);
            }
            
            // Batch update entire channel at once
            ymfmWrapper.batchUpdateChannelParameters(channel, preset->algorithm, preset->feedback, operatorParams);
        }
        
        // Update LFO and other global settings
        updateYmfmParameters();
        
        CS_DBG(" Optimized batch preset loading complete");
    } else {
        // Fallback to standard update if ymfm not initialized
        updateYmfmParameters();
        CS_DBG(" Standard preset loading complete (ymfm not initialized)");
    }
}

void YMulatorSynthAudioProcessor::parameterValueChanged(int parameterIndex, float newValue)
{
    juce::ignoreUnused(parameterIndex, newValue);
    
    // Only switch to custom if not already in custom mode and gesture is in progress
    if (!isCustomPreset && userGestureInProgress) {
        CS_DBG(" Parameter changed by user gesture, switching to custom preset");
        isCustomPreset = true;
        
        // Notify UI components of custom mode change
        parameters.state.setProperty("isCustomMode", true, nullptr);
        
        // Update host display on message thread
        juce::MessageManager::callAsync([this]() {
            updateHostDisplay();
        });
    }
}

void YMulatorSynthAudioProcessor::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    juce::ignoreUnused(parameterIndex);
    
    userGestureInProgress = gestureIsStarting;
    CS_DBG(" User gesture " + juce::String(gestureIsStarting ? "started" : "ended"));
}

void YMulatorSynthAudioProcessor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                                      const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged, property);
    
    // ValueTree changes no longer trigger custom state
    // Custom state is only triggered by user gestures via parameterValueChanged()
    CS_DBG(" ValueTree property changed: " + property.toString() + " (no custom state change)");
}

void YMulatorSynthAudioProcessor::updateYmfmParameters()
{
    // Check if ymfm is initialized
    if (!ymfmWrapper.isInitialized()) {
        CS_DBG(" updateYmfmParameters called before ymfm initialization");
        return;
    }
    
    // Get current parameter values
    int algorithm = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::Algorithm));
    int feedback = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::Feedback));
    
    static int updateCounter = 0;
    if (updateCounter++ % 100 == 0) {
        CS_DBG(" updateYmfmParameters - Algorithm: " + juce::String(algorithm) + 
            ", Feedback: " + juce::String(feedback));
    }
    
    // Update parameters for all 8 channels to keep them in sync
    for (int channel = 0; channel < 8; ++channel)
    {
        // Assert valid channel index
        CS_ASSERT_CHANNEL(channel);
        
        // Update global parameters for each channel
        ymfmWrapper.setAlgorithm(channel, algorithm);
        ymfmWrapper.setFeedback(channel, feedback);
        
        // Update operator parameters
        for (int op = 0; op < 4; ++op)
        {
            // Assert valid operator index
            CS_ASSERT_OPERATOR(op);
            
            juce::String opId = "op" + juce::String(op + 1);
            
            int tl = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::tl(op + 1).c_str()));
            int ar = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::ar(op + 1).c_str()));
            int d1r = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::d1r(op + 1).c_str()));
            int d2r = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::d2r(op + 1).c_str()));
            int rr = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::rr(op + 1).c_str()));
            int d1l = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::d1l(op + 1).c_str()));
            int ks = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::ks(op + 1).c_str()));
            int mul = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::mul(op + 1).c_str()));
            int dt1 = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::dt1(op + 1).c_str()));
            int dt2 = static_cast<int>(*parameters.getRawParameterValue(ParamID::Op::dt2(op + 1).c_str()));
            
            // Use optimized envelope setting for envelope parameters only
            ymfmWrapper.setOperatorEnvelope(channel, op, ar, d1r, d2r, rr, d1l);
            
            // Set TL considering SLOT enable/disable state
            bool slotEnabled = *parameters.getRawParameterValue(ParamID::Op::slot_en(op + 1).c_str()) >= 0.5f;
            int effectiveTL = slotEnabled ? tl : 127; // Mute if slot disabled
            ymfmWrapper.setOperatorParameter(channel, op, YmfmWrapper::OperatorParameter::TotalLevel, effectiveTL);
            ymfmWrapper.setOperatorParameter(channel, op, YmfmWrapper::OperatorParameter::KeyScale, ks);
            ymfmWrapper.setOperatorParameter(channel, op, YmfmWrapper::OperatorParameter::Multiple, mul);
            ymfmWrapper.setOperatorParameter(channel, op, YmfmWrapper::OperatorParameter::Detune1, dt1);
            ymfmWrapper.setOperatorParameter(channel, op, YmfmWrapper::OperatorParameter::Detune2, dt2);
        }
        
        // Update channel pan
        float pan = *parameters.getRawParameterValue(ParamID::Channel::pan(channel).c_str());
        CS_ASSERT_PAN_RANGE(pan);
        ymfmWrapper.setChannelPan(channel, pan);
        
        // Update channel AMS/PMS settings
        int ams = static_cast<int>(*parameters.getRawParameterValue(ParamID::Channel::ams(channel).c_str()));
        int pms = static_cast<int>(*parameters.getRawParameterValue(ParamID::Channel::pms(channel).c_str()));
        CS_ASSERT_PARAMETER_RANGE(ams, 0, 3);
        CS_ASSERT_PARAMETER_RANGE(pms, 0, 7);
        ymfmWrapper.setChannelAmsPms(channel, static_cast<uint8_t>(ams), static_cast<uint8_t>(pms));
        
    }
    
    // Update global LFO settings
    int lfoRate = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::LfoRate));
    int lfoAmd = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::LfoAmd));
    int lfoPmd = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::LfoPmd));
    int lfoWaveform = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::LfoWaveform));
    
    CS_ASSERT_PARAMETER_RANGE(lfoRate, 0, 255);
    CS_ASSERT_PARAMETER_RANGE(lfoAmd, 0, 127);
    CS_ASSERT_PARAMETER_RANGE(lfoPmd, 0, 127);
    CS_ASSERT_PARAMETER_RANGE(lfoWaveform, 0, 3);
    
    ymfmWrapper.setLfoParameters(static_cast<uint8_t>(lfoRate), static_cast<uint8_t>(lfoAmd), 
                                  static_cast<uint8_t>(lfoPmd), static_cast<uint8_t>(lfoWaveform));
    
    // Update noise parameters
    bool noiseEnable = *parameters.getRawParameterValue(ParamID::Global::NoiseEnable) >= 0.5f;
    int noiseFrequency = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::NoiseFrequency));
    
    CS_ASSERT_PARAMETER_RANGE(noiseFrequency, 0, 31);
    
    ymfmWrapper.setNoiseParameters(noiseEnable, static_cast<uint8_t>(noiseFrequency));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YMulatorSynthAudioProcessor();
}