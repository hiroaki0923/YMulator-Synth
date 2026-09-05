#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "MacroMapper.h"
#include "PatchGenerator.h"
#include "SnapshotStore.h"

namespace ymulatorsynth {

/**
 * Applies generated patches and snapshots to the parameter tree, keeps the
 * undo stack and the A/B slots, and re-anchors the macros afterwards.
 * Undo points are taken before a generation and before a TONE knob gesture.
 * Message thread only.
 */
class PatchWorkspace : private juce::AudioProcessorParameter::Listener
{
public:
    using Slot = SnapshotStore::Slot;
    
    struct Callbacks {
        std::function<bool()> isEdited;              // custom-mode flag of the processor
        std::function<void(bool)> setEdited;
    };
    
    PatchWorkspace(juce::AudioProcessorValueTreeState& parameters, MacroMapper& mapper, Callbacks callbacks);
    ~PatchWorkspace() override;
    
    /** Keeps the current sound in slot A, applies a new one as slot B. */
    GeneratedPatch generate(const GeneratorInput& input, juce::int64 seed);
    void applyPatch(const GeneratedPatch& patch);
    
    bool canUndo() const { return store.canUndo(); }
    bool undo();
    
    void selectSlot(Slot slot);
    Slot activeSlot() const { return active; }
    bool hasSlot(Slot slot) const { return store.slot(slot).has_value(); }
    
    PatchSnapshot capture() const;
    void restore(const PatchSnapshot& snapshot);
    
private:
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    void setEdited(bool edited);
    
    juce::AudioProcessorValueTreeState& parameters;
    MacroMapper& mapper;
    Callbacks callbacks;
    SnapshotStore store;
    Slot active = Slot::A;
    std::vector<juce::RangedAudioParameter*> toneParameters;
    bool restoring = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchWorkspace)
};

} // namespace ymulatorsynth
