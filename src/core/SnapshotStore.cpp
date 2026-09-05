#include "SnapshotStore.h"

namespace ymulatorsynth {

void SnapshotStore::pushUndo(PatchSnapshot snapshot)
{
    if (undoStack.size() >= kMaxUndo) undoStack.erase(undoStack.begin());
    undoStack.push_back(std::move(snapshot));
}

std::optional<PatchSnapshot> SnapshotStore::popUndo()
{
    if (undoStack.empty()) return std::nullopt;
    PatchSnapshot top = std::move(undoStack.back());
    undoStack.pop_back();
    return top;
}

} // namespace ymulatorsynth
