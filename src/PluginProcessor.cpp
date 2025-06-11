#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/Debug.h"
#include "utils/ParameterIDs.h"

ChipSynthAudioProcessor::ChipSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("ChipSynth"), createParameterLayout())
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

ChipSynthAudioProcessor::~ChipSynthAudioProcessor()
{
    // Remove ValueTree listener
    parameters.state.removeListener(this);
    
    // Remove parameter listeners
    const auto& allParams = AudioProcessor::getParameters();
    for (auto* param : allParams) {
        param->removeListener(this);
    }
}

const juce::String ChipSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ChipSynthAudioProcessor::acceptsMidi() const
{
    return true;
}

bool ChipSynthAudioProcessor::producesMidi() const
{
    return false;
}

bool ChipSynthAudioProcessor::isMidiEffect() const
{
    return false;
}

double ChipSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ChipSynthAudioProcessor::getNumPrograms()
{
    // Add 1 for custom preset if active
    return presetManager.getNumPresets() + (isCustomPreset ? 1 : 0);
}

int ChipSynthAudioProcessor::getCurrentProgram()
{
    if (isCustomPreset) {
        return presetManager.getNumPresets(); // Custom preset index
    }
    return currentPreset;
}

void ChipSynthAudioProcessor::setCurrentProgram(int index)
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

const juce::String ChipSynthAudioProcessor::getProgramName(int index)
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

void ChipSynthAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void ChipSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
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

void ChipSynthAudioProcessor::releaseResources()
{
}

bool ChipSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void ChipSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midiMessages)
{
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
            CS_DBG(" Note ON - Note: " + juce::String(message.getNoteNumber()) + 
                ", Velocity: " + juce::String(message.getVelocity()));
            
            // Allocate a voice for this note
            int channel = voiceManager.allocateVoice(message.getNoteNumber(), message.getVelocity());
            
            // Tell ymfm to play this note on the allocated channel
            ymfmWrapper.noteOn(channel, message.getNoteNumber(), message.getVelocity());
            
        } else if (message.isNoteOff()) {
            CS_DBG(" Note OFF - Note: " + juce::String(message.getNoteNumber()));
            
            // Find which channel is playing this note
            int channel = voiceManager.getChannelForNote(message.getNoteNumber());
            if (channel >= 0) {
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
        
        ymfmWrapper.generateSamples(buffer.getWritePointer(0), numSamples);
        
        // Apply moderate gain to prevent clipping
        buffer.applyGain(0, 0, numSamples, 2.0f);
        
        // Copy left channel to right channel for stereo
        if (buffer.getNumChannels() > 1) {
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
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

bool ChipSynthAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ChipSynthAudioProcessor::createEditor()
{
    return new ChipSynthAudioProcessorEditor(*this);
}

void ChipSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
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

void ChipSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
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

juce::AudioProcessorValueTreeState::ParameterLayout ChipSynthAudioProcessor::createParameterLayout()
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
    }
    
    return { params.begin(), params.end() };
}

void ChipSynthAudioProcessor::setupCCMapping()
{
    // VOPMex compatible MIDI CC mapping
    
    // Global parameters
    ccToParameterMap[14] = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Global::Algorithm));
    ccToParameterMap[15] = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Global::Feedback));
    
    // Operator parameters (4 operators)
    for (int op = 1; op <= 4; ++op)
    {
        juce::String opId = "op" + juce::String(op);
        int opIndex = op - 1;
        
        // Total Level (CC 16-19)
        ccToParameterMap[16 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::tl(op)));
        
        // Multiple (CC 20-23)
        ccToParameterMap[20 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::mul(op)));
        
        // Detune1 (CC 24-27)
        ccToParameterMap[24 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::dt1(op)));
        
        // Detune2 (CC 28-31)
        ccToParameterMap[28 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::dt2(op)));
        
        // Key Scale (CC 39-42)
        ccToParameterMap[39 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::ks(op)));
        
        // Attack Rate (CC 43-46)
        ccToParameterMap[43 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::ar(op)));
        
        // Decay1 Rate (CC 47-50)
        ccToParameterMap[47 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::d1r(op)));
        
        // Sustain Rate (CC 51-54)
        ccToParameterMap[51 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::d2r(op)));
        
        // Release Rate (CC 55-58)
        ccToParameterMap[55 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::rr(op)));
        
        // Sustain Level (CC 59-62)
        ccToParameterMap[59 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(ParamID::Op::d1l(op)));
    }
}

void ChipSynthAudioProcessor::handleMidiCC(int ccNumber, int value)
{
    auto it = ccToParameterMap.find(ccNumber);
    if (it != ccToParameterMap.end() && it->second != nullptr)
    {
        // Normalize CC value (0-127) to parameter range (0.0-1.0)
        float normalizedValue = juce::jlimit(0.0f, 1.0f, value / 127.0f);
        
        // Update parameter (thread-safe)
        it->second->setValueNotifyingHost(normalizedValue);
        
        CS_DBG(" MIDI CC " + juce::String(ccNumber) + " = " + juce::String(value) + 
            " -> " + it->second->name + " = " + juce::String(it->second->get()));
    }
}

void ChipSynthAudioProcessor::handlePitchBend(int pitchBendValue)
{
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

void ChipSynthAudioProcessor::setCurrentPreset(int index)
{
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

void ChipSynthAudioProcessor::loadPreset(int index)
{
    auto preset = presetManager.getPreset(index);
    if (preset != nullptr)
    {
        loadPreset(preset);
    }
}

void ChipSynthAudioProcessor::loadPreset(const chipsynth::Preset* preset)
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
            param->setValueNotifyingHost(0.0f);
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
    
    // Force parameter update to ymfm
    updateYmfmParameters();
    
    CS_DBG(" Preset loading complete");
}

void ChipSynthAudioProcessor::parameterValueChanged(int parameterIndex, float newValue)
{
    juce::ignoreUnused(parameterIndex, newValue);
    
    // Only switch to custom if not already in custom mode and gesture is in progress
    if (!isCustomPreset && userGestureInProgress) {
        CS_DBG(" Parameter changed by user gesture, switching to custom preset");
        isCustomPreset = true;
        
        // Update host display on message thread
        juce::MessageManager::callAsync([this]() {
            updateHostDisplay();
        });
    }
}

void ChipSynthAudioProcessor::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    juce::ignoreUnused(parameterIndex);
    
    userGestureInProgress = gestureIsStarting;
    CS_DBG(" User gesture " + juce::String(gestureIsStarting ? "started" : "ended"));
}

void ChipSynthAudioProcessor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                                      const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged, property);
    
    // ValueTree changes no longer trigger custom state
    // Custom state is only triggered by user gestures via parameterValueChanged()
    CS_DBG(" ValueTree property changed: " + property.toString() + " (no custom state change)");
}

void ChipSynthAudioProcessor::updateYmfmParameters()
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
        // Update global parameters for each channel
        ymfmWrapper.setAlgorithm(channel, algorithm);
        ymfmWrapper.setFeedback(channel, feedback);
        
        // Update operator parameters
        for (int op = 0; op < 4; ++op)
        {
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
            
            ymfmWrapper.setOperatorParameters(channel, op, tl, ar, d1r, d2r, rr, d1l, ks, mul, dt1, dt2);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipSynthAudioProcessor();
}