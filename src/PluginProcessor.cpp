#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/Debug.h"
#include "utils/ParameterIDs.h"
#include "dsp/YM2151Registers.h"

using namespace ymulatorsynth;

YMulatorSynthAudioProcessor::YMulatorSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("YMulatorSynth"), ymulatorsynth::ParameterManager::createParameterLayout()),
       ymfmWrapper(std::make_unique<YmfmWrapper>()),
       voiceManager(std::make_unique<VoiceManager>()),
       midiProcessor(nullptr), // Will be initialized after other components
       parameterManager(std::make_unique<ymulatorsynth::ParameterManager>(*ymfmWrapper, *this)),
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
    
    // Initialize ParameterManager with parameters
    parameterManager->initializeParameters(parameters);
    
    // Initialize MidiProcessor after other components are ready
    midiProcessor = std::make_unique<ymulatorsynth::MidiProcessor>(*voiceManager, *ymfmWrapper, parameters, *parameterManager);
    
    // Initialize preset manager
    presetManager->initialize();
    
    // Load default preset (Init) 
    setCurrentPreset(7); // Init preset
    
    // Add parameter change listener through ValueTree after initial setup
    parameters.state.addListener(this);
    
    CS_DBG(" Constructor completed - default preset: " + juce::String(currentPreset));
}

YMulatorSynthAudioProcessor::YMulatorSynthAudioProcessor(std::unique_ptr<YmfmWrapperInterface> ymfmWrapperPtr,
                                                        std::unique_ptr<VoiceManagerInterface> voiceManagerPtr,
                                                        std::unique_ptr<ymulatorsynth::MidiProcessorInterface> midiProcessorPtr,
                                                        std::unique_ptr<ymulatorsynth::ParameterManager> parameterManagerPtr,
                                                        std::unique_ptr<PresetManagerInterface> presetManagerPtr)
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("YMulatorSynth"), ymulatorsynth::ParameterManager::createParameterLayout()),
       ymfmWrapper(std::move(ymfmWrapperPtr)),
       voiceManager(std::move(voiceManagerPtr)),
       midiProcessor(std::move(midiProcessorPtr)),
       parameterManager(std::move(parameterManagerPtr)),
       presetManager(std::move(presetManagerPtr))
{
    CS_DBG(" Dependency injection constructor called");
    
    // Initialize ParameterManager with parameters
    if (parameterManager) {
        parameterManager->initializeParameters(parameters);
    }
    
    // Initialize preset manager
    presetManager->initialize();
    
    // Load default preset (Init) before adding listener
    setCurrentPreset(7); // Init preset
    
    // Add parameter change listener through ValueTree after initial setup
    parameters.state.addListener(this);
    
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
    return presetManager->getNumPresets() + (isInCustomMode() ? 1 : 0);
}

int YMulatorSynthAudioProcessor::getCurrentProgram()
{
    if (isInCustomMode()) {
        return presetManager->getNumPresets(); // Custom preset index
    }
    return currentPreset;
}

void YMulatorSynthAudioProcessor::setCurrentProgram(int index)
{
    CS_DBG(" setCurrentProgram called with index: " + juce::String(index) + 
        ", current isCustomPreset: " + juce::String(isInCustomMode() ? "true" : "false"));
    
    // Check if this is the custom preset index
    if (index == presetManager->getNumPresets() && isInCustomMode()) {
        // Stay in custom mode, don't change anything
        CS_DBG(" Staying in custom preset mode");
        return;
    }
    
    // Reset custom state and load factory preset
    if (parameterManager) {
        parameterManager->setCustomMode(false);
    }
    
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
    if (index == presetManager->getNumPresets() && isInCustomMode()) {
        return getCustomPresetName();
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
    
    // Update parameters periodically (rate limiting handled by ParameterManager)
    updateYmfmParameters();
    
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
    state.setProperty("isCustomPreset", parameterManager->isInCustomMode(), nullptr);
    state.setProperty("customPresetName", parameterManager->getCustomPresetName(), nullptr);
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
    
    CS_DBG(" State saved - preset: " + juce::String(currentPreset) + 
        ", custom: " + juce::String(isInCustomMode() ? "true" : "false"));
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
            bool customMode = newState.getProperty("isCustomPreset", false);
            juce::String customName = newState.getProperty("customPresetName", "Custom");
            parameterManager->setCustomMode(customMode, customName);
            
            CS_DBG(" State loaded - preset: " + juce::String(currentPreset) + 
                ", custom: " + juce::String(customMode ? "true" : "false"));
            
            // Restore user data (imported OMP files and user presets) FIRST
            int restoredItems = presetManager->loadUserData();
            CS_DBG(" User data restored: " + juce::String(restoredItems) + " items, notifying UI of bank list changes");
            
            // THEN apply the preset (after banks are loaded)
            if (!isInCustomMode() && ymfmWrapper->isInitialized()) {
                CS_DBG(" Applying preset after state restore");
                setCurrentPreset(currentPreset);
            } else if (!isInCustomMode()) {
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

// DEPRECATED: Moved to ParameterManager::createParameterLayout()
juce::AudioProcessorValueTreeState::ParameterLayout YMulatorSynthAudioProcessor::createParameterLayout()
{
    // This method is deprecated - ParameterManager::createParameterLayout() should be used instead
    return ymulatorsynth::ParameterManager::createParameterLayout();
    
    // OLD IMPLEMENTATION BELOW (to be removed):
    /*
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
    */
}



void YMulatorSynthAudioProcessor::setCurrentPreset(int index)
{
    // Assert valid preset index range
    CS_ASSERT_PARAMETER_RANGE(index, 0, presetManager->getNumPresets() - 1);
    
    if (index >= 0 && index < presetManager->getNumPresets())
    {
        currentPreset = index;
        if (parameterManager) parameterManager->setCustomMode(false); // Reset custom state when loading factory preset
        
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
        if (parameterManager) parameterManager->setCustomMode(false);
        
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
        // Skip applying pan if RANDOM mode - MidiProcessor will handle per-note random pan
        if (static_cast<juce::AudioParameterChoice*>(globalPanParam)->getIndex() != static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM)) {
            applyGlobalPanToAllChannels();
        }
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
        // BUT: Skip if switching TO RANDOM mode - MidiProcessor will handle per-note random pan
        if (static_cast<juce::AudioParameterChoice*>(globalPanParam)->getIndex() != static_cast<int>(ymulatorsynth::GlobalPanPosition::RANDOM)) {
            applyGlobalPanToAllChannels();
        }
        
        // Global pan changes don't affect preset identity, so return early
        return;
    }
    
    // Only switch to custom if not already in custom mode and gesture is in progress
    // Global pan changes are excluded from this logic
    bool isInCustomMode = parameterManager ? parameterManager->isInCustomMode() : false;
    bool userGestureInProgress = parameterManager ? parameterManager->isUserGestureInProgress() : false;
    if (!isInCustomMode && userGestureInProgress) {
        CS_DBG(" Parameter changed by user gesture, switching to custom preset");
        if (parameterManager) parameterManager->setCustomMode(true);
        
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
    
    if (parameterManager) parameterManager->setUserGestureInProgress(gestureIsStarting);
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

// updateYmfmParameters method moved to ParameterManager

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

// applyGlobalPan, applyGlobalPanToAllChannels, setChannelRandomPan methods moved to ParameterManager

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

// setupParameterListeners method moved to ParameterManager

// loadPresetParameters method moved to ParameterManager

// applyPresetToYmfm method moved to ParameterManager

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YMulatorSynthAudioProcessor();
}