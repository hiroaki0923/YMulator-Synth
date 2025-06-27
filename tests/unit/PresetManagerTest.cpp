#include <gtest/gtest.h>
#include "utils/PresetManager.h"
#include "utils/VOPMParser.h"
#include "utils/Debug.h"
#include <juce_core/juce_core.h>
#include <fstream>

using namespace ymulatorsynth;

class PresetManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        presetManager = std::make_unique<PresetManager>();
        
        // Create temporary directory for testing
        tempDir = juce::File::createTempFile("PresetManagerTest");
        tempDir.deleteFile();
        tempDir.createDirectory();
    }
    
    void TearDown() override {
        presetManager.reset();
        if (tempDir.exists()) {
            tempDir.deleteRecursively();
        }
    }
    
    std::unique_ptr<PresetManager> presetManager;
    juce::File tempDir;
    
    // Helper to create test OPM file
    juce::File createTestOPMFile(const juce::String& filename, const juce::String& content) {
        auto file = tempDir.getChildFile(filename);
        file.replaceWithText(content);
        return file;
    }
    
    // Helper to create a test preset
    Preset createTestPreset(int id, const juce::String& name) {
        Preset preset;
        preset.id = id;
        preset.name = name;
        preset.algorithm = 4;
        preset.feedback = 3;
        
        // Set some operator parameters
        for (int i = 0; i < 4; ++i) {
            preset.operators[i].totalLevel = 20.0f + i * 5;
            preset.operators[i].attackRate = 31.0f;
            preset.operators[i].releaseRate = 7.0f;
            preset.operators[i].multiple = 1.0f + i;
        }
        
        return preset;
    }
    
    // Valid OPM content for testing
    const juce::String validOPMContent = R"(//MiOPMdrv sound bank Paramer Ver2002.04.22
//LFO: LFRQ AMD PMD WF NFRQ
//CH: PAN   FL CON AMS PMS SLOT NE
//[M1]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[C1]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[M2]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN
//[C2]: AR D1R D2R  RR D1L  TL  KS MUL DT1 DT2 AMS-EN

@:0 Instrument Name
LFO:  0   0   0   0   0
CH: 64   6   4   0   0  15   0
M1: 31   8   8  11   1  20   0   1   3   0   0
C1: 31   8   8  11   1   0   0   1   3   0   0
M2: 31   8   8  11   1  20   0   1   3   0   0
C2: 31   8   8  11   1   0   0   1   3   0   0

@:1 Test Instrument 2
LFO:  0   0   0   0   0
CH: 64   7   2   0   0  15   0
M1: 25  10   0   5   1  29   1   1   1   0   0
C1: 25  11   0   8   5  15   1   5   1   0   0
M2: 28  13   0   6   2  45   1   1   0   0   0
C2: 14   4   0   6   0   0   1   1   0   0   0
)";
};

// =============================================================================
// 1. Constructor and Initialization Testing
// =============================================================================

TEST_F(PresetManagerTest, ConstructorInitializesCorrectly) {
    // Should start with no presets
    EXPECT_EQ(presetManager->getNumPresets(), 0);
    EXPECT_TRUE(presetManager->getPresetNames().isEmpty());
    EXPECT_TRUE(presetManager->getBanks().empty());
}

TEST_F(PresetManagerTest, InitializeLoadsFactoryPresets) {
    presetManager->initialize();
    
    // Should load factory presets
    EXPECT_GT(presetManager->getNumPresets(), 0);
    
    // Should have at least the basic factory presets
    auto names = presetManager->getPresetNames();
    EXPECT_TRUE(names.contains("Electric Piano"));
    EXPECT_TRUE(names.contains("Synth Bass"));
    EXPECT_TRUE(names.contains("Init"));
}

TEST_F(PresetManagerTest, FactoryPresetsAreValid) {
    presetManager->initialize();
    
    // Check that first few factory presets have valid data
    auto preset = presetManager->getPreset(0);
    ASSERT_NE(preset, nullptr);
    EXPECT_FALSE(preset->name.isEmpty());
    EXPECT_GE(preset->algorithm, 0);
    EXPECT_LE(preset->algorithm, 7);
    EXPECT_GE(preset->feedback, 0);
    EXPECT_LE(preset->feedback, 7);
    
    // Check operator parameters are in valid ranges
    for (int i = 0; i < 4; ++i) {
        EXPECT_GE(preset->operators[i].totalLevel, 0.0f);
        EXPECT_LE(preset->operators[i].totalLevel, 127.0f);
        EXPECT_GE(preset->operators[i].attackRate, 0.0f);
        EXPECT_LE(preset->operators[i].attackRate, 31.0f);
    }
}

// =============================================================================
// 2. OPM File Loading Testing
// =============================================================================

TEST_F(PresetManagerTest, LoadValidOPMFile) {
    auto opmFile = createTestOPMFile("test.opm", validOPMContent);
    
    int loaded = presetManager->loadOPMFile(opmFile);
    EXPECT_EQ(loaded, 2); // Should load 2 instruments from the test content
    
    // Verify presets were added
    EXPECT_EQ(presetManager->getNumPresets(), 2);
    
    // Check first preset
    auto preset1 = presetManager->getPreset(0);
    ASSERT_NE(preset1, nullptr);
    EXPECT_EQ(preset1->name, "Instrument Name");
    EXPECT_EQ(preset1->algorithm, 4);
    EXPECT_EQ(preset1->feedback, 6);
    
    // Check second preset
    auto preset2 = presetManager->getPreset(1);
    ASSERT_NE(preset2, nullptr);
    EXPECT_EQ(preset2->name, "Test Instrument 2");
    EXPECT_EQ(preset2->algorithm, 2);
    EXPECT_EQ(preset2->feedback, 7);
}

TEST_F(PresetManagerTest, LoadNonexistentFile) {
    auto nonexistentFile = tempDir.getChildFile("nonexistent.opm");
    
    int loaded = presetManager->loadOPMFile(nonexistentFile);
    EXPECT_EQ(loaded, 0);
    EXPECT_EQ(presetManager->getNumPresets(), 0);
}

TEST_F(PresetManagerTest, LoadEmptyOPMFile) {
    auto emptyFile = createTestOPMFile("empty.opm", "");
    
    int loaded = presetManager->loadOPMFile(emptyFile);
    EXPECT_EQ(loaded, 0);
    EXPECT_EQ(presetManager->getNumPresets(), 0);
}

TEST_F(PresetManagerTest, LoadInvalidOPMFile) {
    auto invalidFile = createTestOPMFile("invalid.opm", "This is not a valid OPM file");
    
    int loaded = presetManager->loadOPMFile(invalidFile);
    EXPECT_EQ(loaded, 0);
    EXPECT_EQ(presetManager->getNumPresets(), 0);
}

TEST_F(PresetManagerTest, LoadOPMFileCreatesBank) {
    auto opmFile = createTestOPMFile("testbank.opm", validOPMContent);
    
    presetManager->loadOPMFile(opmFile);
    
    // Should create a bank
    auto& banks = presetManager->getBanks();
    EXPECT_EQ(banks.size(), 1);
    EXPECT_EQ(banks[0].name, "testbank");
    EXPECT_EQ(banks[0].fileName, "testbank.opm");
    EXPECT_EQ(banks[0].presetIndices.size(), 2);
}

TEST_F(PresetManagerTest, LoadDuplicateOPMFile) {
    auto opmFile = createTestOPMFile("duplicate.opm", validOPMContent);
    
    // Load first time
    int loaded1 = presetManager->loadOPMFile(opmFile);
    EXPECT_EQ(loaded1, 2);
    
    // Load same file again - should skip
    int loaded2 = presetManager->loadOPMFile(opmFile);
    EXPECT_EQ(loaded2, 0);
    EXPECT_EQ(presetManager->getNumPresets(), 2); // Should not duplicate
}

// =============================================================================
// 3. Preset Access and Queries Testing
// =============================================================================

TEST_F(PresetManagerTest, GetPresetById) {
    auto testPreset = createTestPreset(0, "Test Preset");
    presetManager->addPreset(testPreset);
    
    auto retrieved = presetManager->getPreset(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, 0);
    EXPECT_EQ(retrieved->name, "Test Preset");
    EXPECT_EQ(retrieved->algorithm, 4);
    EXPECT_EQ(retrieved->feedback, 3);
}

TEST_F(PresetManagerTest, GetPresetByInvalidId) {
    auto retrieved = presetManager->getPreset(999);
    EXPECT_EQ(retrieved, nullptr);
    
    // Negative ID
    retrieved = presetManager->getPreset(-1);
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(PresetManagerTest, GetPresetByName) {
    auto testPreset = createTestPreset(0, "Unique Name");
    presetManager->addPreset(testPreset);
    
    auto retrieved = presetManager->getPreset("Unique Name");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Unique Name");
    
    // Non-existent name
    auto notFound = presetManager->getPreset("Non-existent");
    EXPECT_EQ(notFound, nullptr);
}

TEST_F(PresetManagerTest, GetPresetNames) {
    presetManager->addPreset(createTestPreset(0, "First"));
    presetManager->addPreset(createTestPreset(1, "Second"));
    presetManager->addPreset(createTestPreset(2, "Third"));
    
    auto names = presetManager->getPresetNames();
    EXPECT_EQ(names.size(), 3);
    EXPECT_TRUE(names.contains("First"));
    EXPECT_TRUE(names.contains("Second"));
    EXPECT_TRUE(names.contains("Third"));
}

TEST_F(PresetManagerTest, GetNumPresets) {
    EXPECT_EQ(presetManager->getNumPresets(), 0);
    
    presetManager->addPreset(createTestPreset(0, "Test 1"));
    EXPECT_EQ(presetManager->getNumPresets(), 1);
    
    presetManager->addPreset(createTestPreset(1, "Test 2"));
    EXPECT_EQ(presetManager->getNumPresets(), 2);
}

// =============================================================================
// 4. Preset Management Testing
// =============================================================================

TEST_F(PresetManagerTest, AddPreset) {
    auto testPreset = createTestPreset(42, "Test Preset");
    
    presetManager->addPreset(testPreset);
    EXPECT_EQ(presetManager->getNumPresets(), 1);
    
    auto retrieved = presetManager->getPreset(0); // Gets by index, not ID
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->id, 42);
    EXPECT_EQ(retrieved->name, "Test Preset");
}

TEST_F(PresetManagerTest, AddPresetWithDuplicateId) {
    auto preset1 = createTestPreset(5, "First");
    auto preset2 = createTestPreset(5, "Second"); // Same ID
    
    presetManager->addPreset(preset1);
    EXPECT_EQ(presetManager->getNumPresets(), 1);
    
    // Adding preset with same ID should replace the first one
    presetManager->addPreset(preset2);
    EXPECT_EQ(presetManager->getNumPresets(), 1);
    
    auto retrieved = presetManager->getPreset(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, "Second");
}

TEST_F(PresetManagerTest, RemovePreset) {
    presetManager->addPreset(createTestPreset(0, "Keep"));
    presetManager->addPreset(createTestPreset(1, "Remove"));
    presetManager->addPreset(createTestPreset(2, "Keep Too"));
    
    EXPECT_EQ(presetManager->getNumPresets(), 3);
    
    presetManager->removePreset(1);
    EXPECT_EQ(presetManager->getNumPresets(), 2);
    
    // Verify the correct preset was removed
    auto names = presetManager->getPresetNames();
    EXPECT_TRUE(names.contains("Keep"));
    EXPECT_FALSE(names.contains("Remove"));
    EXPECT_TRUE(names.contains("Keep Too"));
}

TEST_F(PresetManagerTest, RemoveNonexistentPreset) {
    presetManager->addPreset(createTestPreset(0, "Test"));
    EXPECT_EQ(presetManager->getNumPresets(), 1);
    
    // Removing non-existent preset should not crash or affect existing presets
    presetManager->removePreset(999);
    EXPECT_EQ(presetManager->getNumPresets(), 1);
}

TEST_F(PresetManagerTest, ClearPresets) {
    presetManager->addPreset(createTestPreset(0, "Test 1"));
    presetManager->addPreset(createTestPreset(1, "Test 2"));
    EXPECT_EQ(presetManager->getNumPresets(), 2);
    
    presetManager->clear();
    EXPECT_EQ(presetManager->getNumPresets(), 0);
    EXPECT_TRUE(presetManager->getPresetNames().isEmpty());
    EXPECT_TRUE(presetManager->getBanks().empty());
}

// =============================================================================
// 5. Bank Management Testing
// =============================================================================

TEST_F(PresetManagerTest, GetBanks) {
    // Initially no banks
    EXPECT_TRUE(presetManager->getBanks().empty());
    
    // Load an OPM file to create a bank
    auto opmFile = createTestOPMFile("testbank.opm", validOPMContent);
    presetManager->loadOPMFile(opmFile);
    
    auto& banks = presetManager->getBanks();
    EXPECT_EQ(banks.size(), 1);
    EXPECT_EQ(banks[0].name, "testbank");
}

TEST_F(PresetManagerTest, GetPresetsForBank) {
    auto opmFile = createTestOPMFile("testbank.opm", validOPMContent);
    presetManager->loadOPMFile(opmFile);
    
    auto bankPresets = presetManager->getPresetsForBank(0);
    EXPECT_EQ(bankPresets.size(), 2);
    EXPECT_TRUE(bankPresets.contains("Instrument Name"));
    EXPECT_TRUE(bankPresets.contains("Test Instrument 2"));
}

TEST_F(PresetManagerTest, GetPresetsForInvalidBank) {
    auto bankPresets = presetManager->getPresetsForBank(999);
    EXPECT_TRUE(bankPresets.isEmpty());
}

TEST_F(PresetManagerTest, GetPresetInBank) {
    auto opmFile = createTestOPMFile("testbank.opm", validOPMContent);
    presetManager->loadOPMFile(opmFile);
    
    auto preset = presetManager->getPresetInBank(0, 0);
    ASSERT_NE(preset, nullptr);
    EXPECT_EQ(preset->name, "Instrument Name");
    
    preset = presetManager->getPresetInBank(0, 1);
    ASSERT_NE(preset, nullptr);
    EXPECT_EQ(preset->name, "Test Instrument 2");
    
    // Invalid indices
    preset = presetManager->getPresetInBank(0, 999);
    EXPECT_EQ(preset, nullptr);
    
    preset = presetManager->getPresetInBank(999, 0);
    EXPECT_EQ(preset, nullptr);
}

TEST_F(PresetManagerTest, GetGlobalPresetIndex) {
    auto opmFile = createTestOPMFile("testbank.opm", validOPMContent);
    presetManager->loadOPMFile(opmFile);
    
    int globalIndex = presetManager->getGlobalPresetIndex(0, 0);
    EXPECT_EQ(globalIndex, 0);
    
    globalIndex = presetManager->getGlobalPresetIndex(0, 1);
    EXPECT_EQ(globalIndex, 1);
    
    // Invalid indices
    globalIndex = presetManager->getGlobalPresetIndex(999, 0);
    EXPECT_EQ(globalIndex, -1);
    
    globalIndex = presetManager->getGlobalPresetIndex(0, 999);
    EXPECT_EQ(globalIndex, -1);
}

// =============================================================================
// 6. File Saving Testing
// =============================================================================

TEST_F(PresetManagerTest, SaveOPMFile) {
    // Add some test presets
    presetManager->addPreset(createTestPreset(0, "Test 1"));
    presetManager->addPreset(createTestPreset(1, "Test 2"));
    
    auto saveFile = tempDir.getChildFile("saved.opm");
    bool success = presetManager->saveOPMFile(saveFile);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(saveFile.exists());
    EXPECT_GT(saveFile.getSize(), 0);
    
    // Verify we can load the saved file
    auto loadManager = std::make_unique<PresetManager>();
    int loaded = loadManager->loadOPMFile(saveFile);
    EXPECT_EQ(loaded, 2);
}

TEST_F(PresetManagerTest, SavePresetAsOPM) {
    auto testPreset = createTestPreset(0, "Single Test");
    
    auto saveFile = tempDir.getChildFile("single.opm");
    bool success = presetManager->savePresetAsOPM(saveFile, testPreset);
    
    EXPECT_TRUE(success);
    EXPECT_TRUE(saveFile.exists());
    EXPECT_GT(saveFile.getSize(), 0);
    
    // Verify the saved file contains our preset
    auto loadManager = std::make_unique<PresetManager>();
    int loaded = loadManager->loadOPMFile(saveFile);
    EXPECT_EQ(loaded, 1);
    
    auto loadedPreset = loadManager->getPreset(0);
    ASSERT_NE(loadedPreset, nullptr);
    EXPECT_EQ(loadedPreset->name, "Single Test");
}

TEST_F(PresetManagerTest, SaveToInvalidPath) {
    auto testPreset = createTestPreset(0, "Test");
    
    // Try to save to a directory that doesn't exist and can't be created
    auto invalidFile = juce::File("/invalid/path/that/does/not/exist/test.opm");
    bool success = presetManager->savePresetAsOPM(invalidFile, testPreset);
    
    // Should handle gracefully without crashing
    EXPECT_FALSE(success);
}

// =============================================================================
// 7. VOPM Conversion Testing
// =============================================================================

TEST_F(PresetManagerTest, PresetToVOPMConversion) {
    auto testPreset = createTestPreset(10, "Conversion Test");
    
    auto vopmVoice = testPreset.toVOPM();
    
    EXPECT_EQ(vopmVoice.number, 10);
    EXPECT_EQ(vopmVoice.name, "Conversion Test");
    EXPECT_EQ(vopmVoice.channel.algorithm, 4);
    EXPECT_EQ(vopmVoice.channel.feedback, 3);
    
    // Check operator parameters were converted
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(vopmVoice.operators[i].totalLevel, static_cast<int>(testPreset.operators[i].totalLevel));
        EXPECT_EQ(vopmVoice.operators[i].multiple, static_cast<int>(testPreset.operators[i].multiple));
    }
}

TEST_F(PresetManagerTest, VOPMToPresetConversion) {
    // Create a VOPM voice
    VOPMVoice voice;
    voice.number = 5;
    voice.name = "VOPM Test";
    voice.channel.algorithm = 2;
    voice.channel.feedback = 5;
    voice.channel.pan = 3;
    voice.channel.slotMask = 15; // All operators enabled
    
    for (int i = 0; i < 4; ++i) {
        voice.operators[i].totalLevel = 30 + i * 5;
        voice.operators[i].attackRate = 25;
        voice.operators[i].releaseRate = 10;
        voice.operators[i].multiple = i + 1;
    }
    
    auto preset = Preset::fromVOPM(voice);
    
    EXPECT_EQ(preset.id, 5);
    EXPECT_EQ(preset.name, "VOPM Test");
    EXPECT_EQ(preset.algorithm, 2);
    EXPECT_EQ(preset.feedback, 5);
    
    // Check operator parameters
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(preset.operators[i].totalLevel, 30.0f + i * 5);
        EXPECT_FLOAT_EQ(preset.operators[i].attackRate, 25.0f);
        EXPECT_FLOAT_EQ(preset.operators[i].releaseRate, 10.0f);
        EXPECT_FLOAT_EQ(preset.operators[i].multiple, static_cast<float>(i + 1));
        EXPECT_TRUE(preset.operators[i].slotEnable); // Should be enabled from slotMask
    }
}

TEST_F(PresetManagerTest, RoundTripConversion) {
    auto originalPreset = createTestPreset(7, "Round Trip");
    
    // Convert to VOPM and back
    auto vopmVoice = originalPreset.toVOPM();
    auto convertedPreset = Preset::fromVOPM(vopmVoice);
    
    EXPECT_EQ(convertedPreset.id, originalPreset.id);
    EXPECT_EQ(convertedPreset.name, originalPreset.name);
    EXPECT_EQ(convertedPreset.algorithm, originalPreset.algorithm);
    EXPECT_EQ(convertedPreset.feedback, originalPreset.feedback);
    
    // Check operator parameters within reasonable precision
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(convertedPreset.operators[i].totalLevel, originalPreset.operators[i].totalLevel);
        EXPECT_FLOAT_EQ(convertedPreset.operators[i].multiple, originalPreset.operators[i].multiple);
    }
}

// =============================================================================
// 8. Edge Cases and Error Handling
// =============================================================================

TEST_F(PresetManagerTest, HandleEmptyPresetNames) {
    auto preset = createTestPreset(0, "");
    presetManager->addPreset(preset);
    
    EXPECT_EQ(presetManager->getNumPresets(), 1);
    auto retrieved = presetManager->getPreset(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_TRUE(retrieved->name.isEmpty());
}

TEST_F(PresetManagerTest, HandleVeryLongPresetNames) {
    juce::String longName = juce::String::repeatedString("A", 1000);
    auto preset = createTestPreset(0, longName);
    
    presetManager->addPreset(preset);
    auto retrieved = presetManager->getPreset(0);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->name, longName);
}

TEST_F(PresetManagerTest, HandleExtremeParameterValues) {
    auto preset = createTestPreset(0, "Extreme");
    
    // Set extreme but valid values
    for (int i = 0; i < 4; ++i) {
        preset.operators[i].totalLevel = 127.0f; // Maximum
        preset.operators[i].attackRate = 31.0f;  // Maximum
        preset.operators[i].releaseRate = 0.0f;  // Minimum
    }
    
    presetManager->addPreset(preset);
    auto retrieved = presetManager->getPreset(0);
    ASSERT_NE(retrieved, nullptr);
    
    for (int i = 0; i < 4; ++i) {
        EXPECT_FLOAT_EQ(retrieved->operators[i].totalLevel, 127.0f);
        EXPECT_FLOAT_EQ(retrieved->operators[i].attackRate, 31.0f);
        EXPECT_FLOAT_EQ(retrieved->operators[i].releaseRate, 0.0f);
    }
}

TEST_F(PresetManagerTest, HandleManyPresets) {
    // Add many presets to test performance and memory handling
    const int numPresets = 1000;
    
    for (int i = 0; i < numPresets; ++i) {
        auto preset = createTestPreset(i, "Preset " + juce::String(i));
        presetManager->addPreset(preset);
    }
    
    EXPECT_EQ(presetManager->getNumPresets(), numPresets);
    
    // Verify random access works
    auto preset = presetManager->getPreset(500);
    ASSERT_NE(preset, nullptr);
    EXPECT_EQ(preset->name, "Preset 500");
    
    // Verify name search still works
    auto foundPreset = presetManager->getPreset("Preset 750");
    ASSERT_NE(foundPreset, nullptr);
    EXPECT_EQ(foundPreset->id, 750);
}

TEST_F(PresetManagerTest, HandleRepeatedInitialization) {
    presetManager->initialize();
    int firstCount = presetManager->getNumPresets();
    EXPECT_GT(firstCount, 0);
    
    // Initialize again - should reset and reload
    presetManager->initialize();
    int secondCount = presetManager->getNumPresets();
    
    // Should have same number of factory presets
    EXPECT_EQ(firstCount, secondCount);
}