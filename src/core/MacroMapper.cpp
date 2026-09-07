#include "MacroMapper.h"
#include "../dsp/AlgorithmInfo.h"
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"

namespace ymulatorsynth {

namespace {

// Mapping strengths from the design spec (section 2.2)
constexpr int kBrightnessTlSteps = 40;      // about 30 dB at full swing
constexpr int kBrightnessFeedbackSteps = 7; // used when the algorithm has no modulator
constexpr int kAttackSteps = 12;
constexpr int kDecayD1rSteps = 10;
constexpr int kDecayD2rSteps = 6;
constexpr int kReleaseSteps = 6;
constexpr int kSpreadSteps = 3;
constexpr int kTlMax = 127, kRateMax = 31, kRrMax = 15, kMulMax = 15, kFeedbackMax = 7, kDetuneMagMax = 3;
constexpr int kLastAlgorithm = 7;

constexpr float harmonicsRatio(HarmonicsTemplate t)
{
    switch (t) {
        case HarmonicsTemplate::Saw:    return 1.0f;
        case HarmonicsTemplate::Square: return 2.0f;
        case HarmonicsTemplate::Pulse:  return 3.0f;
        case HarmonicsTemplate::Bright: return 4.0f;
        case HarmonicsTemplate::Bell:   return 3.5f;
        case HarmonicsTemplate::Metal:  return 5.5f;
        default:                        return 0.0f;
    }
}

int scaled(float m, int steps) { return juce::roundToInt(m * static_cast<float>(steps)); }

const char* const kAnchorFields[] = { "tl", "ar", "d1r", "d2r", "rr", "dt1", "mul" };

std::array<int, 4>& fieldOf(RawPatch& patch, int field)
{
    switch (field) {
        case 0: return patch.tl;
        case 1: return patch.ar;
        case 2: return patch.d1r;
        case 3: return patch.d2r;
        case 4: return patch.rr;
        case 5: return patch.dt1;
        default: return patch.mul;
    }
}

} // namespace

const juce::Identifier MacroMapper::anchorNodeType("macroAnchor");

// ============================================================================
// Value types
// ============================================================================

bool RawPatch::operator==(const RawPatch& o) const
{
    return tl == o.tl && ar == o.ar && d1r == o.d1r && d2r == o.d2r && rr == o.rr
        && dt1 == o.dt1 && mul == o.mul && feedback == o.feedback;
}

bool MacroValues::isCentred() const
{
    return brightness == 0.0f && attack == 0.0f && decay == 0.0f && release == 0.0f && spread == 0.0f
        && harmonics == HarmonicsTemplate::Preset;
}

// ============================================================================
// Pure mapping
// ============================================================================

int MacroMapper::encodeDetune1(int signedDetune)
{
    // DT1 register: 0..3 = 0,+1,+2,+3 and 4..7 = 0,-1,-2,-3
    const int mag = juce::jlimit(0, kDetuneMagMax, std::abs(signedDetune));
    return (signedDetune < 0 && mag > 0) ? 4 + mag : mag;
}

int MacroMapper::decodeDetune1(int registerValue)
{
    const int mag = registerValue & 3;
    return registerValue >= 4 ? -mag : mag;
}

RawPatch MacroMapper::apply(const RawPatch& anchor, const MacroValues& macros, int algorithm)
{
    const auto& info = algorithmInfo(algorithm);
    RawPatch out = anchor;
    
    // Brightness: modulator level, or feedback when there is nothing to modulate with
    if (info.modulatorMask() == 0) {
        out.feedback = juce::jlimit(0, kFeedbackMax, anchor.feedback + scaled(macros.brightness, kBrightnessFeedbackSteps));
    } else {
        for (int op = 0; op < 4; ++op)
            if (info.isModulator(op))
                out.tl[static_cast<size_t>(op)] = juce::jlimit(0, kTlMax, anchor.tl[static_cast<size_t>(op)] - scaled(macros.brightness, kBrightnessTlSteps));
    }
    
    // Harmonics: modulator ratios relative to the operator they feed
    switch (macros.harmonics) {
        case HarmonicsTemplate::Preset:
            break;
        case HarmonicsTemplate::Sub:
            for (int op = 0; op < 4; ++op)
                if (info.isCarrier(op))
                    out.mul[static_cast<size_t>(op)] = anchor.mul[static_cast<size_t>(op)] / 2;
            break;
        case HarmonicsTemplate::Octave:
            for (auto& m : out.mul) m = juce::jlimit(0, kMulMax, m == 0 ? 1 : m * 2);
            break;
        default: {
            const float r = harmonicsRatio(macros.harmonics);
            // Edges always point to a higher operator, so walking down uses the updated target
            for (int op = 3; op >= 0; --op) {
                const int target = info.targetOf(op);
                if (target < 0) continue;
                const int targetMul = out.mul[static_cast<size_t>(target)];
                const float base = targetMul == 0 ? 0.5f : static_cast<float>(targetMul);
                out.mul[static_cast<size_t>(op)] = juce::jlimit(0, kMulMax, juce::roundToInt(base * r));
            }
            break;
        }
    }
    
    // Envelope macros: the loudness envelope, so carriers only; positive = slower, like an ADSR time
    // control. Modulator envelopes are the timbre envelope and stay as the patch has them
    for (int op = 0; op < 4; ++op) {
        if (!info.isCarrier(op)) continue;
        const auto i = static_cast<size_t>(op);
        out.ar[i] = juce::jlimit(0, kRateMax, anchor.ar[i] - scaled(macros.attack, kAttackSteps));
        out.d1r[i] = juce::jlimit(0, kRateMax, anchor.d1r[i] - scaled(macros.decay, kDecayD1rSteps));
        out.d2r[i] = juce::jlimit(0, kRateMax, anchor.d2r[i] - scaled(macros.decay, kDecayD2rSteps));
        out.rr[i] = juce::jlimit(0, kRrMax, anchor.rr[i] - scaled(macros.release, kReleaseSteps));
    }
    
    // Spread: detune magnitude on operators 1-3; operator 4 stays as the pitch reference
    static constexpr int kDefaultSign[3] = { +1, -1, +1 };
    for (int op = 0; op < 3; ++op) {
        const auto i = static_cast<size_t>(op);
        const int anchorDetune = decodeDetune1(anchor.dt1[i]);
        const int sign = anchorDetune == 0 ? kDefaultSign[op] : (anchorDetune < 0 ? -1 : +1);
        const int mag = juce::jlimit(0, kDetuneMagMax, std::abs(anchorDetune) + scaled(macros.spread, kSpreadSteps));
        out.dt1[i] = encodeDetune1(sign * mag);
    }
    
    return out;
}

std::vector<std::string> MacroMapper::targetsOf(Macro macro, int algorithm)
{
    const auto& info = algorithmInfo(algorithm);
    std::vector<std::string> ids;
    const bool noModulators = info.modulatorMask() == 0;
    
    switch (macro) {
        case Macro::Brightness:
            if (noModulators) ids.push_back(ParamID::Global::Feedback);
            else for (int op = 1; op <= 4; ++op) if (info.isModulator(op - 1)) ids.push_back(ParamID::Op::tl(op));
            break;
        case Macro::Harmonics:
            for (int op = 1; op <= 4; ++op) if (noModulators || info.isModulator(op - 1)) ids.push_back(ParamID::Op::mul(op));
            break;
        case Macro::Attack:
            for (int op = 1; op <= 4; ++op) if (info.isCarrier(op - 1)) ids.push_back(ParamID::Op::ar(op));
            break;
        case Macro::Decay:
            for (int op = 1; op <= 4; ++op) if (info.isCarrier(op - 1)) { ids.push_back(ParamID::Op::d1r(op)); ids.push_back(ParamID::Op::d2r(op)); }
            break;
        case Macro::Release:
            for (int op = 1; op <= 4; ++op) if (info.isCarrier(op - 1)) ids.push_back(ParamID::Op::rr(op));
            break;
        case Macro::Spread:
            for (int op = 1; op <= 3; ++op) ids.push_back(ParamID::Op::dt1(op));
            break;
    }
    return ids;
}

// ============================================================================
// Instance: anchor bookkeeping against the parameter tree
// ============================================================================

MacroMapper::MacroMapper(juce::AudioProcessorValueTreeState& params)
    : parameters(params)
{
    for (const char* id : { ParamID::Macro::Brightness, ParamID::Macro::Harmonics, ParamID::Macro::Attack,
                            ParamID::Macro::Decay, ParamID::Macro::Release, ParamID::Macro::Spread,
                            ParamID::Global::Feedback })
        listenedIds.emplace_back(id);
    for (int op = 1; op <= 4; ++op)
        for (const auto& id : { ParamID::Op::tl(op), ParamID::Op::ar(op), ParamID::Op::d1r(op), ParamID::Op::d2r(op),
                                ParamID::Op::rr(op), ParamID::Op::dt1(op), ParamID::Op::mul(op) })
            listenedIds.emplace_back(id);
    
    for (const auto& id : listenedIds)
        parameters.addParameterListener(id, this);
    
    anchor = lastRaw = currentRaw();
}

MacroMapper::~MacroMapper()
{
    for (const auto& id : listenedIds)
        parameters.removeParameterListener(id, this);
}

int MacroMapper::readInt(const std::string& id) const
{
    auto* param = parameters.getParameter(id);
    return param ? juce::roundToInt(param->convertFrom0to1(param->getValue())) : 0;
}

void MacroMapper::writeRaw(const std::string& id, int value)
{
    auto* param = parameters.getParameter(id);
    if (param == nullptr) return;
    const float normalised = param->convertTo0to1(static_cast<float>(value));
    if (param->getValue() != normalised)
        param->setValueNotifyingHost(normalised);
}

RawPatch MacroMapper::currentRaw() const
{
    RawPatch raw;
    for (int op = 1; op <= 4; ++op) {
        const auto i = static_cast<size_t>(op - 1);
        raw.tl[i] = readInt(ParamID::Op::tl(op));
        raw.ar[i] = readInt(ParamID::Op::ar(op));
        raw.d1r[i] = readInt(ParamID::Op::d1r(op));
        raw.d2r[i] = readInt(ParamID::Op::d2r(op));
        raw.rr[i] = readInt(ParamID::Op::rr(op));
        raw.dt1[i] = readInt(ParamID::Op::dt1(op));
        raw.mul[i] = readInt(ParamID::Op::mul(op));
    }
    raw.feedback = readInt(ParamID::Global::Feedback);
    return raw;
}

MacroValues MacroMapper::currentMacros() const
{
    auto macro = [this](const char* id) {
        auto* param = parameters.getParameter(id);
        return param ? param->convertFrom0to1(param->getValue()) / kDisplayRange : 0.0f;
    };
    MacroValues m;
    m.brightness = macro(ParamID::Macro::Brightness);
    m.attack = macro(ParamID::Macro::Attack);
    m.decay = macro(ParamID::Macro::Decay);
    m.release = macro(ParamID::Macro::Release);
    m.spread = macro(ParamID::Macro::Spread);
    m.harmonics = static_cast<HarmonicsTemplate>(juce::jlimit(0, static_cast<int>(HarmonicsTemplate::Count) - 1,
                                                              readInt(ParamID::Macro::Harmonics)));
    return m;
}

int MacroMapper::currentAlgorithm() const
{
    return juce::jlimit(0, kLastAlgorithm, readInt(ParamID::Global::Algorithm));
}

bool MacroMapper::isEdited() const
{
    return currentRaw() != anchor || !currentMacros().isCentred();
}

void MacroMapper::captureAnchor()
{
    anchor = lastRaw = currentRaw();
    
    const juce::ScopedValueSetter<bool> guard(applying, true);
    for (const char* id : { ParamID::Macro::Brightness, ParamID::Macro::Attack, ParamID::Macro::Decay,
                            ParamID::Macro::Release, ParamID::Macro::Spread })
        writeRaw(id, 0);
    writeRaw(ParamID::Macro::Harmonics, static_cast<int>(HarmonicsTemplate::Preset));
    CS_DBG("MacroMapper: anchor captured");
}

void MacroMapper::setAnchor(const RawPatch& newAnchor)
{
    anchor = newAnchor;
    lastRaw = currentRaw();
}

void MacroMapper::writeAnchorTo(juce::ValueTree& state) const
{
    state.removeChild(state.getChildWithName(anchorNodeType), nullptr);
    juce::ValueTree node(anchorNodeType);
    RawPatch copy = anchor;
    for (int field = 0; field < 7; ++field)
        for (int op = 0; op < 4; ++op)
            node.setProperty(juce::String(kAnchorFields[field]) + juce::String(op + 1),
                             fieldOf(copy, field)[static_cast<size_t>(op)], nullptr);
    node.setProperty("feedback", anchor.feedback, nullptr);
    state.appendChild(node, nullptr);
}

bool MacroMapper::restoreFromState(const juce::ValueTree& state)
{
    auto node = state.getChildWithName(anchorNodeType);
    if (!node.isValid()) {
        // Older state without a macro layer: whatever is loaded becomes the reference
        captureAnchor();
        return false;
    }
    RawPatch restored;
    for (int field = 0; field < 7; ++field)
        for (int op = 0; op < 4; ++op)
            fieldOf(restored, field)[static_cast<size_t>(op)] =
                static_cast<int>(node.getProperty(juce::String(kAnchorFields[field]) + juce::String(op + 1), 0));
    restored.feedback = static_cast<int>(node.getProperty("feedback", 0));
    anchor = restored;
    lastRaw = currentRaw();
    return true;
}

void MacroMapper::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (applying || suspended) return;
    
    // An algorithm change is not remapped on its own: the raw knobs stay where
    // they are and the new role set takes effect on the next macro move.
    if (parameterID.startsWith("macro_")) {
        applyMacros();
        return;
    }
    rebase(parameterID, juce::roundToInt(newValue));
}

void MacroMapper::applyMacros()
{
    const juce::ScopedValueSetter<bool> guard(applying, true);
    const RawPatch mapped = apply(anchor, currentMacros(), currentAlgorithm());
    
    for (int op = 1; op <= 4; ++op) {
        const auto i = static_cast<size_t>(op - 1);
        writeRaw(ParamID::Op::tl(op), mapped.tl[i]);
        writeRaw(ParamID::Op::ar(op), mapped.ar[i]);
        writeRaw(ParamID::Op::d1r(op), mapped.d1r[i]);
        writeRaw(ParamID::Op::d2r(op), mapped.d2r[i]);
        writeRaw(ParamID::Op::rr(op), mapped.rr[i]);
        writeRaw(ParamID::Op::dt1(op), mapped.dt1[i]);
        writeRaw(ParamID::Op::mul(op), mapped.mul[i]);
    }
    writeRaw(ParamID::Global::Feedback, mapped.feedback);
    lastRaw = mapped;
}

void MacroMapper::rebase(const juce::String& parameterID, int newValue)
{
    // A direct edit keeps the macro position and moves the anchor by the same amount
    const std::string id = parameterID.toStdString();
    int* anchorValue = nullptr;
    int* lastValue = nullptr;
    int maxValue = 0;
    
    if (id == ParamID::Global::Feedback) {
        anchorValue = &anchor.feedback; lastValue = &lastRaw.feedback; maxValue = kFeedbackMax;
    } else {
        for (int op = 1; op <= 4 && anchorValue == nullptr; ++op) {
            const auto i = static_cast<size_t>(op - 1);
            if (id == ParamID::Op::tl(op))       { anchorValue = &anchor.tl[i];  lastValue = &lastRaw.tl[i];  maxValue = kTlMax; }
            else if (id == ParamID::Op::ar(op))  { anchorValue = &anchor.ar[i];  lastValue = &lastRaw.ar[i];  maxValue = kRateMax; }
            else if (id == ParamID::Op::d1r(op)) { anchorValue = &anchor.d1r[i]; lastValue = &lastRaw.d1r[i]; maxValue = kRateMax; }
            else if (id == ParamID::Op::d2r(op)) { anchorValue = &anchor.d2r[i]; lastValue = &lastRaw.d2r[i]; maxValue = kRateMax; }
            else if (id == ParamID::Op::rr(op))  { anchorValue = &anchor.rr[i];  lastValue = &lastRaw.rr[i];  maxValue = kRrMax; }
            else if (id == ParamID::Op::mul(op)) { anchorValue = &anchor.mul[i]; lastValue = &lastRaw.mul[i]; maxValue = kMulMax; }
            else if (id == ParamID::Op::dt1(op)) {
                // Detune is re-based on its signed value so the sign convention survives
                const int delta = decodeDetune1(newValue) - decodeDetune1(lastRaw.dt1[i]);
                anchor.dt1[i] = encodeDetune1(juce::jlimit(-kDetuneMagMax, kDetuneMagMax, decodeDetune1(anchor.dt1[i]) + delta));
                lastRaw.dt1[i] = newValue;
                return;
            }
        }
    }
    if (anchorValue == nullptr) return;
    
    *anchorValue = juce::jlimit(0, maxValue, *anchorValue + (newValue - *lastValue));
    *lastValue = newValue;
}

} // namespace ymulatorsynth
