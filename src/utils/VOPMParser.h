#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace chipsynth {

/**
 * VOPM Voice data structure representing a complete YM2151 voice configuration
 */
struct VOPMVoice
{
    int number = 0;
    juce::String name;
    
    // LFO parameters
    struct LFO
    {
        int frequency = 0;    // LFRQ (0-255)
        int amd = 0;         // AMD (0-127)
        int pmd = 0;         // PMD (0-127)
        int waveform = 0;    // WF (0-3)
        int noiseFreq = 0;   // NFRQ (0-31)
    } lfo;
    
    // Channel parameters
    struct Channel
    {
        int pan = 3;         // PAN (0-3: off, right, left, center)
        int feedback = 0;    // FL (0-7)
        int algorithm = 0;   // CON (0-7)
        int ams = 0;         // AMS (0-3)
        int pms = 0;         // PMS (0-7)
        int slotMask = 15;   // SLOT (0-15)
        int noiseEnable = 0; // NE (0-1)
    } channel;
    
    // Operator parameters (4 operators)
    struct Operator
    {
        int attackRate = 31;     // AR (0-31)
        int decay1Rate = 0;      // D1R (0-31)
        int decay2Rate = 0;      // D2R (0-31)
        int releaseRate = 7;     // RR (0-15)
        int decay1Level = 0;     // D1L (0-15)
        int totalLevel = 0;      // TL (0-127)
        int keyScale = 0;        // KS (0-3)
        int multiple = 1;        // MUL (0-15)
        int detune1 = 3;         // DT1 (0-7)
        int detune2 = 0;         // DT2 (0-3)
        int amsEnable = 0;       // AMS-EN (0-1)
    } operators[4];
};

/**
 * Validation result for VOPM voices
 */
struct ValidationResult
{
    bool isValid = true;
    juce::StringArray errors;
    juce::StringArray warnings;
};

/**
 * VOPM file parser for loading .opm voice files
 */
class VOPMParser
{
public:
    /**
     * Parse a VOPM file from disk
     * @param file The .opm file to parse
     * @return Vector of parsed voices
     */
    static std::vector<VOPMVoice> parseFile(const juce::File& file);
    
    /**
     * Parse VOPM content from string
     * @param content The raw VOPM file content
     * @return Vector of parsed voices
     */
    static std::vector<VOPMVoice> parseContent(const juce::String& content);
    
    /**
     * Validate a VOPM voice
     * @param voice The voice to validate
     * @return Validation result with errors and warnings
     */
    static ValidationResult validate(const VOPMVoice& voice);
    
    /**
     * Convert VOPM voice to string representation
     * @param voice The voice to serialize
     * @return String representation in VOPM format
     */
    static juce::String voiceToString(const VOPMVoice& voice);
    
    /**
     * Convert OPM format values to internal representation
     */
    static int convertOpmPanToInternal(int opmPan);
    static int convertOpmAmeToInternal(int opmAme);
    
    /**
     * Convert internal values to OPM format
     */
    static int convertInternalPanToOpm(int internalPan);
    static int convertInternalAmeToOpm(int internalAme);

private:
    static void parseVoiceHeader(const juce::String& line, VOPMVoice& voice);
    static void parseLFO(const juce::String& line, VOPMVoice::LFO& lfo);
    static void parseChannel(const juce::String& line, VOPMVoice::Channel& channel);
    static void parseOperator(const juce::String& line, VOPMVoice::Operator& op);
    
    static juce::StringArray tokenizeLine(const juce::String& line);
    static bool isCommentLine(const juce::String& line);
    static bool isValidRange(int value, int min, int max);
};

} // namespace chipsynth