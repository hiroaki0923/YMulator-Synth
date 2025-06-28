#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/Debug.h"
#include "utils/ParameterIDs.h"
#include "dsp/YM2151Registers.h"

YMulatorSynthAudioProcessor::YMulatorSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("YMulatorSynth"), createParameterLayout()),
       ymfmWrapper(std::make_unique<YmfmWrapper>()),
       voiceManager(std::make_unique<VoiceManager>()),
       midiProcessor(nullptr), // Will be initialized after other components
       presetManager(std::make_unique<ymulatorsynth::PresetManager>())
{
    // Clear debug log file on startup and test file creation
    auto logFile1 = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("ymulator_debug.txt");
    auto logFile2 = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("ymulator_debug.txt");
    
    CS_DBG("Desktop path: " + logFile1.getFullPathName());
    CS_DBG("Temp path: " + logFile2.getFullPathName());
    
    if (logFile1.getParentDirectory().exists()) {
        logFile1.deleteFile();
        CS_DBG("Deleted desktop log file");
    } else {
        logFile2.deleteFile();
        CS_DBG("Deleted temp log file");
    }
    
    CS_DBG(" Constructor called");
    
    // Initialize MidiProcessor after other components are ready
    midiProcessor = std::make_unique<ymulatorsynth::MidiProcessor>(*voiceManager, *ymfmWrapper, parameters);
    
    // Initialize preset manager
    presetManager->initialize();
    
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

YMulatorSynthAudioProcessor::YMulatorSynthAudioProcessor(std::unique_ptr<YmfmWrapperInterface> ymfmWrapperPtr,
                                                        std::unique_ptr<VoiceManagerInterface> voiceManagerPtr,
                                                        std::unique_ptr<ymulatorsynth::MidiProcessorInterface> midiProcessorPtr,
                                                        std::unique_ptr<PresetManagerInterface> presetManagerPtr)
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("YMulatorSynth"), createParameterLayout()),
       ymfmWrapper(std::move(ymfmWrapperPtr)),
       voiceManager(std::move(voiceManagerPtr)),
       midiProcessor(std::move(midiProcessorPtr)),
       presetManager(std::move(presetManagerPtr))
{
    CS_DBG(" Dependency injection constructor called");
    
    // Initialize preset manager
    presetManager->initialize();
    
    // Load default preset (Init) before adding listener
    setCurrentPreset(7); // Init preset
    
    // Add parameter change listener through ValueTree after initial setup
    parameters.state.addListener(this);
    
    // Add parameter listeners for user-initiated changes
    const auto& allParams = AudioProcessor::getParameters();
    for (auto* param : allParams) {
        param->addListener(this);
    }
    
    CS_DBG(" Dependency injection constructor completed - default preset: " + juce::String(currentPreset));
}

YMulatorSynthAudioProcessor::~YMulatorSynthAudioProcessor()
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
    return presetManager->getNumPresets() + (isCustomPreset ? 1 : 0);
}

int YMulatorSynthAudioProcessor::getCurrentProgram()
{
    if (isCustomPreset) {
        return presetManager->getNumPresets(); // Custom preset index
    }
    return currentPreset;
}

void YMulatorSynthAudioProcessor::setCurrentProgram(int index)
{
    CS_DBG(" setCurrentProgram called with index: " + juce::String(index) + 
        ", current isCustomPreset: " + juce::String(isCustomPreset ? "true" : "false"));
    
    // Check if this is the custom preset index
    if (index == presetManager->getNumPresets() && isCustomPreset) {
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
    
    // Find which bank/preset this global index corresponds to and update state
    bool foundBankPreset = false;
    for (int bankIdx = 0; bankIdx < static_cast<int>(presetManager->getBanks().size()); ++bankIdx) {
        const auto& bank = presetManager->getBanks()[bankIdx];
        for (int presetIdx = 0; presetIdx < static_cast<int>(bank.presetIndices.size()); ++presetIdx) {
            if (bank.presetIndices[presetIdx] == index) {
                // Found the bank/preset combination - update state
                parameters.state.setProperty(ParamID::Global::CurrentBankIndex, bankIdx, nullptr);
                parameters.state.setProperty(ParamID::Global::CurrentPresetInBank, presetIdx, nullptr);
                
                foundBankPreset = true;
                break;
            }
        }
        if (foundBankPreset) break;
    }
    
    // Notify the UI to update the preset combo box
    // Send a special property change to trigger UI update without affecting custom state
    juce::MessageManager::callAsync([this]() {
        parameters.state.sendPropertyChangeMessage("presetIndexChanged");
    });
}

const juce::String YMulatorSynthAudioProcessor::getProgramName(int index)
{
    // Handle custom preset case
    if (index == presetManager->getNumPresets() && isCustomPreset) {
        return customPresetName;
    }
    
    if (index >= 0 && index < presetManager->getNumPresets())
    {
        auto presetNames = presetManager->getPresetNames();
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
    
    // File debug output for sample rate analysis
    static int callCount = 0;
    callCount++;
    CS_FILE_DBG("=== prepareToPlay called (call #" + juce::String(callCount) + ") ===");
    CS_FILE_DBG("DAW provided sampleRate: " + juce::String(sampleRate, 2));
    CS_FILE_DBG("samplesPerBlock: " + juce::String(samplesPerBlock));
    CS_FILE_DBG("sampleRate as uint32_t: " + juce::String(static_cast<uint32_t>(sampleRate)));
    
    // Initialize ymfm wrapper with OPM for now
    ymfmWrapper->initialize(YmfmWrapperInterface::ChipType::OPM, static_cast<uint32_t>(sampleRate));
    
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
    CS_FILE_DBG("=== releaseResources called ===");
    
    // Clear all voices to prevent audio after stop
    voiceManager->releaseAllVoices();
    
    // Reset ymfm to clear any lingering audio
    ymfmWrapper->reset();
    
    CS_FILE_DBG("=== releaseResources complete ===");
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
    
    // Process all MIDI events through MidiProcessor
    midiProcessor->processMidiMessages(midiMessages);
    
    // Update parameters periodically
    if (++parameterUpdateCounter >= PARAMETER_UPDATE_RATE_DIVIDER)
    {
        parameterUpdateCounter = 0;
        updateYmfmParameters();
    }
    
    // Generate audio samples
    generateAudioSamples(buffer);
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
            
            // Restore user data (imported OMP files and user presets) FIRST
            int restoredItems = presetManager->loadUserData();
            CS_DBG(" User data restored: " + juce::String(restoredItems) + " items, notifying UI of bank list changes");
            
            // THEN apply the preset (after banks are loaded)
            if (!isCustomPreset && ymfmWrapper->isInitialized()) {
                CS_DBG(" Applying preset after state restore");
                setCurrentPreset(currentPreset);
            } else if (!isCustomPreset) {
                CS_DBG(" Deferring preset application until ymfm init");
                needsPresetReapply = true;
            } else {
                CS_DBG(" Staying in custom mode after state restore");
            }
            
            // Notify UI that banks/presets may have changed
            juce::MessageManager::callAsync([this]() {
                parameters.state.sendPropertyChangeMessage("bankListUpdated");
            });
            
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
        
    // Global Pan parameter
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        ParamID::Global::GlobalPan, "Global Pan",
        juce::StringArray{"Left", "Center", "Right", "Random"},
        1)); // Default: Center
    
    // Bank/Preset state parameters for DAW persistence
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::CurrentBankIndex, "Current Bank Index", 0, 99, 0)); // 0 = Factory bank by default
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::CurrentPresetInBank, "Current Preset In Bank", 0, 127, 7)); // Default to Init preset
    
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
    // DEPRECATED: This is now handled by MidiProcessor, but kept for backward compatibility
    // The MidiProcessor setupCCMapping is called during its construction
    CS_DBG("setupCCMapping called (deprecated - now handled by MidiProcessor)");
}

void YMulatorSynthAudioProcessor::handleMidiCC(int ccNumber, int value)
{
    // DEPRECATED: Delegate to MidiProcessor
    if (midiProcessor) {
        midiProcessor->handleMidiCC(ccNumber, value);
    }
}

void YMulatorSynthAudioProcessor::handlePitchBend(int pitchBendValue)
{
    // DEPRECATED: Delegate to MidiProcessor
    if (midiProcessor) {
        midiProcessor->handlePitchBend(pitchBendValue);
    }
}


void YMulatorSynthAudioProcessor::setCurrentPreset(int index)
{
    // Assert valid preset index range
    CS_ASSERT_PARAMETER_RANGE(index, 0, presetManager->getNumPresets() - 1);
    
    if (index >= 0 && index < presetManager->getNumPresets())
    {
        currentPreset = index;
        isCustomPreset = false; // Reset custom state when loading factory preset
        
        if (ymfmWrapper->isInitialized()) {
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

int YMulatorSynthAudioProcessor::loadOpmFile(const juce::File& file)
{
    CS_DBG("YMulatorSynthAudioProcessor::loadOpmFile - Loading file: " + file.getFullPathName());
    
    int numLoaded = presetManager->loadOPMFile(file);
    
    if (numLoaded > 0)
    {
        CS_DBG("Successfully loaded " + juce::String(numLoaded) + " presets from OPM file");
        
        // Notify ValueTree listeners that preset list has been updated
        parameters.state.setProperty("presetListUpdated", juce::Random::getSystemRandom().nextInt(), nullptr);
        
        // Update host display to refresh preset list in DAW
        updateHostDisplay();
    }
    else
    {
        CS_DBG("Failed to load any presets from OPM file");
    }
    
    return numLoaded;
}

bool YMulatorSynthAudioProcessor::saveCurrentPresetAsOpm(const juce::File& file, const juce::String& presetName)
{
    CS_DBG("YMulatorSynthAudioProcessor::saveCurrentPresetAsOpm - Saving to: " + file.getFullPathName());
    
    // Create a preset from current parameters
    ymulatorsynth::Preset currentPreset;
    
    // Set preset name
    currentPreset.name = presetName.toStdString();
    
    // Get global parameters
    if (auto* param = parameters.getParameter("algorithm")) {
        currentPreset.algorithm = static_cast<uint8_t>(param->getValue() * 7.0f);
    }
    if (auto* param = parameters.getParameter("feedback")) {
        currentPreset.feedback = static_cast<uint8_t>(param->getValue() * 7.0f);
    }
    
    // Get LFO parameters
    if (auto* param = parameters.getParameter(ParamID::Global::LfoRate)) {
        currentPreset.lfo.rate = static_cast<int>(param->getValue() * 255.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::LfoAmd)) {
        currentPreset.lfo.amd = static_cast<int>(param->getValue() * 127.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::LfoPmd)) {
        currentPreset.lfo.pmd = static_cast<int>(param->getValue() * 127.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::LfoWaveform)) {
        currentPreset.lfo.waveform = static_cast<int>(param->getValue() * 3.0f);
    }
    
    // Get Noise parameters
    if (auto* param = parameters.getParameter(ParamID::Global::NoiseFrequency)) {
        currentPreset.lfo.noiseFreq = static_cast<int>(param->getValue() * 31.0f);
    }
    
    // Get operator parameters
    for (int op = 0; op < 4; ++op)
    {
        auto& opData = currentPreset.operators[op];
        
        if (auto* param = parameters.getParameter(ParamID::Op::ar(op + 1))) {
            opData.attackRate = param->getValue() * 31.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d1r(op + 1))) {
            opData.decay1Rate = param->getValue() * 31.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d2r(op + 1))) {
            opData.decay2Rate = param->getValue() * 31.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::rr(op + 1))) {
            opData.releaseRate = param->getValue() * 15.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d1l(op + 1))) {
            opData.sustainLevel = param->getValue() * 15.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::tl(op + 1))) {
            opData.totalLevel = param->getValue() * 127.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::ks(op + 1))) {
            opData.keyScale = param->getValue() * 3.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::mul(op + 1))) {
            opData.multiple = param->getValue() * 15.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::dt1(op + 1))) {
            opData.detune1 = param->getValue() * 7.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::dt2(op + 1))) {
            opData.detune2 = param->getValue() * 3.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::ams_en(op + 1))) {
            opData.amsEnable = param->getValue() > 0.5f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::slot_en(op + 1))) {
            opData.slotEnable = param->getValue() > 0.5f;
        }
    }
    
    // Get channel parameters (first channel as template)
    if (auto* param = parameters.getParameter(ParamID::Channel::ams(0))) {
        currentPreset.channels[0].ams = static_cast<int>(param->getValue() * 3.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Channel::pms(0))) {
        currentPreset.channels[0].pms = static_cast<int>(param->getValue() * 7.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::NoiseEnable)) {
        currentPreset.channels[0].noiseEnable = param->getValue() > 0.5f ? 1 : 0;
    }
    
    // Copy channel 0 settings to all channels
    for (int ch = 1; ch < 8; ++ch) {
        currentPreset.channels[ch] = currentPreset.channels[0];
    }
    
    // Save using PresetManager
    bool success = presetManager->savePresetAsOPM(file, currentPreset);
    
    if (success)
    {
        CS_DBG("Successfully saved preset as OPM file");
    }
    else
    {
        CS_DBG("Failed to save preset as OPM file");
    }
    
    return success;
}

bool YMulatorSynthAudioProcessor::saveCurrentPresetToUserBank(const juce::String& presetName)
{
    CS_DBG("YMulatorSynthAudioProcessor::saveCurrentPresetToUserBank - Saving: " + presetName);
    
    // Create a preset from current parameters (reuse the logic from saveCurrentPresetAsOpm)
    ymulatorsynth::Preset currentPreset;
    
    // Set preset name
    currentPreset.name = presetName.toStdString();
    
    // Get global parameters
    if (auto* param = parameters.getParameter("algorithm")) {
        currentPreset.algorithm = static_cast<uint8_t>(param->getValue() * 7.0f);
    }
    if (auto* param = parameters.getParameter("feedback")) {
        currentPreset.feedback = static_cast<uint8_t>(param->getValue() * 7.0f);
    }
    
    // Get LFO parameters
    if (auto* param = parameters.getParameter(ParamID::Global::LfoRate)) {
        currentPreset.lfo.rate = static_cast<int>(param->getValue() * 255.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::LfoAmd)) {
        currentPreset.lfo.amd = static_cast<int>(param->getValue() * 127.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::LfoPmd)) {
        currentPreset.lfo.pmd = static_cast<int>(param->getValue() * 127.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::LfoWaveform)) {
        currentPreset.lfo.waveform = static_cast<int>(param->getValue() * 3.0f);
    }
    
    // Get Noise parameters
    if (auto* param = parameters.getParameter(ParamID::Global::NoiseFrequency)) {
        currentPreset.lfo.noiseFreq = static_cast<int>(param->getValue() * 31.0f);
    }
    
    // Get operator parameters
    for (int op = 0; op < 4; ++op)
    {
        auto& opData = currentPreset.operators[op];
        
        if (auto* param = parameters.getParameter(ParamID::Op::ar(op + 1))) {
            opData.attackRate = param->getValue() * 31.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d1r(op + 1))) {
            opData.decay1Rate = param->getValue() * 31.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d2r(op + 1))) {
            opData.decay2Rate = param->getValue() * 31.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::rr(op + 1))) {
            opData.releaseRate = param->getValue() * 15.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::d1l(op + 1))) {
            opData.sustainLevel = param->getValue() * 15.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::tl(op + 1))) {
            opData.totalLevel = param->getValue() * 127.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::ks(op + 1))) {
            opData.keyScale = param->getValue() * 3.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::mul(op + 1))) {
            opData.multiple = param->getValue() * 15.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::dt1(op + 1))) {
            opData.detune1 = param->getValue() * 7.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::dt2(op + 1))) {
            opData.detune2 = param->getValue() * 3.0f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::ams_en(op + 1))) {
            opData.amsEnable = param->getValue() > 0.5f;
        }
        if (auto* param = parameters.getParameter(ParamID::Op::slot_en(op + 1))) {
            opData.slotEnable = param->getValue() > 0.5f;
        }
    }
    
    // Get channel parameters (first channel as template)
    if (auto* param = parameters.getParameter(ParamID::Channel::ams(0))) {
        currentPreset.channels[0].ams = static_cast<int>(param->getValue() * 3.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Channel::pms(0))) {
        currentPreset.channels[0].pms = static_cast<int>(param->getValue() * 7.0f);
    }
    if (auto* param = parameters.getParameter(ParamID::Global::NoiseEnable)) {
        currentPreset.channels[0].noiseEnable = param->getValue() > 0.5f ? 1 : 0;
    }
    
    // Copy channel 0 settings to all channels
    for (int ch = 1; ch < 8; ++ch) {
        currentPreset.channels[ch] = currentPreset.channels[0];
    }
    
    // Add to User bank
    bool success = presetManager->addUserPreset(currentPreset);
    
    if (success) {
        CS_DBG("Successfully saved preset '" + presetName + "' to User bank");
        
        // Switch out of custom mode and to the newly saved preset
        isCustomPreset = false;
        
        // Notify that preset list has been updated
        parameters.state.setProperty("presetListUpdated", juce::Random::getSystemRandom().nextInt(), nullptr);
        updateHostDisplay();
    } else {
        CS_DBG("Failed to save preset to User bank");
    }
    
    return success;
}

void YMulatorSynthAudioProcessor::loadPreset(int index)
{
    // Assert valid preset index range
    CS_ASSERT_PARAMETER_RANGE(index, 0, presetManager->getNumPresets() - 1);
    
    auto preset = presetManager->getPreset(index);
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
    
    float preservedGlobalPan = 0.0f;
    
    // Disable listeners during preset loading
    setupParameterListeners(false);
    
    // Load all preset parameters
    loadPresetParameters(preset, preservedGlobalPan);
    
    // Re-enable listeners
    setupParameterListeners(true);
    
    // Restore global pan setting
    if (auto* globalPanParam = parameters.getParameter(ParamID::Global::GlobalPan)) {
        globalPanParam->setValueNotifyingHost(preservedGlobalPan);
        CS_DBG(" Restored global pan value: " + juce::String(preservedGlobalPan));
        applyGlobalPanToAllChannels();
    }
    
    // Apply preset to ymfm engine
    applyPresetToYmfm(preset);
    
    CS_DBG(" OP1 loaded - TL: " + juce::String(preset->operators[0].totalLevel) + 
        ", AR: " + juce::String(preset->operators[0].attackRate) +
        ", MUL: " + juce::String(preset->operators[0].multiple));
}

void YMulatorSynthAudioProcessor::parameterValueChanged(int parameterIndex, float newValue)
{
    // CS_FILE_DBG("parameterValueChanged called - index: " + juce::String(parameterIndex) + 
    //             ", value: " + juce::String(newValue));
    
    // Check if this is the GlobalPan parameter by ID
    auto* globalPanParam = parameters.getParameter(ParamID::Global::GlobalPan);
    auto& allParams = AudioProcessor::getParameters();
    bool isGlobalPanChange = (parameterIndex < allParams.size() && allParams[parameterIndex] == globalPanParam);
    
    if (isGlobalPanChange) {
        // Apply to ALL channels, not just active ones
        // This is necessary because YM2151 mixes all channels, not just active ones
        applyGlobalPanToAllChannels();
        
        // Global pan changes don't affect preset identity, so return early
        return;
    }
    
    // Only switch to custom if not already in custom mode and gesture is in progress
    // Global pan changes are excluded from this logic
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
    if (!ymfmWrapper->isInitialized()) {
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
        ymfmWrapper->setAlgorithm(channel, algorithm);
        ymfmWrapper->setFeedback(channel, feedback);
        
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
            ymfmWrapper->setOperatorEnvelope(channel, op, ar, d1r, d2r, rr, d1l);
            
            // Set TL considering SLOT enable/disable state
            bool slotEnabled = *parameters.getRawParameterValue(ParamID::Op::slot_en(op + 1).c_str()) >= 0.5f;
            int effectiveTL = slotEnabled ? tl : 127; // Mute if slot disabled
            ymfmWrapper->setOperatorParameter(channel, op, YmfmWrapperInterface::OperatorParameter::TotalLevel, effectiveTL);
            ymfmWrapper->setOperatorParameter(channel, op, YmfmWrapperInterface::OperatorParameter::KeyScale, ks);
            ymfmWrapper->setOperatorParameter(channel, op, YmfmWrapperInterface::OperatorParameter::Multiple, mul);
            ymfmWrapper->setOperatorParameter(channel, op, YmfmWrapperInterface::OperatorParameter::Detune1, dt1);
            ymfmWrapper->setOperatorParameter(channel, op, YmfmWrapperInterface::OperatorParameter::Detune2, dt2);
        }
        
        // Update channel pan
        float pan = *parameters.getRawParameterValue(ParamID::Channel::pan(channel).c_str());
        CS_ASSERT_PAN_RANGE(pan);
        ymfmWrapper->setChannelPan(channel, pan);
        
        // Update channel AMS/PMS settings
        int ams = static_cast<int>(*parameters.getRawParameterValue(ParamID::Channel::ams(channel).c_str()));
        int pms = static_cast<int>(*parameters.getRawParameterValue(ParamID::Channel::pms(channel).c_str()));
        CS_ASSERT_PARAMETER_RANGE(ams, 0, 3);
        CS_ASSERT_PARAMETER_RANGE(pms, 0, 7);
        ymfmWrapper->setChannelAmsPms(channel, static_cast<uint8_t>(ams), static_cast<uint8_t>(pms));
        
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
    
    ymfmWrapper->setLfoParameters(static_cast<uint8_t>(lfoRate), static_cast<uint8_t>(lfoAmd), 
                                  static_cast<uint8_t>(lfoPmd), static_cast<uint8_t>(lfoWaveform));
    
    // Update noise parameters
    bool noiseEnable = *parameters.getRawParameterValue(ParamID::Global::NoiseEnable) >= 0.5f;
    int noiseFrequency = static_cast<int>(*parameters.getRawParameterValue(ParamID::Global::NoiseFrequency));
    
    CS_ASSERT_PARAMETER_RANGE(noiseFrequency, 0, 31);
    
    ymfmWrapper->setNoiseParameters(noiseEnable, static_cast<uint8_t>(noiseFrequency));
    
    // CRITICAL: Apply global pan AFTER all other parameter updates
    // This ensures global pan overrides individual channel pan settings
    applyGlobalPanToAllChannels();
}

juce::StringArray YMulatorSynthAudioProcessor::getBankNames() const
{
    juce::StringArray names;
    const auto& banks = presetManager->getBanks();
    
    for (const auto& bank : banks) {
        names.add(bank.name);
    }
    
    return names;
}

void YMulatorSynthAudioProcessor::setCurrentPresetInBank(int bankIndex, int presetIndex)
{
    int globalIndex = presetManager->getGlobalPresetIndex(bankIndex, presetIndex);
    if (globalIndex >= 0) {
        // Save bank/preset state to ValueTreeState for DAW persistence
        auto bankParam = parameters.getParameter(ParamID::Global::CurrentBankIndex);
        auto presetParam = parameters.getParameter(ParamID::Global::CurrentPresetInBank);
        
        if (bankParam && presetParam) {
            bankParam->setValueNotifyingHost(bankParam->convertTo0to1(static_cast<float>(bankIndex)));
            presetParam->setValueNotifyingHost(presetParam->convertTo0to1(static_cast<float>(presetIndex)));
        }
        
        setCurrentProgram(globalIndex);
    }
}

void YMulatorSynthAudioProcessor::applyGlobalPan(int channel)
{
    CS_ASSERT_CHANNEL(channel);
    
    // LIGHTWEIGHT DEBUG - only for important info
    // CS_DBG("=== applyGlobalPan called for channel " + juce::String(channel) + " ===");
    
    // 現在のレジスタ値を読み取り（他のビットを保持）
    uint8_t currentReg = ymfmWrapper->readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
    uint8_t otherBits = currentReg & YM2151Regs::PRESERVE_ALG_FB;  // パン以外のビット
    
    // CS_DBG("Current register value: 0x" + juce::String::toHexString(currentReg) + 
    //             ", other bits: 0x" + juce::String::toHexString(otherBits));
    
    // グローバルパン設定を取得
    auto* panParam = static_cast<juce::AudioParameterChoice*>(parameters.getParameter(ParamID::Global::GlobalPan));
    int panIndex = panParam ? panParam->getIndex() : 1; // デフォルトはCenter
    auto panChoice = static_cast<GlobalPanPosition>(panIndex);
    uint8_t panBits;
    
    // CS_DBG("Pan parameter index: " + juce::String(panIndex) + 
    //             ", pan choice: " + juce::String(static_cast<int>(panChoice)));
    
    switch(panChoice) {
        case GlobalPanPosition::LEFT:   
            panBits = YM2151Regs::PAN_LEFT_ONLY;
            CS_DBG("Setting LEFT pan (0x" + juce::String::toHexString(panBits) + ")");
            break;
        case GlobalPanPosition::CENTER: 
            panBits = YM2151Regs::PAN_CENTER;
            CS_DBG("Setting CENTER pan (0x" + juce::String::toHexString(panBits) + ")");
            break;
        case GlobalPanPosition::RIGHT:  
            panBits = YM2151Regs::PAN_RIGHT_ONLY;
            CS_DBG("Setting RIGHT pan (0x" + juce::String::toHexString(panBits) + ")");
            break;
        case GlobalPanPosition::RANDOM:
            // Use the stored random pan value for this channel
            panBits = channelRandomPanBits[channel];
            break;
    }
    
    uint8_t finalRegValue = otherBits | panBits;
    // CS_DBG("Writing to register 0x" + juce::String::toHexString(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel) + 
    //             " value: 0x" + juce::String::toHexString(finalRegValue));
    
    // YM2151に書き込み
    ymfmWrapper->writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, finalRegValue);
    
    // IMMEDIATE VERIFICATION: Read back the register to confirm it was written correctly
    // DISABLED FOR PERFORMANCE
    // uint8_t verifyReg = ymfmWrapper.readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
    // CS_FILE_DBG("VERIFY: Register 0x" + juce::String::toHexString(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel) + 
    //             " now reads: 0x" + juce::String::toHexString(verifyReg));
    // if ((verifyReg & YM2151Regs::MASK_PAN_LR) != panBits) {
    //     CS_FILE_DBG("ERROR: Pan bits verification failed! Expected: 0x" + juce::String::toHexString(panBits) + 
    //                 ", Got: 0x" + juce::String::toHexString(verifyReg & YM2151Regs::MASK_PAN_LR));
    // }
    
    // CS_FILE_DBG("Applied global pan to channel " + juce::String(channel) + 
    //             ", pan bits: 0x" + juce::String::toHexString(panBits));
}

void YMulatorSynthAudioProcessor::applyGlobalPanToAllChannels()
{
    // CS_FILE_DBG("=== Applying global pan to ALL 8 channels ===");
    
    // Apply global pan to all 8 YM2151 channels
    for (int channel = 0; channel < YM2151Regs::MAX_OPM_CHANNELS; ++channel) {
        applyGlobalPan(channel);
    }
    
    // CS_FILE_DBG("Global pan applied to all channels");
}

void YMulatorSynthAudioProcessor::setChannelRandomPan(int channel)
{
    CS_ASSERT_CHANNEL(channel);
    
    // Generate new random pan value for this channel
    int r = juce::Random::getSystemRandom().nextInt(3);
    channelRandomPanBits[channel] = (r == 0) ? YM2151Regs::PAN_LEFT_ONLY : 
                                   (r == 1) ? YM2151Regs::PAN_RIGHT_ONLY : YM2151Regs::PAN_CENTER;
}

void YMulatorSynthAudioProcessor::processMidiMessages(juce::MidiBuffer& midiMessages)
{
    // DEPRECATED: Now handled by MidiProcessor directly in processBlock
    // This method is kept for backward compatibility but should not be called
    CS_DBG("DEPRECATED processMidiMessages called - should use midiProcessor directly");
}

void YMulatorSynthAudioProcessor::processMidiNoteOn(const juce::MidiMessage& message)
{
    // Delegate to MidiProcessor
    midiProcessor->processMidiNoteOn(message);
}

void YMulatorSynthAudioProcessor::processMidiNoteOff(const juce::MidiMessage& message)
{
    // Delegate to MidiProcessor
    midiProcessor->processMidiNoteOff(message);
}

void YMulatorSynthAudioProcessor::generateAudioSamples(juce::AudioBuffer<float>& buffer)
{
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
        
        ymfmWrapper->generateSamples(leftBuffer, rightBuffer, numSamples);
        
        // DEBUG: Measure left/right channel levels for pan analysis
        static int panDebugCounter = 0;
        static bool lastHadAudio = false;
        
        if (++panDebugCounter % 2048 == 0) { // Every ~2048 samples
            float leftLevel = 0.0f, rightLevel = 0.0f;
            float leftRMS = 0.0f, rightRMS = 0.0f;
            
            for (int i = 0; i < numSamples; i++) {
                float leftSample = leftBuffer[i];
                float rightSample = rightBuffer[i];
                leftLevel = std::max(leftLevel, std::abs(leftSample));
                rightLevel = std::max(rightLevel, std::abs(rightSample));
                leftRMS += leftSample * leftSample;
                rightRMS += rightSample * rightSample;
            }
            
            leftRMS = std::sqrt(leftRMS / numSamples);
            rightRMS = std::sqrt(rightRMS / numSamples);
            
            bool hasAudio = (leftLevel > 0.0001f || rightLevel > 0.0001f);
            lastHadAudio = hasAudio;
        }
        
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

void YMulatorSynthAudioProcessor::setupParameterListeners(bool enable)
{
    if (enable) {
        // Re-add all listeners
        parameters.state.addListener(this);
        const auto& allParams = AudioProcessor::getParameters();
        for (auto* param : allParams) {
            param->addListener(this);
        }
    } else {
        // Remove all listeners
        parameters.state.removeListener(this);
        const auto& allParams = AudioProcessor::getParameters();
        for (auto* param : allParams) {
            param->removeListener(this);
        }
    }
}

void YMulatorSynthAudioProcessor::loadPresetParameters(const ymulatorsynth::Preset* preset, float& preservedGlobalPan)
{
    // Preserve current global pan setting before loading preset
    if (auto* globalPanParam = parameters.getParameter(ParamID::Global::GlobalPan)) {
        preservedGlobalPan = globalPanParam->getValue();
        CS_DBG(" Preserving global pan value: " + juce::String(preservedGlobalPan));
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
            param->setValueNotifyingHost(preset->operators[op].slotEnable ? 1.0f : 0.0f);
        }
    }
}

void YMulatorSynthAudioProcessor::applyPresetToYmfm(const ymulatorsynth::Preset* preset)
{
    // Use optimized batch update for preset loading (more efficient than updateYmfmParameters)
    if (ymfmWrapper->isInitialized()) {
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
            ymfmWrapper->batchUpdateChannelParameters(channel, preset->algorithm, preset->feedback, operatorParams);
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

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YMulatorSynthAudioProcessor();
}