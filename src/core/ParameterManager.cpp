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
    
    // ========================================================================
    // Macro parameters: -50..+50 display, centre = preset unchanged
    // ========================================================================
    for (const char* id : { ParamID::Macro::Brightness, ParamID::Macro::Attack,
                            ParamID::Macro::Decay, ParamID::Macro::Release, ParamID::Macro::Spread }) {
        juce::String name = juce::String(id).replace("macro_", "").toUpperCase().substring(0, 1)
                          + juce::String(id).replace("macro_", "").substring(1);
        // Meta: moving a macro rewrites raw parameters (hosts and auval expect the flag)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            id, name, juce::NormalisableRange<float>(-50.0f, 50.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withMeta(true)));
    }
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        ParamID::Macro::Harmonics, "Harmonics",
        juce::StringArray{"Preset", "Saw", "Square", "Pulse", "Bright", "Bell", "Metal", "Sub", "Octave"}, 0,
        juce::AudioParameterChoiceAttributes().withMeta(true)));
    
    CS_DBG("Created parameter layout successfully");
    return layout;
}

void ParameterManager::initializeParameters(juce::AudioProcessorValueTreeState& parameters)
{
    parametersPtr = &parameters;
    
    // Setup parameter listeners
    setupParameterListeners(true);
    
    CS_DBG("ParameterManager initialized with parameter ValueTree");
    cacheParameterHandles();
    invalidateRegisterCache();
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
    
    updateGlobalParameters();
    
    for (int op = 0; op < 4; ++op) {
        for (int which = 0; which < NumOpParams; ++which) {
            auto* param = opParamHandles[static_cast<size_t>(op)][static_cast<size_t>(which)];
            if (param == nullptr) continue;
            const int value = static_cast<int>(registerValue(param));
            if (value == lastOpValues[static_cast<size_t>(op)][static_cast<size_t>(which)]) continue;
            lastOpValues[static_cast<size_t>(op)][static_cast<size_t>(which)] = value;
            writeOperatorParameterToAllChannels(op, static_cast<OpParam>(which), value);
        }
    }
}

void ParameterManager::invalidateRegisterCache()
{
    for (auto& row : lastOpValues) row.fill(-1);
    lastGlobalValues.fill(-1);
}

void ParameterManager::cacheParameterHandles()
{
    if (!parametersPtr) return;
    for (int op = 1; op <= 4; ++op) {
        auto& row = opParamHandles[static_cast<size_t>(op - 1)];
        row[OP_TL]     = parametersPtr->getParameter(ParamID::Op::tl(op));
        row[OP_AR]     = parametersPtr->getParameter(ParamID::Op::ar(op));
        row[OP_D1R]    = parametersPtr->getParameter(ParamID::Op::d1r(op));
        row[OP_D1L]    = parametersPtr->getParameter(ParamID::Op::d1l(op));
        row[OP_D2R]    = parametersPtr->getParameter(ParamID::Op::d2r(op));
        row[OP_RR]     = parametersPtr->getParameter(ParamID::Op::rr(op));
        row[OP_KS]     = parametersPtr->getParameter(ParamID::Op::ks(op));
        row[OP_MUL]    = parametersPtr->getParameter(ParamID::Op::mul(op));
        row[OP_DT1]    = parametersPtr->getParameter(ParamID::Op::dt1(op));
        row[OP_DT2]    = parametersPtr->getParameter(ParamID::Op::dt2(op));
        row[OP_AMS_EN] = parametersPtr->getParameter(ParamID::Op::ams_en(op));
    }
    globalParamHandles[G_ALG]        = parametersPtr->getParameter(ParamID::Global::Algorithm);
    globalParamHandles[G_FB]         = parametersPtr->getParameter(ParamID::Global::Feedback);
    globalParamHandles[G_LFO_RATE]   = parametersPtr->getParameter(ParamID::Global::LfoRate);
    globalParamHandles[G_LFO_AMD]    = parametersPtr->getParameter(ParamID::Global::LfoAmd);
    globalParamHandles[G_LFO_PMD]    = parametersPtr->getParameter(ParamID::Global::LfoPmd);
    globalParamHandles[G_LFO_WF]     = parametersPtr->getParameter(ParamID::Global::LfoWaveform);
    globalParamHandles[G_NOISE_EN]   = parametersPtr->getParameter(ParamID::Global::NoiseEnable);
    globalParamHandles[G_NOISE_FREQ] = parametersPtr->getParameter(ParamID::Global::NoiseFrequency);
}

void ParameterManager::writeOperatorParameterToAllChannels(int op, OpParam which, int value)
{
    using P = YmfmWrapperInterface::OperatorParameter;
    const auto v = static_cast<uint8_t>(value);
    for (uint8_t ch = 0; ch < 8; ++ch) {
        const auto opIndex = static_cast<uint8_t>(op);
        switch (which) {
            case OP_TL:     ymfmWrapper.setOperatorParameter(ch, opIndex, P::TotalLevel, v); break;
            case OP_AR:     ymfmWrapper.setOperatorParameter(ch, opIndex, P::AttackRate, v); break;
            case OP_D1R:    ymfmWrapper.setOperatorParameter(ch, opIndex, P::Decay1Rate, v); break;
            case OP_D1L:    ymfmWrapper.setOperatorParameter(ch, opIndex, P::SustainLevel, v); break;
            case OP_D2R:    ymfmWrapper.setOperatorParameter(ch, opIndex, P::Decay2Rate, v); break;
            case OP_RR:     ymfmWrapper.setOperatorParameter(ch, opIndex, P::ReleaseRate, v); break;
            case OP_KS:     ymfmWrapper.setOperatorParameter(ch, opIndex, P::KeyScale, v); break;
            case OP_MUL:    ymfmWrapper.setOperatorParameter(ch, opIndex, P::Multiple, v); break;
            case OP_DT1:    ymfmWrapper.setOperatorParameter(ch, opIndex, P::Detune1, v); break;
            case OP_DT2:    ymfmWrapper.setOperatorParameter(ch, opIndex, P::Detune2, v); break;
            case OP_AMS_EN: ymfmWrapper.setOperatorAmsEnable(ch, opIndex, value > 0); break;
            default: break;
        }
    }
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
    invalidateRegisterCache();
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

void ParameterManager::updateGlobalParameters()
{
    if (!parametersPtr) {
        return;
    }
    
    std::array<int, NumGlobalParams> current {};
    for (int i = 0; i < NumGlobalParams; ++i) {
        auto* param = globalParamHandles[static_cast<size_t>(i)];
        current[static_cast<size_t>(i)] = param ? static_cast<int>(registerValue(param)) : 0;
    }
    auto changed = [&](GlobalParam g) { return current[g] != lastGlobalValues[g]; };
    
    if (changed(G_ALG) || changed(G_FB)) {
        const auto algorithmValue = static_cast<uint8_t>(current[G_ALG]);
        const auto feedbackValue = static_cast<uint8_t>(current[G_FB]);
        CS_ASSERT_ALGORITHM(algorithmValue);
        CS_ASSERT_FEEDBACK(feedbackValue);
        for (uint8_t ch = 0; ch < 8; ++ch) {
            if (changed(G_ALG)) ymfmWrapper.setAlgorithm(ch, algorithmValue);
            if (changed(G_FB)) ymfmWrapper.setFeedback(ch, feedbackValue);
        }
    }
    if (changed(G_LFO_RATE) || changed(G_LFO_AMD) || changed(G_LFO_PMD) || changed(G_LFO_WF)) {
        ymfmWrapper.setLfoParameters(static_cast<uint8_t>(current[G_LFO_RATE]),
                                     static_cast<uint8_t>(current[G_LFO_AMD]),
                                     static_cast<uint8_t>(current[G_LFO_PMD]),
                                     static_cast<uint8_t>(current[G_LFO_WF]));
    }
    if (changed(G_NOISE_EN) || changed(G_NOISE_FREQ)) {
        ymfmWrapper.setNoiseParameters(current[G_NOISE_EN] > 0, static_cast<uint8_t>(current[G_NOISE_FREQ]));
    }
    lastGlobalValues = current;
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