#include "PatchWorkspace.h"
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"
#include <set>

namespace ymulatorsynth {

namespace {
// Playback settings that are not part of the sound
const std::set<std::string> kExcluded = { ParamID::Global::PitchBendRange, ParamID::Global::Expressive };
bool isSoundParameter(const juce::String& id)
{
    return kExcluded.count(id.toStdString()) == 0 && !id.endsWith(ParamID::Channel::Pan);
}
}

PatchWorkspace::PatchWorkspace(juce::AudioProcessorValueTreeState& params, MacroMapper& macroMapper, Callbacks cb)
    : parameters(params), mapper(macroMapper), callbacks(std::move(cb))
{
    for (const char* id : { ParamID::Macro::Brightness, ParamID::Macro::Harmonics, ParamID::Global::Feedback,
                            ParamID::Macro::Attack, ParamID::Macro::Decay, ParamID::Macro::Release, ParamID::Macro::Spread })
        if (auto* p = parameters.getParameter(id)) { toneParameters.push_back(p); p->addListener(this); }
}

PatchWorkspace::~PatchWorkspace()
{
    for (auto* p : toneParameters) p->removeListener(this);
}

void PatchWorkspace::parameterGestureChanged(int, bool gestureIsStarting)
{
    // A TONE knob is about to move: remember the sound before it does
    if (gestureIsStarting && !restoring) store.pushUndo(capture());
}

void PatchWorkspace::setEdited(bool edited)
{
    if (callbacks.setEdited) callbacks.setEdited(edited);
}

PatchSnapshot PatchWorkspace::capture() const
{
    PatchSnapshot snapshot;
    for (auto* param : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            if (isSoundParameter(ranged->paramID))
                snapshot.parameters.emplace_back(ranged->paramID.toStdString(), ranged->getValue());
    snapshot.anchor = mapper.getAnchor();
    snapshot.edited = callbacks.isEdited ? callbacks.isEdited() : true;
    return snapshot;
}

void PatchWorkspace::restore(const PatchSnapshot& snapshot)
{
    const juce::ScopedValueSetter<bool> guard(restoring, true);
    mapper.setSuspended(true);
    for (const auto& [id, value] : snapshot.parameters)
        if (auto* p = parameters.getParameter(id))
            if (p->getValue() != value) p->setValueNotifyingHost(value);
    mapper.setAnchor(snapshot.anchor);
    mapper.setSuspended(false);
    setEdited(snapshot.edited);
}

void PatchWorkspace::applyPatch(const GeneratedPatch& patch)
{
    const juce::ScopedValueSetter<bool> guard(restoring, true);
    auto set = [this](const std::string& id, int value) {
        if (auto* p = parameters.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
    };
    mapper.setSuspended(true);
    set(ParamID::Global::Algorithm, patch.algorithm);
    set(ParamID::Global::Feedback, patch.feedback);
    for (int op = 1; op <= 4; ++op) {
        const auto& o = patch.ops[static_cast<size_t>(op - 1)];
        set(ParamID::Op::tl(op), o.tl);
        set(ParamID::Op::ar(op), o.ar);
        set(ParamID::Op::d1r(op), o.d1r);
        set(ParamID::Op::d1l(op), o.d1l);
        set(ParamID::Op::d2r(op), o.d2r);
        set(ParamID::Op::rr(op), o.rr);
        set(ParamID::Op::ks(op), o.ks);
        set(ParamID::Op::mul(op), o.mul);
        set(ParamID::Op::dt1(op), o.dt1);
        set(ParamID::Op::dt2(op), o.dt2);
        set(ParamID::Op::ams_en(op), o.amsEnable ? 1 : 0);
        set(ParamID::Op::slot_en(op), 1);
    }
    set(ParamID::Global::LfoRate, patch.lfoRate);
    set(ParamID::Global::LfoAmd, patch.lfoAmd);
    set(ParamID::Global::LfoPmd, patch.lfoPmd);
    set(ParamID::Global::LfoWaveform, patch.lfoWaveform);
    set(ParamID::Global::LfoAms, patch.lfoAms);
    set(ParamID::Global::LfoPms, patch.lfoPms);
    set(ParamID::Global::NoiseEnable, patch.noiseEnable ? 1 : 0);
    set(ParamID::Global::NoiseFrequency, patch.noiseFrequency);
    mapper.setSuspended(false);
    mapper.captureAnchor();          // the new sound is the reference; macros go back to centre
    setEdited(true);
}

GeneratedPatch PatchWorkspace::generate(const GeneratorInput& input, juce::int64 seed)
{
    const PatchSnapshot before = capture();
    store.pushUndo(before);
    store.setSlot(Slot::A, before);
    
    const GeneratedPatch patch = PatchGenerator::generate(input, seed);
    applyPatch(patch);
    store.setSlot(Slot::B, capture());
    active = Slot::B;
    CS_DBG("PatchWorkspace: generated algorithm " + juce::String(patch.algorithm));
    return patch;
}

bool PatchWorkspace::undo()
{
    auto snapshot = store.popUndo();
    if (!snapshot.has_value()) return false;
    restore(*snapshot);
    return true;
}

void PatchWorkspace::selectSlot(Slot slot)
{
    if (slot == active) return;
    store.setSlot(active, capture());
    if (const auto& target = store.slot(slot); target.has_value()) restore(*target);
    active = slot;
}

} // namespace ymulatorsynth
