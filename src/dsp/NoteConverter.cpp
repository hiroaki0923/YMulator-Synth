#include "NoteConverter.h"
#include "YM2151Registers.h"
#include "../utils/Debug.h"
#include <cmath>

using namespace ymulatorsynth;

uint16_t NoteConverter::noteToFnum(uint8_t note)
{
    CS_ASSERT_NOTE(note);
    
    float frequency = calculateNoteFrequency(note);
    uint16_t fnum = frequencyToFnum(frequency);
    
    CS_DBG("NoteConverter::noteToFnum - note: " + juce::String(note) + 
           ", freq: " + juce::String(frequency, 2) + "Hz, fnum: 0x" + 
           juce::String::toHexString(fnum));
    
    return fnum;
}

uint16_t NoteConverter::noteToFnumWithPitchBend(uint8_t note, float pitchBendSemitones)
{
    CS_ASSERT_NOTE(note);
    CS_ASSERT_PARAMETER_RANGE(std::abs(pitchBendSemitones), 0.0f, 12.0f);
    
    float baseFrequency = calculateNoteFrequency(note);
    
    // Apply pitch bend: frequency * 2^(semitones/12)
    float bendRatio = std::pow(2.0f, pitchBendSemitones / 12.0f);
    float bentFrequency = baseFrequency * bendRatio;
    
    uint16_t fnum = frequencyToFnum(bentFrequency);
    
    CS_DBG("NoteConverter::noteToFnumWithPitchBend - note: " + juce::String(note) + 
           ", bend: " + juce::String(pitchBendSemitones, 3) + " semitones" +
           ", bent freq: " + juce::String(bentFrequency, 2) + "Hz, fnum: 0x" + 
           juce::String::toHexString(fnum));
    
    return fnum;
}

float NoteConverter::calculateNoteFrequency(uint8_t note) const
{
    // Calculate frequency using A4 = 440Hz as reference
    // frequency = 440 * 2^((note - 69) / 12)
    float semitonesFromA4 = static_cast<float>(note) - static_cast<float>(A4_NOTE);
    float frequency = A4_FREQUENCY * std::pow(2.0f, semitonesFromA4 / 12.0f);
    
    return frequency;
}

uint16_t NoteConverter::frequencyToFnum(float frequency) const
{
    // YM2151 FNUM calculation based on chip clock and frequency
    // This is a simplified calculation - actual implementation may need
    // more precise clock-based calculations
    
    // Clamp frequency to reasonable range
    frequency = std::max(20.0f, std::min(frequency, 20000.0f));
    
    // Convert to 16-bit FNUM value
    // This uses a simplified linear mapping - real hardware uses more complex calculation
    uint32_t fnum32 = static_cast<uint32_t>(frequency * FNUM_BASE / A4_FREQUENCY * 0x200);
    
    // Clamp to 16-bit range
    uint16_t fnum = static_cast<uint16_t>(std::min(fnum32, 0xFFFFu));
    
    return fnum;
}