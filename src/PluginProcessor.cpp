#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/Debug.h"
#include "utils/ParameterIDs.h"
#include "dsp/YM2151Registers.h"

using namespace ymulatorsynth;

// Global thread_local variables for test isolation
thread_local bool g_hasLoggedFirstCall = false;
thread_local int g_processBlockCallCounter = 0;
thread_local bool g_ymfmInitialized = false;
thread_local uint32_t g_lastSampleRate = 0;

YMulatorSynthAudioProcessor::YMulatorSynthAudioProcessor()
     : AudioProcessor(BusesProperties()
                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, juce::Identifier("YMulatorSynth"), ymulatorsynth::ParameterManager::createParameterLayout()),
       ymfmWrapper(std::make_unique<YmfmWrapper>()),
       voiceManager(std::make_unique<VoiceManager>()),
       midiProcessor(nullptr), // Will be initialized after other components
       panProcessor(std::make_shared<ymulatorsynth::PanProcessor>(*ymfmWrapper)),
       parameterManager(std::make_unique<ymulatorsynth::ParameterManager>(*ymfmWrapper, *this, panProcessor)),
       presetManager(std::make_unique<ymulatorsynth::PresetManager>())
{
    
    CS_DBG(" Constructor called");
    
    // Initialize ParameterManager with parameters
    parameterManager->initializeParameters(parameters);
    
    // Initialize StateManager with dependencies
    stateManager = std::make_unique<ymulatorsynth::StateManager>(parameters, *presetManager, *parameterManager);
    
    // Initialize MidiProcessor after other components are ready
    midiProcessor = std::make_unique<ymulatorsynth::MidiProcessor>(*voiceManager, *ymfmWrapper, parameters, *parameterManager);
    
    // Initialize preset manager
    presetManager->initialize();
    
    // Load default preset (Init) 
    setCurrentProgram(7); // Init preset
    
    // Add parameter change listener through ValueTree after initial setup
    parameters.state.addListener(this);
    
    CS_DBG(" Constructor completed - default preset: " + juce::String(getCurrentProgram()));
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
    setCurrentProgram(7); // Init preset
    
    // Add parameter change listener through ValueTree after initial setup
    parameters.state.addListener(this);
    
    CS_DBG(" Dependency injection constructor completed - default preset: " + juce::String(getCurrentProgram()));
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

// State management methods moved to StateManager (delegated through header)

// setCurrentProgram, getProgramName, changeProgramName moved to StateManager (delegated through header)

void YMulatorSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Assert valid sample rate and buffer size
    CS_ASSERT_SAMPLE_RATE(sampleRate);
    CS_ASSERT_BUFFER_SIZE(samplesPerBlock);
    
    juce::ignoreUnused(samplesPerBlock);
    
    // Initialize ymfm wrapper with OPM for now (only if needed)
    uint32_t currentSampleRate = static_cast<uint32_t>(sampleRate);
    if (!g_ymfmInitialized || g_lastSampleRate != currentSampleRate) {
        ymfmWrapper->initialize(YmfmWrapperInterface::ChipType::OPM, currentSampleRate);
        g_ymfmInitialized = true;
        g_lastSampleRate = currentSampleRate;
        
        // Apply initial parameters only when truly initializing
        updateYmfmParameters();
    }
    
    // If a preset was set before ymfm was initialized, apply it now
    if (needsPresetReapply) {
        loadPreset(getCurrentProgram());
        needsPresetReapply = false;
        CS_DBG(" Applied deferred preset " + juce::String(getCurrentProgram()));
    }
    
    CS_DBG(" ymfm initialization complete");
}

void YMulatorSynthAudioProcessor::releaseResources()
{
    // Clear all voices to prevent audio after stop
    voiceManager->releaseAllVoices();
    
    // Reset ymfm to clear any lingering audio
    ymfmWrapper->reset();
    
    // Reset static variables for test isolation
    resetProcessBlockStaticState();
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
    
    g_processBlockCallCounter++;
    
    if (!g_hasLoggedFirstCall) {
        CS_DBG(" processBlock FIRST CALL - channels: " + juce::String(buffer.getNumChannels()) + 
            ", samples: " + juce::String(buffer.getNumSamples()));
        g_hasLoggedFirstCall = true;
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

// getStateInformation moved to StateManager (delegated through header)

// setStateInformation moved to StateManager (delegated through header)

// Delegated to ParameterManager::createParameterLayout() for maintainability
juce::AudioProcessorValueTreeState::ParameterLayout YMulatorSynthAudioProcessor::createParameterLayout()
{
    // Delegate to ParameterManager for maintainability
    return ymulatorsynth::ParameterManager::createParameterLayout();
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

// loadPreset(int) moved to StateManager (delegated through header)

// loadPreset(Preset*) moved to StateManager through ParameterManager

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
        
        // Note: UI notifications removed to prevent audio thread → Message Thread violations
        // Custom mode changes will be detected by UI through parameter listening
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

void YMulatorSynthAudioProcessor::resetProcessBlockStaticState()
{
    // Reset thread_local variables for test isolation
    g_hasLoggedFirstCall = false;
    g_processBlockCallCounter = 0;
    g_ymfmInitialized = false;
    g_lastSampleRate = 0;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new YMulatorSynthAudioProcessor();
}