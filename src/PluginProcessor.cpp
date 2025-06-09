#include "PluginProcessor.h"
#include "PluginEditor.h"

ChipSynthAudioProcessor::ChipSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("ChipSynth"), createParameterLayout())
{
    DBG("ChipSynth: Constructor called");
    
    setupCCMapping();
    
    // Initialize preset manager
    presetManager.initialize();
    
    // Load default preset (Init)
    setCurrentPreset(7); // Init preset
    
    DBG("ChipSynth: Constructor completed - default preset: " + juce::String(currentPreset));
}

ChipSynthAudioProcessor::~ChipSynthAudioProcessor()
{
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
    return presetManager.getNumPresets();
}

int ChipSynthAudioProcessor::getCurrentProgram()
{
    return currentPreset;
}

void ChipSynthAudioProcessor::setCurrentProgram(int index)
{
    DBG("ChipSynth: setCurrentProgram called with index: " + juce::String(index));
    setCurrentPreset(index);
}

const juce::String ChipSynthAudioProcessor::getProgramName(int index)
{
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
    DBG("ChipSynth: prepareToPlay called - sampleRate: " + juce::String(sampleRate) + 
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
        DBG("ChipSynth: Applied deferred preset " + juce::String(currentPreset));
    }
    
    DBG("ChipSynth: ymfm initialization complete");
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
        DBG("ChipSynth: processBlock FIRST CALL - channels: " + juce::String(buffer.getNumChannels()) + 
            ", samples: " + juce::String(buffer.getNumSamples()));
        hasLoggedFirstCall = true;
    }
    
    // Clear output buffer
    buffer.clear();
    
    // Debug MIDI events
    if (!midiMessages.isEmpty()) {
        DBG("ChipSynth: Received " + juce::String(midiMessages.getNumEvents()) + " MIDI events");
    }
    
    // Process MIDI events
    for (const auto metadata : midiMessages) {
        const auto message = metadata.getMessage();
        
        if (message.isNoteOn()) {
            DBG("ChipSynth: Note ON - Note: " + juce::String(message.getNoteNumber()) + 
                ", Velocity: " + juce::String(message.getVelocity()));
            
            // Allocate a voice for this note
            int channel = voiceManager.allocateVoice(message.getNoteNumber(), message.getVelocity());
            
            // Tell ymfm to play this note on the allocated channel
            ymfmWrapper.noteOn(channel, message.getNoteNumber(), message.getVelocity());
            
        } else if (message.isNoteOff()) {
            DBG("ChipSynth: Note OFF - Note: " + juce::String(message.getNoteNumber()));
            
            // Find which channel is playing this note
            int channel = voiceManager.getChannelForNote(message.getNoteNumber());
            if (channel >= 0) {
                // Tell ymfm to stop this note
                ymfmWrapper.noteOff(channel, message.getNoteNumber());
                
                // Release the voice
                voiceManager.releaseVoice(message.getNoteNumber());
            }
        } else if (message.isController()) {
            DBG("ChipSynth: MIDI CC - CC: " + juce::String(message.getControllerNumber()) + 
                ", Value: " + juce::String(message.getControllerValue()));
            handleMidiCC(message.getControllerNumber(), message.getControllerValue());
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
            DBG("ChipSynth: processBlock audio generation - call #" + juce::String(audioCallCounter) + 
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
            DBG("ChipSynth: Audio check - " + juce::String(hasAudio ? "HAS AUDIO" : "SILENT"));
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
    
    // Add current preset number to state
    state.setProperty("currentPreset", currentPreset, nullptr);
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
    
    DBG("ChipSynth: State saved - preset: " + juce::String(currentPreset));
}

void ChipSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    DBG("ChipSynth: setStateInformation called - size: " + juce::String(sizeInBytes));
    
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
    {
        DBG("ChipSynth: XML state parsed successfully");
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(newState);
            
            // Restore preset number
            currentPreset = newState.getProperty("currentPreset", 0);
            
            DBG("ChipSynth: State loaded - preset: " + juce::String(currentPreset));
            
            // If ymfm is already initialized, apply the preset
            if (ymfmWrapper.isInitialized()) {
                DBG("ChipSynth: Applying preset after state restore");
                setCurrentPreset(currentPreset);
            } else {
                DBG("ChipSynth: Deferring preset application until ymfm init");
                needsPresetReapply = true;
            }
        } else {
            DBG("ChipSynth: XML state tag mismatch");
        }
    } else {
        DBG("ChipSynth: Failed to parse XML state");
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ChipSynthAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    
    // Global parameters
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "algorithm", "Algorithm", 0, 7, 0));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        "feedback", "Feedback", 0, 7, 0));
    
    // Operator parameters (4 operators)
    for (int op = 1; op <= 4; ++op)
    {
        juce::String opId = "op" + juce::String(op);
        
        // Total Level (TL) - 0-127, default values for safe volume
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_tl", "OP" + juce::String(op) + " TL", 
            0, 127, op == 1 ? 80 : 127)); // OP1=80, others=127 (quiet)
        
        // Attack Rate (AR) - 0-31, default 31
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_ar", "OP" + juce::String(op) + " AR", 
            0, 31, 31));
        
        // Decay Rate (D1R) - 0-31, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_d1r", "OP" + juce::String(op) + " D1R", 
            0, 31, 0));
        
        // Sustain Rate (D2R) - 0-31, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_d2r", "OP" + juce::String(op) + " D2R", 
            0, 31, 0));
        
        // Release Rate (RR) - 0-15, default 7
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_rr", "OP" + juce::String(op) + " RR", 
            0, 15, 7));
        
        // Sustain Level (D1L) - 0-15, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_d1l", "OP" + juce::String(op) + " D1L", 
            0, 15, 0));
        
        // Multiple (MUL) - 0-15, default 1
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_mul", "OP" + juce::String(op) + " MUL", 
            0, 15, 1));
        
        // Detune 1 (DT1) - 0-7, default 3 (center)
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_dt1", "OP" + juce::String(op) + " DT1", 
            0, 7, 3));
        
        // Detune 2 (DT2) - 0-3, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_dt2", "OP" + juce::String(op) + " DT2", 
            0, 3, 0));
        
        // Key Scale (KS) - 0-3, default 0
        params.push_back(std::make_unique<juce::AudioParameterInt>(
            opId + "_ks", "OP" + juce::String(op) + " KS", 
            0, 3, 0));
        
        // AM Enable (AMS-EN) - 0-1, default 0
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            opId + "_ams_en", "OP" + juce::String(op) + " AMS-EN", false));
    }
    
    return { params.begin(), params.end() };
}

void ChipSynthAudioProcessor::setupCCMapping()
{
    // VOPMex compatible MIDI CC mapping
    
    // Global parameters
    ccToParameterMap[14] = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("algorithm"));
    ccToParameterMap[15] = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("feedback"));
    
    // Operator parameters (4 operators)
    for (int op = 1; op <= 4; ++op)
    {
        juce::String opId = "op" + juce::String(op);
        int opIndex = op - 1;
        
        // Total Level (CC 16-19)
        ccToParameterMap[16 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_tl"));
        
        // Multiple (CC 20-23)
        ccToParameterMap[20 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_mul"));
        
        // Detune1 (CC 24-27)
        ccToParameterMap[24 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_dt1"));
        
        // Detune2 (CC 28-31)
        ccToParameterMap[28 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_dt2"));
        
        // Key Scale (CC 39-42)
        ccToParameterMap[39 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_ks"));
        
        // Attack Rate (CC 43-46)
        ccToParameterMap[43 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_ar"));
        
        // Decay1 Rate (CC 47-50)
        ccToParameterMap[47 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_d1r"));
        
        // Sustain Rate (CC 51-54)
        ccToParameterMap[51 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_d2r"));
        
        // Release Rate (CC 55-58)
        ccToParameterMap[55 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_rr"));
        
        // Sustain Level (CC 59-62)
        ccToParameterMap[59 + opIndex] = 
            dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter(opId + "_d1l"));
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
        
        DBG("ChipSynth: MIDI CC " + juce::String(ccNumber) + " = " + juce::String(value) + 
            " -> " + it->second->name + " = " + juce::String(it->second->get()));
    }
}


void ChipSynthAudioProcessor::setCurrentPreset(int index)
{
    if (index >= 0 && index < presetManager.getNumPresets())
    {
        currentPreset = index;
        if (ymfmWrapper.isInitialized()) {
            loadPreset(index);
            DBG("ChipSynth: Loaded preset " + juce::String(index) + ": " + getProgramName(index));
        } else {
            needsPresetReapply = true;
            DBG("ChipSynth: Preset " + juce::String(index) + " will be applied when ymfm is initialized");
        }
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
    
    DBG("ChipSynth: Loading preset '" + preset->name + "' - Algorithm: " + 
        juce::String(preset->algorithm) + ", Feedback: " + juce::String(preset->feedback));
    
    // Set global parameters using parameter methods that notify UI
    if (auto* algorithmParam = parameters.getParameter("algorithm"))
        algorithmParam->setValueNotifyingHost(algorithmParam->convertTo0to1(preset->algorithm));
    if (auto* feedbackParam = parameters.getParameter("feedback"))
        feedbackParam->setValueNotifyingHost(feedbackParam->convertTo0to1(preset->feedback));
    
    // Set operator parameters using parameter methods that notify UI
    for (int op = 0; op < 4; ++op)
    {
        juce::String opId = "op" + juce::String(op + 1);
        const auto& opData = preset->operators[op];
        
        if (auto* param = parameters.getParameter(opId + "_ar"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.attackRate)));
        if (auto* param = parameters.getParameter(opId + "_d1r"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.decay1Rate)));
        if (auto* param = parameters.getParameter(opId + "_d2r"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.decay2Rate)));
        if (auto* param = parameters.getParameter(opId + "_rr"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.releaseRate)));
        if (auto* param = parameters.getParameter(opId + "_d1l"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.sustainLevel)));
        if (auto* param = parameters.getParameter(opId + "_tl"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.totalLevel)));
        if (auto* param = parameters.getParameter(opId + "_ks"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.keyScale)));
        if (auto* param = parameters.getParameter(opId + "_mul"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.multiple)));
        if (auto* param = parameters.getParameter(opId + "_dt1"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.detune1)));
        if (auto* param = parameters.getParameter(opId + "_dt2"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<int>(opData.detune2)));
        if (auto* param = parameters.getParameter(opId + "_ams_en"))
            param->setValueNotifyingHost(0.0f);
    }
    
    // Debug: print one operator's parameters for verification
    DBG("ChipSynth: OP1 loaded - TL: " + juce::String(preset->operators[0].totalLevel) + 
        ", AR: " + juce::String(preset->operators[0].attackRate) +
        ", MUL: " + juce::String(preset->operators[0].multiple));
    
    // Force parameter update to ymfm
    updateYmfmParameters();
}

void ChipSynthAudioProcessor::updateYmfmParameters()
{
    // Check if ymfm is initialized
    if (!ymfmWrapper.isInitialized()) {
        DBG("ChipSynth: updateYmfmParameters called before ymfm initialization");
        return;
    }
    
    // Get current parameter values
    int algorithm = static_cast<int>(*parameters.getRawParameterValue("algorithm"));
    int feedback = static_cast<int>(*parameters.getRawParameterValue("feedback"));
    
    static int updateCounter = 0;
    if (updateCounter++ % 100 == 0) {
        DBG("ChipSynth: updateYmfmParameters - Algorithm: " + juce::String(algorithm) + 
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
            
            int tl = static_cast<int>(*parameters.getRawParameterValue(opId + "_tl"));
            int ar = static_cast<int>(*parameters.getRawParameterValue(opId + "_ar"));
            int d1r = static_cast<int>(*parameters.getRawParameterValue(opId + "_d1r"));
            int d2r = static_cast<int>(*parameters.getRawParameterValue(opId + "_d2r"));
            int rr = static_cast<int>(*parameters.getRawParameterValue(opId + "_rr"));
            int d1l = static_cast<int>(*parameters.getRawParameterValue(opId + "_d1l"));
            int ks = static_cast<int>(*parameters.getRawParameterValue(opId + "_ks"));
            int mul = static_cast<int>(*parameters.getRawParameterValue(opId + "_mul"));
            int dt1 = static_cast<int>(*parameters.getRawParameterValue(opId + "_dt1"));
            int dt2 = static_cast<int>(*parameters.getRawParameterValue(opId + "_dt2"));
            
            ymfmWrapper.setOperatorParameters(channel, op, tl, ar, d1r, d2r, rr, d1l, ks, mul, dt1, dt2);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChipSynthAudioProcessor();
}