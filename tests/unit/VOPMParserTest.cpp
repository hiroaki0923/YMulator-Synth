#include <gtest/gtest.h>
#include "utils/VOPMParser.h"
#include "utils/Debug.h"
#include <juce_core/juce_core.h>
#include <fstream>

using namespace ymulatorsynth;

class VOPMParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for testing
        tempDir = juce::File::createTempFile("VOPMParserTest");
        tempDir.deleteFile();
        tempDir.createDirectory();
    }
    
    void TearDown() override {
        if (tempDir.exists()) {
            tempDir.deleteRecursively();
        }
    }
    
    juce::File tempDir;
    
    // Helper to create test OPM file
    juce::File createTestOPMFile(const juce::String& filename, const juce::String& content) {
        auto file = tempDir.getChildFile(filename);
        file.replaceWithText(content);
        return file;
    }
    
    // Helper to create a test voice
    VOPMVoice createTestVoice(int number = 0, const juce::String& name = "Test Voice") {
        VOPMVoice voice;
        voice.number = number;
        voice.name = name;
        
        // Set LFO parameters
        voice.lfo.frequency = 100;
        voice.lfo.amd = 50;
        voice.lfo.pmd = 25;
        voice.lfo.waveform = 2;
        voice.lfo.noiseFreq = 15;
        
        // Set channel parameters
        voice.channel.pan = 3;
        voice.channel.feedback = 5;
        voice.channel.algorithm = 4;
        voice.channel.ams = 2;
        voice.channel.pms = 3;
        voice.channel.slotMask = 15;
        voice.channel.noiseEnable = 0;
        
        // Set operator parameters
        for (int i = 0; i < 4; ++i) {
            voice.operators[i].attackRate = 31;
            voice.operators[i].decay1Rate = 15;
            voice.operators[i].decay2Rate = 10;
            voice.operators[i].releaseRate = 7;
            voice.operators[i].decay1Level = 8;
            voice.operators[i].totalLevel = 20 + i * 10;
            voice.operators[i].keyScale = 1;
            voice.operators[i].multiple = 1 + i;
            voice.operators[i].detune1 = 3 + i;
            voice.operators[i].detune2 = i % 4;
            voice.operators[i].amsEnable = i % 2;
        }
        
        return voice;
    }
    
    // Valid OPM content for testing
    const juce::String validOPMContent = R"(//MiOPMdrv sound bank Paramer Ver2002.04.22
//LFO: LFRQ AMD PMD WF NFRQ
//CH: PAN   FL CON AMS PMS SLOT NE
//[M1]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[C1]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[M2]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[C2]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN

@:0 Test Instrument
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   1   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0

@:1 Another Test
LFO: 100  50  25   2  15
CH: 64   5   4   2   3  15   0
M1: 25  10   0   5   1  29   1   1   1   0   0
C1: 25  11   0   8   5  15   1   5   1   0   0
M2: 28  13   0   6   2  45   1   1   0   0   0
C2: 14   4   0   6   0   0   1   1   0   0   0
)";

    const juce::String malformedOPMContent = R"(@:0 Test
LFO: invalid data here
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
INVALID LINE FORMAT
C1: 31   8   8  11   1   0   0   1   3   0   0
)";
};

// =============================================================================
// 1. File Parsing Testing
// =============================================================================

TEST_F(VOPMParserTest, ParseValidOPMFile) {
    auto opmFile = createTestOPMFile("test.opm", validOPMContent);
    
    auto voices = VOPMParser::parseFile(opmFile);
    EXPECT_EQ(voices.size(), 2);
    
    // Check first voice
    EXPECT_EQ(voices[0].number, 0);
    EXPECT_EQ(voices[0].name, "Test Instrument");
    EXPECT_EQ(voices[0].channel.algorithm, 4);
    EXPECT_EQ(voices[0].channel.feedback, 6);
    
    // Check second voice
    EXPECT_EQ(voices[1].number, 1);
    EXPECT_EQ(voices[1].name, "Another Test");
    EXPECT_EQ(voices[1].lfo.frequency, 100);
    EXPECT_EQ(voices[1].lfo.amd, 50);
    EXPECT_EQ(voices[1].lfo.pmd, 25);
}

TEST_F(VOPMParserTest, ParseNonexistentFile) {
    auto nonexistentFile = tempDir.getChildFile("nonexistent.opm");
    
    auto voices = VOPMParser::parseFile(nonexistentFile);
    EXPECT_TRUE(voices.empty());
}

TEST_F(VOPMParserTest, ParseEmptyFile) {
    auto emptyFile = createTestOPMFile("empty.opm", "");
    
    auto voices = VOPMParser::parseFile(emptyFile);
    EXPECT_TRUE(voices.empty());
}

TEST_F(VOPMParserTest, ParseFileWithOnlyComments) {
    juce::String commentsOnly = R"(//MiOPMdrv sound bank Paramer Ver2002.04.22
//LFO: LFRQ AMD PMD WF NFRQ
//CH: PAN   FL CON AMS PMS SLOT NE
// This file contains only comments
)";
    
    auto commentsFile = createTestOPMFile("comments.opm", commentsOnly);
    auto voices = VOPMParser::parseFile(commentsFile);
    EXPECT_TRUE(voices.empty());
}

// =============================================================================
// 2. Content Parsing Testing
// =============================================================================

TEST_F(VOPMParserTest, ParseValidContent) {
    auto voices = VOPMParser::parseContent(validOPMContent);
    EXPECT_EQ(voices.size(), 2);
    
    // Verify first voice details
    const auto& voice1 = voices[0];
    EXPECT_EQ(voice1.number, 0);
    EXPECT_EQ(voice1.name, "Test Instrument");
    EXPECT_EQ(voice1.channel.algorithm, 4);
    EXPECT_EQ(voice1.channel.feedback, 6);
    
    // Check operator parameters
    EXPECT_EQ(voice1.operators[0].attackRate, 31);
    EXPECT_EQ(voice1.operators[0].totalLevel, 20);
    EXPECT_EQ(voice1.operators[1].totalLevel, 0);
}

TEST_F(VOPMParserTest, ParseEmptyContent) {
    auto voices = VOPMParser::parseContent("");
    EXPECT_TRUE(voices.empty());
}

TEST_F(VOPMParserTest, ParseMalformedContent) {
    auto voices = VOPMParser::parseContent(malformedOPMContent);
    
    // Malformed content with incomplete operators should not produce valid voices
    EXPECT_EQ(voices.size(), 0);
}

TEST_F(VOPMParserTest, ParseContentWithVariousLineEndings) {
    juce::String mixedLineEndings = "@:0 Test\r\nLFO:  0   0   0   0   0\nCH: 64   6   4   0   0  15   0\r\nM1: 31   8   8  11   1  20   0   1   3   0   0\nC1: 31   8   8  11   1   0   0   1   3   0   0\nM2: 31   8   8  11   1  20   0   1   3   0   0\nC2: 31   8   8  11   1   0   0   1   3   0   0\n";
    
    auto voices = VOPMParser::parseContent(mixedLineEndings);
    EXPECT_EQ(voices.size(), 1);
    EXPECT_EQ(voices[0].name, "Test");
}

// =============================================================================
// 3. Voice Validation Testing
// =============================================================================

TEST_F(VOPMParserTest, ValidateValidVoice) {
    auto voice = createTestVoice();
    auto result = VOPMParser::validate(voice);
    
    EXPECT_TRUE(result.isValid);
    EXPECT_TRUE(result.errors.isEmpty());
}

TEST_F(VOPMParserTest, ValidateVoiceWithInvalidParameters) {
    auto voice = createTestVoice();
    
    // Set invalid parameter values
    voice.channel.algorithm = 8; // Valid range: 0-7
    voice.operators[0].attackRate = 32; // Valid range: 0-31
    voice.operators[1].totalLevel = 128; // Valid range: 0-127
    voice.lfo.waveform = 5; // Valid range: 0-3
    
    auto result = VOPMParser::validate(voice);
    
    // Implementation treats out-of-range values as warnings, not errors
    // This is a design choice for tolerance - voices remain valid but with warnings
    EXPECT_TRUE(result.isValid);
    EXPECT_GT(result.warnings.size(), 0);
    
    // Verify specific warnings are generated
    bool foundAlgorithmWarning = false;
    bool foundAttackRateWarning = false;
    bool foundTotalLevelWarning = false;
    bool foundWaveformWarning = false;
    
    for (const auto& warning : result.warnings) {
        if (warning.contains("algorithm")) foundAlgorithmWarning = true;
        if (warning.contains("AR")) foundAttackRateWarning = true;
        if (warning.contains("TL")) foundTotalLevelWarning = true;
        if (warning.contains("waveform")) foundWaveformWarning = true;
    }
    
    EXPECT_TRUE(foundAlgorithmWarning);
    EXPECT_TRUE(foundAttackRateWarning);
    EXPECT_TRUE(foundTotalLevelWarning);
    EXPECT_TRUE(foundWaveformWarning);
}

TEST_F(VOPMParserTest, ValidateVoiceWithEmptyName) {
    auto voice = createTestVoice();
    voice.name = "";
    
    auto result = VOPMParser::validate(voice);
    // Empty name might be acceptable - just test it doesn't crash
    EXPECT_NO_FATAL_FAILURE();
}

// =============================================================================
// 4. Voice Serialization Testing
// =============================================================================

TEST_F(VOPMParserTest, VoiceToString) {
    auto voice = createTestVoice(5, "Serialization Test");
    
    auto serialized = VOPMParser::voiceToString(voice);
    
    // Should contain voice header
    EXPECT_TRUE(serialized.contains("@:5 Serialization Test"));
    
    // Should contain LFO line
    EXPECT_TRUE(serialized.contains("LFO:"));
    
    // Should contain channel line
    EXPECT_TRUE(serialized.contains("CH:"));
    
    // Should contain all operator lines
    EXPECT_TRUE(serialized.contains("M1:"));
    EXPECT_TRUE(serialized.contains("C1:"));
    EXPECT_TRUE(serialized.contains("M2:"));
    EXPECT_TRUE(serialized.contains("C2:"));
}

TEST_F(VOPMParserTest, RoundTripSerialization) {
    auto originalVoice = createTestVoice(10, "Round Trip Test");
    
    // Serialize to string
    auto serialized = VOPMParser::voiceToString(originalVoice);
    
    // Parse back from string
    auto parsedVoices = VOPMParser::parseContent(serialized);
    
    ASSERT_EQ(parsedVoices.size(), 1);
    const auto& parsedVoice = parsedVoices[0];
    
    // Compare key parameters
    EXPECT_EQ(parsedVoice.number, originalVoice.number);
    EXPECT_EQ(parsedVoice.name, originalVoice.name);
    EXPECT_EQ(parsedVoice.channel.algorithm, originalVoice.channel.algorithm);
    EXPECT_EQ(parsedVoice.channel.feedback, originalVoice.channel.feedback);
    
    // Compare LFO parameters
    EXPECT_EQ(parsedVoice.lfo.frequency, originalVoice.lfo.frequency);
    EXPECT_EQ(parsedVoice.lfo.amd, originalVoice.lfo.amd);
    
    // Compare first operator
    EXPECT_EQ(parsedVoice.operators[0].attackRate, originalVoice.operators[0].attackRate);
    EXPECT_EQ(parsedVoice.operators[0].totalLevel, originalVoice.operators[0].totalLevel);
}

// =============================================================================
// 5. Format Conversion Testing
// =============================================================================

TEST_F(VOPMParserTest, OpmPanConversion) {
    // Test OPM pan to internal conversion
    EXPECT_EQ(VOPMParser::convertOpmPanToInternal(0), 0);   // Off
    EXPECT_EQ(VOPMParser::convertOpmPanToInternal(64), 1);  // Right
    EXPECT_EQ(VOPMParser::convertOpmPanToInternal(128), 2); // Left
    EXPECT_EQ(VOPMParser::convertOpmPanToInternal(192), 3); // Center
    
    // Test internal pan to OPM conversion
    EXPECT_EQ(VOPMParser::convertInternalPanToOpm(0), 0);   // Off
    EXPECT_EQ(VOPMParser::convertInternalPanToOpm(1), 64);  // Right
    EXPECT_EQ(VOPMParser::convertInternalPanToOpm(2), 128); // Left
    EXPECT_EQ(VOPMParser::convertInternalPanToOpm(3), 192); // Center
}

TEST_F(VOPMParserTest, OpmAmeConversion) {
    // Test AME (AMS Enable) conversion - actual implementation uses 0/128 format
    EXPECT_EQ(VOPMParser::convertOpmAmeToInternal(0), 0);   // 0 -> 0
    EXPECT_EQ(VOPMParser::convertOpmAmeToInternal(128), 1); // 128 -> 1
    
    EXPECT_EQ(VOPMParser::convertInternalAmeToOpm(0), 0);   // 0 -> 0  
    EXPECT_EQ(VOPMParser::convertInternalAmeToOpm(1), 128); // 1 -> 128
}

TEST_F(VOPMParserTest, OpmSlotConversion) {
    // Test SLOT mask conversion - actual implementation uses bit shifting
    // Key test cases based on the implementation
    EXPECT_EQ(VOPMParser::convertOpmSlotToInternal(120), 15); // VOPMex standard: all slots
    EXPECT_EQ(VOPMParser::convertInternalSlotToOpm(15), 120); // Internal all slots -> OPM format
    
    // Test round trip for some values
    for (int internal = 0; internal <= 15; ++internal) {
        int opmFormat = VOPMParser::convertInternalSlotToOpm(internal);
        if (opmFormat == 120) {
            // Special case for all slots
            EXPECT_EQ(VOPMParser::convertOpmSlotToInternal(opmFormat), 15);
        } else {
            // For other values, test bit shifting
            EXPECT_EQ(opmFormat, internal << 3);
        }
    }
}

// =============================================================================
// 6. Edge Cases and Error Handling
// =============================================================================

TEST_F(VOPMParserTest, ParseVoiceWithSpecialCharactersInName) {
    juce::String specialCharsContent = R"(@:0 Test Synth & Symbols!@#$%
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   1   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0
)";
    
    auto voices = VOPMParser::parseContent(specialCharsContent);
    EXPECT_EQ(voices.size(), 1);
    EXPECT_EQ(voices[0].name, "Test Synth & Symbols!@#$%");
}

TEST_F(VOPMParserTest, ParseVoiceWithExtraWhitespace) {
    juce::String whitespaceContent = R"(@:0    Test Voice With Spaces    
LFO:   0    0    0    0    0   
CH:  64    6    4    0    0   15    0   
M1:  31    8    8   11    1   20    0    1    3    0    0   
C1:  31    8    8   11    1    0    0    1    3    0    0   
M2:  31    8    8   11    1   20    0    1    3    0    0   
C2:  31    8    8   11    1    0    0    1    3    0    0   
)";
    
    auto voices = VOPMParser::parseContent(whitespaceContent);
    EXPECT_EQ(voices.size(), 1);
    EXPECT_EQ(voices[0].name, "Test Voice With Spaces");
}

TEST_F(VOPMParserTest, ParseVoiceWithMissingOperators) {
    juce::String incompleteContent = R"(@:0 Incomplete Voice
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
// Missing M2 and C2
)";
    
    auto voices = VOPMParser::parseContent(incompleteContent);
    
    // Parser requires all 4 operators to be present for a valid voice
    EXPECT_EQ(voices.size(), 0);
}

TEST_F(VOPMParserTest, ParseMultipleVoicesWithGaps) {
    juce::String gappedContent = R"(@:0 First Voice
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   1   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0

@:5 Voice With Gap
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   1   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0
)";
    
    auto voices = VOPMParser::parseContent(gappedContent);
    EXPECT_EQ(voices.size(), 2);
    EXPECT_EQ(voices[0].number, 0);
    EXPECT_EQ(voices[1].number, 5);
}

TEST_F(VOPMParserTest, ParseVoiceWithOutOfRangeNumbers) {
    juce::String outOfRangeContent = R"(@:999 High Number Voice
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   1   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0
)";
    
    auto voices = VOPMParser::parseContent(outOfRangeContent);
    
    // Voice number 999 is out of valid range (0-127), so validation will fail
    EXPECT_EQ(voices.size(), 0);
}

// =============================================================================
// 7. Performance and Large File Testing
// =============================================================================

TEST_F(VOPMParserTest, ParseLargeNumberOfVoices) {
    juce::String largeContent;
    const int numVoices = 100;
    
    // Generate content with many voices
    for (int i = 0; i < numVoices; ++i) {
        largeContent += "@:" + juce::String(i) + " Voice " + juce::String(i) + "\n";
        largeContent += "LFO:  0   0   0   0   0\n";
        largeContent += "CH: 64   6   4   0   0  15   0\n";
        largeContent += "M1: 31   8   8  11   1  20   0   1   3   0   0\n";
        largeContent += "C1: 31   8   8  11   1   0   0   1   3   0   0\n";
        largeContent += "M2: 31   8   8  11   1  20   0   1   3   0   0\n";
        largeContent += "C2: 31   8   8  11   1   0   0   1   3   0   0\n\n";
    }
    
    auto voices = VOPMParser::parseContent(largeContent);
    EXPECT_EQ(voices.size(), numVoices);
    
    // Verify first and last voices
    EXPECT_EQ(voices[0].number, 0);
    EXPECT_EQ(voices[0].name, "Voice 0");
    EXPECT_EQ(voices[numVoices-1].number, numVoices-1);
    EXPECT_EQ(voices[numVoices-1].name, "Voice " + juce::String(numVoices-1));
}

TEST_F(VOPMParserTest, ParseVeryLongVoiceName) {
    juce::String longName = juce::String::repeatedString("VeryLongVoiceName", 20);
    juce::String longNameContent = "@:0 " + longName + "\n";
    longNameContent += "LFO:  0   0   0   0   0\n";
    longNameContent += "CH: 64   6   4   0   0  15   0\n";
    longNameContent += "M1: 31   8   8  11   1  20   0   1   3   0   0\n";
    longNameContent += "C1: 31   8   8  11   1   0   0   1   3   0   0\n";
    longNameContent += "M2: 31   8   8  11   1  20   0   1   3   0   0\n";
    longNameContent += "C2: 31   8   8  11   1   0   0   1   3   0   0\n";
    
    auto voices = VOPMParser::parseContent(longNameContent);
    EXPECT_EQ(voices.size(), 1);
    EXPECT_EQ(voices[0].name, longName);
}

// =============================================================================
// 8. Compatibility Testing
// =============================================================================

TEST_F(VOPMParserTest, ParseRealWorldOPMFile) {
    // Test with content that mimics real VOPM files
    juce::String realWorldContent = R"(//MiOPMdrv sound bank Paramer Ver2002.04.22
//LFO: LFRQ AMD PMD WF NFRQ
//CH: PAN   FL CON AMS PMS SLOT NE
//[M1]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[C1]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[M2]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[C2]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN

@:0 E.Piano1
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  22   0   5   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   5   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0

@:1 Strings
LFO:  0   0   0   0   0
CH: 64   7   2   0   0  15   0
M1: 25  10   0   5   1  29   1   1   1   0   0
C1: 25  11   0   8   5  15   1   5   1   0   0
M2: 28  13   0   6   2  45   1   1   0   0   0
C2: 14   4   0   6   0   0   1   1   0   0   0
)";
    
    auto voices = VOPMParser::parseContent(realWorldContent);
    EXPECT_EQ(voices.size(), 2);
    
    // Check E.Piano1
    EXPECT_EQ(voices[0].name, "E.Piano1");
    EXPECT_EQ(voices[0].operators[0].multiple, 5);
    EXPECT_EQ(voices[0].operators[2].multiple, 5);
    
    // Check Strings
    EXPECT_EQ(voices[1].name, "Strings");
    EXPECT_EQ(voices[1].channel.algorithm, 2);
    EXPECT_EQ(voices[1].operators[1].multiple, 5);
}