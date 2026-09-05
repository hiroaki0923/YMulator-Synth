#include "ParameterManager.h"
#include "../dsp/YM2151Registers.h"
#include "../utils/Debug.h"
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

using namespace ymulatorsynth;

namespace {
// Register value held by a parameter, honouring the parameter's own range
// (the previous "normalised * max" arithmetic truncated values for any
// parameter whose range does not start at 0 or whose float division rounds down).
float registerValue(const juce::RangedAudioParameter* param)
{
    return param ? static_cast<float>(juce::roundToInt(param->convertFrom0to1(param->getValue()))) : 0.0f;
}
} // namespace

// Static thread_local variable for test isolation
static thread_local bool s_isProcessingParameterChange = false;

// ============================================================================
// Constructor and Destructor
// ============================================================================

ParameterManager::ParameterManager(YmfmWrapperInterface& ymfm, juce::AudioProcessor& processor, 
                                 std::shared_ptr<PanProcessor> panProc)
    : ymfmWrapper(ymfm), audioProcessor(processor), panProcessor(panProc)
{
    CS_DBG("ParameterManager created with PanProcessor delegation");
}

ParameterManager::~ParameterManager()
{
    // Ensure listeners are removed if still registered
    if (parametersPtr) {
        setupParameterListeners(false);
    }
    CS_DBG("ParameterManager destroyed");
}

// ============================================================================
// Parameter System Setup
// ============================================================================

juce::AudioProcessorValueTreeState::ParameterLayout ParameterManager::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // ========================================================================
    // Operator Parameters (4 operators × 6 parameters = 24)
    // ========================================================================
    
    for (int op = 1; op <= 4; ++op) {
        // Total Level (0-127, inverted display)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::tl(op), "Op" + juce::String(op) + " TL", 0, 127, 0));
            
        // Attack Rate (0-31)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::ar(op), "Op" + juce::String(op) + " AR", 0, 31, 31));
            
        // Decay Rate (0-31)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::d1r(op), "Op" + juce::String(op) + " D1R", 0, 31, 0));
            
        // Sustain Level (0-15)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::d1l(op), "Op" + juce::String(op) + " D1L", 0, 15, 15));
            
        // Sustain Rate (0-31)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::d2r(op), "Op" + juce::String(op) + " D2R", 0, 31, 0));
            
        // Release Rate (0-15)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::rr(op), "Op" + juce::String(op) + " RR", 0, 15, 7));
            
        // Key Scale (0-3)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::ks(op), "Op" + juce::String(op) + " KS", 0, 3, 0));
            
        // Multiplier (0-15, with special 0.5 case)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::mul(op), "Op" + juce::String(op) + " MUL", 0, 15, 1));
            
        // Detune (0-7, ±3 range)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::dt1(op), "Op" + juce::String(op) + " DT1", 0, 7, 3));
            
        // Detune 2 (0-3)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::dt2(op), "Op" + juce::String(op) + " DT2", 0, 3, 0));
            
        // Amplitude Modulation Sensitivity (0-3)
        layout.add(std::make_unique<juce::AudioParameterInt>(
            ParamID::Op::ams_en(op), "Op" + juce::String(op) + " AMS", 0, 3, 0));
    }
    
    // ========================================================================
    // Channel Parameters (8 channels × individual pan = 8)
    // ========================================================================
    
    for (int ch = 0; ch < 8; ++ch) {
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            ParamID::Channel::pan(ch), 
            "Ch" + juce::String(ch) + " Pan", 
            juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    }
    
    // ========================================================================
    // Global Parameters (Algorithm, Feedback, etc.)
    // ========================================================================
    
    // Algorithm (0-7)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::Algorithm, "Algorithm", 0, 7, 0));
        
    // Feedback (0-7)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::Feedback, "Feedback", 0, 7, 0));
        
    // Global Pan (LEFT/CENTER/RIGHT/RANDOM)
    juce::StringArray panChoices = {"LEFT", "CENTER", "RIGHT", "RANDOM"};
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParamID::Global::GlobalPan, "Global Pan", panChoices, 1)); // Default: CENTER
        
    // LFO Parameters
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoRate, "LFO Rate", 0, 255, 0));
        
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoPmd, "LFO PMD", 0, 127, 0));
        
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::LfoAmd, "LFO AMD", 0, 127, 0));
        
    // LFO Waveform (0-3: Sawtooth, Square, Triangle, Noise)
    juce::StringArray lfoWaveforms = {"Sawtooth", "Square", "Triangle", "Noise"};
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParamID::Global::LfoWaveform, "LFO Waveform", lfoWaveforms, 0));
        
    // Noise Enable (boolean)
    layout.add(std::make_unique<juce::AudioParameterBool>(
        ParamID::Global::NoiseEnable, "Noise Enable", false));
        
    // Noise Frequency (0-31)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::NoiseFrequency, "Noise Frequency", 0, 31, 0));
        
    // Pitch Bend Range (1-12 semitones)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        ParamID::Global::PitchBendRange, "Pitch Bend Range", 1, 12, 2));
    
    CS_DBG("Created parameter layout successfully");
    return layout;
}

void ParameterManager::initializeParameters(juce::AudioProcessorValueTreeState& parameters)
{
    parametersPtr = &parameters;
    
    // Setup parameter listeners
    setupParameterListeners(true);
    
    CS_DBG("ParameterManager initialized with parameter ValueTree");
}

void ParameterManager::setupParameterListeners(bool enable)
{
    if (!parametersPtr) {
        CS_DBG("Cannot setup parameter listeners - no parameters initialized");
        return;
    }
    
    auto& allParams = audioProcessor.getParameters();
    
    if (enable) {
        for (auto* param : allParams) {
            param->addListener(this);
        }
        CS_DBG("Enabled parameter listeners for " + juce::String(allParams.size()) + " parameters");
    } else {
        for (auto* param : allParams) {
            param->removeListener(this);
        }
        CS_DBG("Disabled parameter listeners");
    }
}

// ============================================================================
// Core Parameter Management
// ============================================================================

void ParameterManager::updateYmfmParameters()
{
    if (!parametersPtr) {
        return;
    }
    
    // CS_FILE_DBG("updateYmfmParameters - Updating all parameters");
    
    // Update global parameters first
    updateGlobalParameters();
    
    // Update all channel parameters
    for (int channel = 0; channel < 8; ++channel) {
        updateChannelParameters(channel);
    }
    
    // Note: applyGlobalPanToAllChannels() removed - handled by parameterValueChanged() listener
}

void ParameterManager::parameterValueChanged(int parameterIndex, float newValue)
{
    if (!parametersPtr) {
        return;
    }
    
    // Recursion guard to prevent infinite loops
    if (s_isProcessingParameterChange) {
        CS_FILE_DBG("parameterValueChanged - Recursion detected, skipping to prevent infinite loop");
        return;
    }
    
    s_isProcessingParameterChange = true;
    
    // Check if this is the GlobalPan parameter change (always apply regardless of gesture state)
    auto* globalPanParam = static_cast<juce::AudioParameterChoice*>(
        parametersPtr->getParameter(ParamID::Global::GlobalPan));
    
    if (globalPanParam && audioProcessor.getParameters()[parameterIndex] == globalPanParam) {
        int panIndex = globalPanParam->getIndex();
        CS_FILE_DBG("parameterValueChanged - GlobalPan changed to index " + juce::String(panIndex) + 
                   " (value=" + juce::String(newValue) + ")");
        applyGlobalPanToAllChannels();
        s_isProcessingParameterChange = false; // Reset guard
        return; // GlobalPan changes don't affect custom preset mode
    }
    
    // Custom preset detection logic
    if (!userGestureInProgress) {
        s_isProcessingParameterChange = false; // Reset guard before early return
        return; // Only switch to custom mode during user gestures
    }
    
    if (!isCustomPreset) {
        setCustomMode(true);
        CS_DBG("Switched to custom preset mode due to parameter change");
    }
    
    // Reset recursion guard
    s_isProcessingParameterChange = false;
}

void ParameterManager::parameterGestureChanged(int parameterIndex, bool gestureIsStarting)
{
    userGestureInProgress = gestureIsStarting;
    CS_FILE_DBG("parameterGestureChanged - Gesture " + 
                juce::String(gestureIsStarting ? "started" : "ended") + 
                " for parameter " + juce::String(parameterIndex));
}

// ============================================================================
// Preset Parameter Management  
// ============================================================================

void ParameterManager::loadPresetParameters(const Preset* preset, float& preservedGlobalPan)
{
    if (!preset || !parametersPtr) {
        CS_DBG("Cannot load preset parameters - invalid preset or parameters not initialized");
        return;
    }
    
    // Temporarily disable listeners to prevent feedback during batch loading
    setupParameterListeners(false);
    
    CS_FILE_DBG("loadPresetParameters - Loading preset: " + preset->name);
    
    // Preserve global pan setting
    auto* globalPanParam = static_cast<juce::AudioParameterChoice*>(
        parametersPtr->getParameter(ParamID::Global::GlobalPan));
    if (globalPanParam) {
        preservedGlobalPan = globalPanParam->getCurrentChoiceName() == "LEFT" ? 0.0f :
                           globalPanParam->getCurrentChoiceName() == "CENTER" ? 0.33f :
                           globalPanParam->getCurrentChoiceName() == "RIGHT" ? 0.66f : 1.0f;
    }
    
    CS_DBG("Loading preset parameters: " + preset->name);
    
    // Load operator parameters
    for (int op = 1; op <= 4; ++op) {
        int opIndex = op - 1; // Convert to 0-based index
        
        parametersPtr->getParameter(ParamID::Op::tl(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::tl(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].totalLevel)));
        parametersPtr->getParameter(ParamID::Op::ar(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::ar(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].attackRate)));
        parametersPtr->getParameter(ParamID::Op::d1r(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::d1r(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].decay1Rate)));
        parametersPtr->getParameter(ParamID::Op::d1l(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::d1l(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].sustainLevel)));
        parametersPtr->getParameter(ParamID::Op::d2r(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::d2r(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].decay2Rate)));
        parametersPtr->getParameter(ParamID::Op::rr(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::rr(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].releaseRate)));
        parametersPtr->getParameter(ParamID::Op::ks(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::ks(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].keyScale)));
        parametersPtr->getParameter(ParamID::Op::mul(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::mul(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].multiple)));
        parametersPtr->getParameter(ParamID::Op::dt1(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::dt1(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].detune1)));
        parametersPtr->getParameter(ParamID::Op::dt2(op))->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Op::dt2(op))->convertTo0to1(static_cast<float>(preset->operators[opIndex].detune2)));
        parametersPtr->getParameter(ParamID::Op::ams_en(op))->setValueNotifyingHost(
            preset->operators[opIndex].amsEnable ? 1.0f : 0.0f);
    }
    
    // Load global parameters
    parametersPtr->getParameter(ParamID::Global::Algorithm)->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Global::Algorithm)->convertTo0to1(static_cast<float>(preset->algorithm)));
    parametersPtr->getParameter(ParamID::Global::Feedback)->setValueNotifyingHost(
            parametersPtr->getParameter(ParamID::Global::Feedback)->convertTo0to1(static_cast<float>(preset->feedback)));
        
    // Re-enable listeners
    setupParameterListeners(true);
    
    CS_DBG("Preset parameters loaded successfully");
}

void ParameterManager::applyPresetToYmfm(const Preset* preset)
{
    if (!preset) {
        CS_DBG("Cannot apply preset to ymfm - invalid preset");
        return;
    }
    
    CS_DBG("Applying preset to ymfm: " + preset->name);
    
    // Apply algorithm and feedback to all channels first
    for (int ch = 0; ch < 8; ++ch) {
        ymfmWrapper.setAlgorithm(ch, static_cast<uint8_t>(preset->algorithm));
        ymfmWrapper.setFeedback(ch, static_cast<uint8_t>(preset->feedback));
    }
    
    // Apply operator parameters for all channels
    for (int channel = 0; channel < 8; ++channel) {
        for (int op = 0; op < 4; ++op) {
            const auto& opParams = preset->operators[op];
            
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::TotalLevel, 
                static_cast<uint8_t>(opParams.totalLevel));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::AttackRate, 
                static_cast<uint8_t>(opParams.attackRate));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::Decay1Rate, 
                static_cast<uint8_t>(opParams.decay1Rate));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::SustainLevel, 
                static_cast<uint8_t>(opParams.sustainLevel));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::Decay2Rate, 
                static_cast<uint8_t>(opParams.decay2Rate));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::ReleaseRate, 
                static_cast<uint8_t>(opParams.releaseRate));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::KeyScale, 
                static_cast<uint8_t>(opParams.keyScale));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::Multiple, 
                static_cast<uint8_t>(opParams.multiple));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::Detune1, 
                static_cast<uint8_t>(opParams.detune1));
            ymfmWrapper.setOperatorParameter(channel, op, 
                YmfmWrapperInterface::OperatorParameter::Detune2, 
                static_cast<uint8_t>(opParams.detune2));
            
            // AMS enable is handled separately as a boolean
            ymfmWrapper.setOperatorAmsEnable(channel, op, opParams.amsEnable);
        }
    }
    
    CS_DBG("Preset applied to ymfm successfully");
}

void ParameterManager::extractCurrentParameterValues(Preset& preset) const
{
    if (!parametersPtr) {
        CS_DBG("Cannot extract parameters - parameters not initialized");
        return;
    }
    
    CS_DBG("Extracting current parameter values to preset");
    
    // Extract operator parameters
    for (int op = 1; op <= 4; ++op) {
        int opIndex = op - 1; // Convert to 0-based index
        auto& opParams = preset.operators[opIndex];
        
        opParams.totalLevel = static_cast<uint8_t>(
            registerValue(parametersPtr->getParameter(ParamID::Op::tl(op))));
        opParams.attackRate = static_cast<uint8_t>(
            registerValue(parametersPtr->getParameter(ParamID::Op::ar(op))));
        opParams.decay1Rate = registerValue(parametersPtr->getParameter(ParamID::Op::d1r(op)));
        opParams.sustainLevel = registerValue(parametersPtr->getParameter(ParamID::Op::d1l(op)));
        opParams.decay2Rate = registerValue(parametersPtr->getParameter(ParamID::Op::d2r(op)));
        opParams.releaseRate = registerValue(parametersPtr->getParameter(ParamID::Op::rr(op)));
        opParams.keyScale = registerValue(parametersPtr->getParameter(ParamID::Op::ks(op)));
        opParams.multiple = registerValue(parametersPtr->getParameter(ParamID::Op::mul(op)));
        opParams.detune1 = registerValue(parametersPtr->getParameter(ParamID::Op::dt1(op)));
        opParams.detune2 = registerValue(parametersPtr->getParameter(ParamID::Op::dt2(op)));
        opParams.amsEnable = parametersPtr->getParameter(ParamID::Op::ams_en(op))->getValue() > 0.5f;
    }
    
    // Extract global parameters
    preset.algorithm = static_cast<uint8_t>(
        registerValue(parametersPtr->getParameter(ParamID::Global::Algorithm)));
    preset.feedback = static_cast<uint8_t>(
        registerValue(parametersPtr->getParameter(ParamID::Global::Feedback)));
    
    CS_DBG("Parameter extraction completed");
}

// ============================================================================
// Global Pan Management
// ============================================================================

void ParameterManager::applyGlobalPan(int channel)
{
    if (!parametersPtr || !panProcessor) {
        return;
    }
    
    auto* globalPanParam = static_cast<juce::AudioParameterChoice*>(
        parametersPtr->getParameter(ParamID::Global::GlobalPan));
    
    if (!globalPanParam) {
        CS_DBG("GlobalPan parameter not found");
        return;
    }
    
    float panValue = globalPanParam->getIndex() / 3.0f;  // Convert index 0-3 to 0.0-1.0
    panProcessor->applyGlobalPan(channel, panValue);
}

void ParameterManager::applyGlobalPanToAllChannels()
{
    if (!parametersPtr) {
        CS_DBG("ParameterManager::applyGlobalPanToAllChannels - Missing parametersPtr");
        return;
    }
    
    auto* globalPanParam = static_cast<juce::AudioParameterChoice*>(
        parametersPtr->getParameter(ParamID::Global::GlobalPan));
    
    if (!globalPanParam) {
        CS_DBG("GlobalPan parameter not found");
        return;
    }
    
    int panIndex = globalPanParam->getIndex();
    CS_FILE_DBG("ParameterManager::applyGlobalPanToAllChannels - Pan index: " + juce::String(panIndex));
    
    // Apply to all 8 YM2151 channels directly (restore working logic)
    for (int channel = 0; channel < 8; ++channel) {
        // Read current register value to preserve algorithm/feedback bits
        uint8_t currentReg = ymfmWrapper.readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        uint8_t otherBits = currentReg & YM2151Regs::PRESERVE_ALG_FB;  // Preserve non-pan bits
        
        uint8_t panBits;
        switch(panIndex) {
            case 0: // LEFT
                panBits = YM2151Regs::PAN_LEFT_ONLY;
                break;
            case 1: // CENTER
                panBits = YM2151Regs::PAN_CENTER;
                break;
            case 2: // RIGHT
                panBits = YM2151Regs::PAN_RIGHT_ONLY;
                break;
            case 3: // RANDOM
                // Use PanProcessor for random logic
                if (panProcessor) {
                    panProcessor->setChannelRandomPan(channel);
                    panBits = panProcessor->getChannelRandomPanBits(channel);
                } else {
                    panBits = YM2151Regs::PAN_CENTER; // Fallback
                }
                break;
            default:
                panBits = YM2151Regs::PAN_CENTER;
        }
        
        uint8_t finalRegValue = otherBits | panBits;
        CS_FILE_DBG("Channel " + juce::String(channel) + " - Current reg: 0x" + juce::String::toHexString(currentReg) + 
                   ", Other bits: 0x" + juce::String::toHexString(otherBits) +
                   ", Pan bits: 0x" + juce::String::toHexString(panBits) + 
                   ", Final reg: 0x" + juce::String::toHexString(finalRegValue));
        
        // Write directly to YM2151 register (restore working logic)
        ymfmWrapper.writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, finalRegValue);
        
        // Verify the write was successful
        uint8_t verifyReg = ymfmWrapper.readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel);
        CS_FILE_DBG("VERIFY Channel " + juce::String(channel) + " - Written: 0x" + juce::String::toHexString(finalRegValue) + 
                   ", Read back: 0x" + juce::String::toHexString(verifyReg));
    }
    
    CS_FILE_DBG("ParameterManager::applyGlobalPanToAllChannels - Applied pan index " + juce::String(panIndex) + " to all channels");
}

void ParameterManager::setChannelRandomPan(int channel)
{
    if (!panProcessor) {
        return;
    }
    
    panProcessor->setChannelRandomPan(channel);
}

// ============================================================================
// Custom Preset State Management
// ============================================================================

void ParameterManager::setCustomMode(bool custom, const juce::String& name)
{
    isCustomPreset = custom;
    customPresetName = name;
    
    CS_DBG("Custom preset mode: " + juce::String(custom ? "enabled" : "disabled") + 
           " name: " + name);
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

void ParameterManager::updateChannelParameters(int channel)
{
    CS_ASSERT_CHANNEL(channel);
    
    if (!parametersPtr) {
        return;
    }
    
    // Update operator parameters for this channel
    for (int op = 1; op <= 4; ++op) {
        int opIndex = op - 1; // Convert to 0-based for ymfm
        
        // Get parameter values (0.0-1.0) and scale to hardware ranges
        float tl = registerValue(parametersPtr->getParameter(ParamID::Op::tl(op)));
        float ar = registerValue(parametersPtr->getParameter(ParamID::Op::ar(op)));
        float d1r = registerValue(parametersPtr->getParameter(ParamID::Op::d1r(op)));
        float d1l = registerValue(parametersPtr->getParameter(ParamID::Op::d1l(op)));
        float d2r = registerValue(parametersPtr->getParameter(ParamID::Op::d2r(op)));
        float rr = registerValue(parametersPtr->getParameter(ParamID::Op::rr(op)));
        float ks = registerValue(parametersPtr->getParameter(ParamID::Op::ks(op)));
        float mul = registerValue(parametersPtr->getParameter(ParamID::Op::mul(op)));
        float dt1 = registerValue(parametersPtr->getParameter(ParamID::Op::dt1(op)));
        float dt2 = registerValue(parametersPtr->getParameter(ParamID::Op::dt2(op)));
        float ams = registerValue(parametersPtr->getParameter(ParamID::Op::ams_en(op)));
        
        // Scale and apply to ymfm
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::TotalLevel, 
            static_cast<uint8_t>(tl));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::AttackRate, 
            static_cast<uint8_t>(ar));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::Decay1Rate, 
            static_cast<uint8_t>(d1r));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::SustainLevel, 
            static_cast<uint8_t>(d1l));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::Decay2Rate, 
            static_cast<uint8_t>(d2r));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::ReleaseRate, 
            static_cast<uint8_t>(rr));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::KeyScale, 
            static_cast<uint8_t>(ks));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::Multiple, 
            static_cast<uint8_t>(mul));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::Detune1, 
            static_cast<uint8_t>(dt1));
        ymfmWrapper.setOperatorParameter(channel, opIndex, 
            YmfmWrapperInterface::OperatorParameter::Detune2, 
            static_cast<uint8_t>(dt2));
        
        // AMS enable is handled separately as a boolean
        ymfmWrapper.setOperatorAmsEnable(channel, opIndex, ams > 0.5f);
    }
    
    // Skip individual channel pan when global pan is active
    // Individual channel pan is only used when global pan is set to a "disabled" state
    // For now, since all global pan modes (LEFT/CENTER/RIGHT/RANDOM) should override individual channel pan,
    // we skip applying individual channel pan entirely
    // Individual channel pan is disabled while global pan system is active
}

void ParameterManager::updateGlobalParameters()
{
    if (!parametersPtr) {
        return;
    }
    
    // Algorithm and Feedback
    float algorithm = registerValue(parametersPtr->getParameter(ParamID::Global::Algorithm));
    float feedback = registerValue(parametersPtr->getParameter(ParamID::Global::Feedback));
    
    uint8_t algorithmValue = static_cast<uint8_t>(algorithm);
    uint8_t feedbackValue = static_cast<uint8_t>(feedback);
    
    CS_ASSERT_ALGORITHM(algorithmValue);
    CS_ASSERT_FEEDBACK(feedbackValue);
    
    // Apply algorithm and feedback to all channels
    for (int ch = 0; ch < 8; ++ch) {
        ymfmWrapper.setAlgorithm(ch, algorithmValue);
        ymfmWrapper.setFeedback(ch, feedbackValue);
    }
    
    // LFO Parameters
    float lfoRate = registerValue(parametersPtr->getParameter(ParamID::Global::LfoRate));
    float lfoPmd = registerValue(parametersPtr->getParameter(ParamID::Global::LfoPmd));
    float lfoAmd = registerValue(parametersPtr->getParameter(ParamID::Global::LfoAmd));
    auto* lfoWaveformParam = static_cast<juce::AudioParameterChoice*>(
        parametersPtr->getParameter(ParamID::Global::LfoWaveform));
    
    uint8_t lfoWaveform = lfoWaveformParam ? static_cast<uint8_t>(lfoWaveformParam->getIndex()) : 0;
    ymfmWrapper.setLfoParameters(
        static_cast<uint8_t>(lfoRate),
        static_cast<uint8_t>(lfoAmd),
        static_cast<uint8_t>(lfoPmd),
        lfoWaveform
    );
    
    // Noise Parameters
    auto* noiseEnableParam = static_cast<juce::AudioParameterBool*>(
        parametersPtr->getParameter(ParamID::Global::NoiseEnable));
    float noiseFreq = registerValue(parametersPtr->getParameter(ParamID::Global::NoiseFrequency));
    
    bool noiseEnable = noiseEnableParam ? noiseEnableParam->get() : false;
    ymfmWrapper.setNoiseParameters(noiseEnable, static_cast<uint8_t>(noiseFreq));
}

void ParameterManager::validateParameterRange(float value, float min, float max, const juce::String& paramName) const
{
    if (value < min || value > max) {
        CS_DBG("Parameter " + paramName + " out of range: " + 
               juce::String(value) + " (expected " + 
               juce::String(min) + "-" + juce::String(max) + ")");
    }
}

// getChannelRandomPanBits method removed - functionality moved to PanProcessor

void ParameterManager::resetStaticState()
{
    // Reset thread_local variable for test isolation
    s_isProcessingParameterChange = false;
    CS_DBG("ParameterManager::resetStaticState called - reset s_isProcessingParameterChange");
}