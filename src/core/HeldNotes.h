#pragma once

#include <array>
#include <cstdint>

namespace ymulatorsynth {

/** Notes currently held in mono / arpeggio mode, newest last, and the one channel that sounds them. */
struct HeldNotes
{
    std::array<uint8_t, 16> notes {};
    int count = 0;
    int channel = -1;
    
    bool contains(uint8_t note) const { for (int i = 0; i < count; ++i) if (notes[static_cast<size_t>(i)] == note) return true; return false; }
    void add(uint8_t note)
    {
        remove(note);
        if (count < static_cast<int>(notes.size())) notes[static_cast<size_t>(count++)] = note;
    }
    void remove(uint8_t note)
    {
        for (int i = 0; i < count; ++i)
            if (notes[static_cast<size_t>(i)] == note) {
                for (int j = i; j + 1 < count; ++j) notes[static_cast<size_t>(j)] = notes[static_cast<size_t>(j + 1)];
                --count;
                return;
            }
    }
    uint8_t last() const { return count > 0 ? notes[static_cast<size_t>(count - 1)] : 0; }
    void clear() { count = 0; channel = -1; }
};

} // namespace ymulatorsynth
