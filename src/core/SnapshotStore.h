#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "MacroMapper.h"

namespace ymulatorsynth {

/** Everything needed to bring a sound back: parameter values, the macro anchor and the edited flag. */
struct PatchSnapshot
{
    std::vector<std::pair<std::string, float>> parameters;   // id -> normalised value
    RawPatch anchor;
    bool edited = false;
    
    bool operator==(const PatchSnapshot& other) const
    {
        return parameters == other.parameters && anchor == other.anchor && edited == other.edited;
    }
};

/** Undo stack (depth 16) and the two compare slots. Pure data; PatchWorkspace fills and applies it. */
class SnapshotStore
{
public:
    enum class Slot { A, B };
    static constexpr size_t kMaxUndo = 16;
    
    void pushUndo(PatchSnapshot snapshot);
    std::optional<PatchSnapshot> popUndo();
    bool canUndo() const { return !undoStack.empty(); }
    size_t undoDepth() const { return undoStack.size(); }
    
    void setSlot(Slot slot, PatchSnapshot snapshot) { slots[index(slot)] = std::move(snapshot); }
    const std::optional<PatchSnapshot>& slot(Slot slot) const { return slots[index(slot)]; }
    
private:
    static size_t index(Slot slot) { return slot == Slot::A ? 0 : 1; }
    std::vector<PatchSnapshot> undoStack;
    std::optional<PatchSnapshot> slots[2];
};

} // namespace ymulatorsynth
