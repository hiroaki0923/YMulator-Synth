#include "PluginProcessor.h"
#include "PluginEditor.h"

ChipSynthAudioProcessor::ChipSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("ChipSynth"), createParameterLayout())
{
    setupCCMapping();
    
    // Load default preset (Init)
    loadFactoryPreset(7); // Init preset
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
    return NUM_FACTORY_PRESETS;
}

int ChipSynthAudioProcessor::getCurrentProgram()
{
    return currentPreset;
}

void ChipSynthAudioProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < NUM_FACTORY_PRESETS)
    {
        currentPreset = index;
        loadFactoryPreset(index);
        DBG("ChipSynth: Loaded factory preset " + juce::String(index) + ": " + getProgramName(index));
    }
}

const juce::String ChipSynthAudioProcessor::getProgramName(int index)
{
    const juce::String presetNames[NUM_FACTORY_PRESETS] = {
        "Electric Piano",
        "Synth Bass", 
        "Brass Section",
        "String Pad",
        "Lead Synth",
        "Organ",
        "Bells",
        "Init"
    };
    
    if (index >= 0 && index < NUM_FACTORY_PRESETS)
        return presetNames[index];
    
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
    DBG("ChipSynth: prepareToPlay called - sampleRate: " + juce::String(sampleRate) + ", samplesPerBlock: " + juce::String(samplesPerBlock));
    
    // Initialize ymfm wrapper with OPM for now
    ymfmWrapper.initialize(YmfmWrapper::ChipType::OPM, static_cast<uint32_t>(sampleRate));
    
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
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml(*xmlState);
            parameters.replaceState(newState);
            
            // Restore preset number
            currentPreset = newState.getProperty("currentPreset", 0);
            
            DBG("ChipSynth: State loaded - preset: " + juce::String(currentPreset));
        }
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


void ChipSynthAudioProcessor::loadFactoryPreset(int index)
{
    // Factory preset data based on VOPM format specification
    struct PresetData {
        int algorithm, feedback;
        struct { int ar, d1r, d2r, rr, d1l, tl, ks, mul, dt1, dt2; } op[4];
    };
    
    const PresetData factoryPresets[NUM_FACTORY_PRESETS] = {
        // Electric Piano - Algorithm 5 with feedback 7 for classic FM electric piano sound
        // Uses DT2 variations to create natural chorusing and warmth
        { 5, 7, {
            {31, 14, 0, 7, 0, 32, 1, 1, 3, 1},  // OP1: carrier with slight KS and DT2 for warmth
            {31, 5, 0, 7, 0, 45, 0, 1, 4, 2},   // OP2: modulator with DT1=4, DT2=2 for rich harmonics
            {31, 7, 0, 7, 0, 50, 1, 1, 2, 0},   // OP3: modulator with DT1=2, KS=1 for brightness
            {31, 9, 0, 7, 0, 55, 0, 1, 3, 1}    // OP4: modulator with DT2=1 for complexity
        }},
        
        // Synth Bass - Enhanced with aggressive feedback and KS
        { 7, 6, {
            {31, 8, 0, 7, 0, 25, 3, 1, 3, 0},   // OP1: KS=3 for punch
            {31, 8, 0, 7, 0, 60, 2, 1, 3, 0},   // OP2: KS=2
            {31, 8, 0, 7, 0, 65, 1, 1, 3, 0},   // OP3: KS=1
            {31, 8, 0, 7, 0, 70, 1, 1, 3, 0}    // OP4: KS=1
        }},
        
        // Brass Section - Enhanced with varied DT2 for ensemble effect
        { 4, 6, {
            {31, 14, 6, 7, 1, 35, 2, 1, 3, 1},  // OP1: KS=2, DT2=1 for brightness
            {31, 14, 3, 7, 1, 40, 2, 1, 2, 2},  // OP2: DT1=2, DT2=2 for spread
            {31, 11, 11, 7, 1, 45, 1, 1, 4, 1}, // OP3: DT1=4, DT2=1 for character
            {31, 14, 6, 7, 1, 50, 2, 1, 3, 0}   // OP4: KS=2 for definition
        }},
        
        // String Pad - Enhanced with subtle DT2 variations
        { 1, 2, {  // Increased feedback for warmth
            {15, 7, 7, 1, 1, 25, 0, 1, 3, 1},   // OP1: DT2=1 for slight spread
            {15, 4, 4, 1, 1, 30, 0, 1, 2, 2},   // OP2: DT1=2, DT2=2 for richness
            {15, 7, 7, 1, 1, 35, 0, 1, 4, 1},   // OP3: DT1=4, DT2=1 for depth
            {15, 4, 4, 1, 1, 40, 0, 1, 3, 0}    // OP4: standard tuning
        }},
        
        // Lead Synth - Enhanced with sharp attack and DT2 character
        { 7, 5, {  // Increased feedback for edge
            {31, 6, 2, 7, 0, 30, 3, 1, 3, 0},   // OP1: KS=3 for attack sharpness
            {31, 6, 2, 7, 0, 60, 2, 1, 2, 1},   // OP2: DT1=2, DT2=1 for character
            {31, 6, 2, 7, 0, 65, 2, 1, 4, 2},   // OP3: DT1=4, DT2=2 for spread
            {31, 6, 2, 7, 0, 70, 2, 1, 3, 0}    // OP4: KS=2
        }},
        
        // Organ - Enhanced with harmonics and key scaling
        { 7, 3, {  // Added feedback for organ character
            {31, 0, 0, 7, 0, 40, 1, 2, 3, 0},   // OP1: MUL=2, KS=1 for fundamental
            {31, 0, 0, 7, 0, 65, 1, 3, 3, 1},   // OP2: MUL=3, DT2=1 for 3rd harmonic
            {31, 0, 0, 7, 0, 70, 0, 4, 3, 0},   // OP3: MUL=4 for 4th harmonic
            {31, 0, 0, 7, 0, 75, 1, 5, 3, 1}    // OP4: MUL=5, DT2=1 for 5th harmonic
        }},
        
        // Bells - Enhanced with complex DT2 relationships
        { 1, 2, {  // Slight feedback for warmth
            {31, 18, 0, 4, 3, 25, 2, 14, 3, 1}, // OP1: KS=2, DT2=1 for bell character
            {31, 18, 0, 4, 3, 30, 1, 1, 2, 3},  // OP2: DT1=2, DT2=3 for inharmonicity
            {31, 18, 0, 4, 3, 35, 1, 1, 4, 2},  // OP3: DT1=4, DT2=2 for complexity
            {31, 18, 0, 4, 3, 40, 0, 1, 5, 1}   // OP4: DT1=5, DT2=1 for shimmer
        }},
        
        // Init (basic sine wave) - Simple but with slight enhancements
        { 7, 0, {
            {31, 0, 0, 7, 0, 32, 0, 1, 3, 0},   // OP1: clean sine
            {31, 0, 0, 7, 0, 127, 0, 1, 3, 0},  // OP2-4: silent
            {31, 0, 0, 7, 0, 127, 0, 1, 3, 0},
            {31, 0, 0, 7, 0, 127, 0, 1, 3, 0}
        }}
    };
    
    if (index < 0 || index >= NUM_FACTORY_PRESETS) return;
    
    const auto& preset = factoryPresets[index];
    
    // Set global parameters using parameter methods that notify UI
    if (auto* algorithmParam = parameters.getParameter("algorithm"))
        algorithmParam->setValueNotifyingHost(algorithmParam->convertTo0to1(preset.algorithm));
    if (auto* feedbackParam = parameters.getParameter("feedback"))
        feedbackParam->setValueNotifyingHost(feedbackParam->convertTo0to1(preset.feedback));
    
    // Set operator parameters using parameter methods that notify UI
    for (int op = 0; op < 4; ++op)
    {
        juce::String opId = "op" + juce::String(op + 1);
        const auto& opData = preset.op[op];
        
        if (auto* param = parameters.getParameter(opId + "_ar"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.ar));
        if (auto* param = parameters.getParameter(opId + "_d1r"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.d1r));
        if (auto* param = parameters.getParameter(opId + "_d2r"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.d2r));
        if (auto* param = parameters.getParameter(opId + "_rr"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.rr));
        if (auto* param = parameters.getParameter(opId + "_d1l"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.d1l));
        if (auto* param = parameters.getParameter(opId + "_tl"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.tl));
        if (auto* param = parameters.getParameter(opId + "_ks"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.ks));
        if (auto* param = parameters.getParameter(opId + "_mul"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.mul));
        if (auto* param = parameters.getParameter(opId + "_dt1"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.dt1));
        if (auto* param = parameters.getParameter(opId + "_dt2"))
            param->setValueNotifyingHost(param->convertTo0to1(opData.dt2));
        if (auto* param = parameters.getParameter(opId + "_ams_en"))
            param->setValueNotifyingHost(0.0f);
    }
    
    // Force parameter update to ymfm
    updateYmfmParameters();
}

void ChipSynthAudioProcessor::updateYmfmParameters()
{
    // Get current parameter values
    int algorithm = static_cast<int>(*parameters.getRawParameterValue("algorithm"));
    int feedback = static_cast<int>(*parameters.getRawParameterValue("feedback"));
    
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