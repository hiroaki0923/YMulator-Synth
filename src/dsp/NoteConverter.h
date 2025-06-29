#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>

namespace ymulatorsynth {

/**
 * @brief Converts MIDI note numbers to YM2151/YM2608 frequency values
 * 
 * This class handles the conversion from MIDI note numbers to the frequency
 * number (FNUM) values used by Yamaha FM chips. Extracted from YmfmWrapper
 * as part of Phase 3 Enhanced Abstraction refactoring.
 */
class NoteConverter {
public:
    NoteConverter() = default;
    ~NoteConverter() = default;
    
    /**
     * @brief Convert MIDI note to frequency number (FNUM)
     * @param note MIDI note number (0-127)
     * @return 16-bit frequency number for YM2151/YM2608
     */
    uint16_t noteToFnum(uint8_t note);
    
    /**
     * @brief Convert MIDI note to FNUM with pitch bend applied
     * @param note MIDI note number (0-127)
     * @param pitchBendSemitones Pitch bend amount in semitones
     * @return 16-bit frequency number with pitch bend applied
     */
    uint16_t noteToFnumWithPitchBend(uint8_t note, float pitchBendSemitones);
    
private:
    // Frequency calculation constants
    static constexpr float FNUM_BASE = 256.0f;
    static constexpr float SEMITONE_RATIO = 1.05946309436f; // 2^(1/12)
    static constexpr float A4_FREQUENCY = 440.0f;
    static constexpr uint8_t A4_NOTE = 69;
    
    /**
     * @brief Calculate base frequency for a MIDI note
     * @param note MIDI note number
     * @return Frequency in Hz
     */
    float calculateNoteFrequency(uint8_t note) const;
    
    /**
     * @brief Convert frequency to YM2151 FNUM value
     * @param frequency Frequency in Hz
     * @return 16-bit FNUM value
     */
    uint16_t frequencyToFnum(float frequency) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteConverter)
};

} // namespace ymulatorsynth