#include <gtest/gtest.h>
#include "core/VoiceManager.h"
#include "utils/Debug.h"

class VoiceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        voiceManager = std::make_unique<VoiceManager>();
    }
    
    void TearDown() override {
        voiceManager.reset();
    }
    
    std::unique_ptr<VoiceManager> voiceManager;
};

// =============================================================================
// 1. Basic Voice Allocation Testing
// =============================================================================

TEST_F(VoiceManagerTest, ConstructorInitializesCorrectly) {
    // All voices should be inactive initially
    for (int channel = 0; channel < VoiceManager::MAX_VOICES; ++channel) {
        EXPECT_FALSE(voiceManager->isVoiceActive(channel));
        EXPECT_EQ(voiceManager->getNoteForChannel(channel), 0);
        EXPECT_EQ(voiceManager->getVelocityForChannel(channel), 0);
    }
    
    // No notes should be allocated
    for (int note = 0; note <= 127; ++note) {
        EXPECT_EQ(voiceManager->getChannelForNote(note), -1);
    }
}

TEST_F(VoiceManagerTest, SingleVoiceAllocation) {
    // Allocate first voice
    int channel = voiceManager->allocateVoice(60, 100);
    
    // Should use channel 7 (highest priority for noise capability)
    EXPECT_EQ(channel, 7);
    EXPECT_TRUE(voiceManager->isVoiceActive(7));
    EXPECT_EQ(voiceManager->getNoteForChannel(7), 60);
    EXPECT_EQ(voiceManager->getVelocityForChannel(7), 100);
    EXPECT_EQ(voiceManager->getChannelForNote(60), 7);
    
    // Other channels should remain inactive
    for (int ch = 0; ch < VoiceManager::MAX_VOICES - 1; ++ch) {
        EXPECT_FALSE(voiceManager->isVoiceActive(ch));
    }
}

TEST_F(VoiceManagerTest, MultipleVoiceAllocation) {
    std::vector<std::pair<uint8_t, uint8_t>> testNotes = {
        {60, 100}, {64, 110}, {67, 90}, {72, 120}
    };
    
    // Channels should be allocated in descending order: 7, 6, 5, 4
    std::vector<int> expectedChannels = {7, 6, 5, 4};
    
    for (size_t i = 0; i < testNotes.size(); ++i) {
        int channel = voiceManager->allocateVoice(testNotes[i].first, testNotes[i].second);
        
        // Should allocate in descending order
        EXPECT_EQ(channel, expectedChannels[i]);
        EXPECT_TRUE(voiceManager->isVoiceActive(channel));
        EXPECT_EQ(voiceManager->getNoteForChannel(channel), testNotes[i].first);
        EXPECT_EQ(voiceManager->getVelocityForChannel(channel), testNotes[i].second);
        EXPECT_EQ(voiceManager->getChannelForNote(testNotes[i].first), channel);
    }
}

TEST_F(VoiceManagerTest, MaxPolyphonyAllocation) {
    // Allocate all 8 voices - channels should be allocated in descending order
    std::vector<int> expectedChannels = {7, 6, 5, 4, 3, 2, 1, 0};
    
    for (int i = 0; i < VoiceManager::MAX_VOICES; ++i) {
        int note = 60 + i;
        int velocity = 100 + i;
        int channel = voiceManager->allocateVoice(note, velocity);
        
        EXPECT_EQ(channel, expectedChannels[i]);
        EXPECT_TRUE(voiceManager->isVoiceActive(channel));
        EXPECT_EQ(voiceManager->getNoteForChannel(channel), note);
        EXPECT_EQ(voiceManager->getVelocityForChannel(channel), velocity);
    }
    
    // All channels should be active
    for (int ch = 0; ch < VoiceManager::MAX_VOICES; ++ch) {
        EXPECT_TRUE(voiceManager->isVoiceActive(ch));
    }
}

// =============================================================================
// 2. Voice Release Testing
// =============================================================================

TEST_F(VoiceManagerTest, SingleVoiceRelease) {
    // Allocate and release a voice
    int channel = voiceManager->allocateVoice(60, 100);
    EXPECT_TRUE(voiceManager->isVoiceActive(channel));
    
    voiceManager->releaseVoice(60);
    EXPECT_FALSE(voiceManager->isVoiceActive(channel));
    EXPECT_EQ(voiceManager->getChannelForNote(60), -1);
}

TEST_F(VoiceManagerTest, MultipleVoiceRelease) {
    // Allocate multiple voices (channels will be 7, 6, 5)
    int channel1 = voiceManager->allocateVoice(60, 100); // Should be channel 7
    int channel2 = voiceManager->allocateVoice(64, 110); // Should be channel 6  
    int channel3 = voiceManager->allocateVoice(67, 90);  // Should be channel 5
    
    EXPECT_EQ(channel1, 7);
    EXPECT_EQ(channel2, 6);
    EXPECT_EQ(channel3, 5);
    
    // Release middle voice
    voiceManager->releaseVoice(64);
    EXPECT_TRUE(voiceManager->isVoiceActive(channel1));
    EXPECT_FALSE(voiceManager->isVoiceActive(channel2));
    EXPECT_TRUE(voiceManager->isVoiceActive(channel3));
    
    // Next allocation should reuse the highest available channel (channel 6)
    int newChannel = voiceManager->allocateVoice(72, 105);
    EXPECT_EQ(newChannel, 6);
    EXPECT_EQ(voiceManager->getNoteForChannel(6), 72);
}

TEST_F(VoiceManagerTest, ReleaseAllVoices) {
    // Allocate multiple voices - will be allocated to channels 7,6,5,4,3
    std::vector<int> allocatedChannels;
    for (int i = 0; i < 5; ++i) {
        int channel = voiceManager->allocateVoice(60 + i, 100 + i);
        allocatedChannels.push_back(channel);
    }
    
    // Verify voices are active in the allocated channels
    for (int channel : allocatedChannels) {
        EXPECT_TRUE(voiceManager->isVoiceActive(channel));
    }
    
    // Release all
    voiceManager->releaseAllVoices();
    
    // All voices should be inactive
    for (int ch = 0; ch < VoiceManager::MAX_VOICES; ++ch) {
        EXPECT_FALSE(voiceManager->isVoiceActive(ch));
    }
    
    // All notes should be unallocated
    for (int note = 60; note < 65; ++note) {
        EXPECT_EQ(voiceManager->getChannelForNote(note), -1);
    }
}

// =============================================================================
// 3. Note Retriggering Testing
// =============================================================================

TEST_F(VoiceManagerTest, NoteRetriggering) {
    // Allocate a voice
    int channel1 = voiceManager->allocateVoice(60, 100);
    EXPECT_EQ(channel1, 7); // First allocation goes to channel 7
    EXPECT_EQ(voiceManager->getVelocityForChannel(7), 100);
    
    // Retrigger same note with different velocity
    int channel2 = voiceManager->allocateVoice(60, 127);
    
    // Should return same channel with updated velocity
    EXPECT_EQ(channel2, 7);
    EXPECT_EQ(voiceManager->getVelocityForChannel(7), 127);
    EXPECT_EQ(voiceManager->getNoteForChannel(7), 60);
}

TEST_F(VoiceManagerTest, RetriggeringWithOtherVoicesActive) {
    // Allocate multiple voices
    int ch1 = voiceManager->allocateVoice(60, 100); // Channel 7
    int ch2 = voiceManager->allocateVoice(64, 110); // Channel 6
    int ch3 = voiceManager->allocateVoice(67, 90);  // Channel 5
    
    EXPECT_EQ(ch1, 7);
    EXPECT_EQ(ch2, 6);
    EXPECT_EQ(ch3, 5);
    
    // Retrigger first note
    int channel = voiceManager->allocateVoice(60, 127);
    EXPECT_EQ(channel, 7);
    EXPECT_EQ(voiceManager->getVelocityForChannel(7), 127);
    
    // Other voices should remain unchanged
    EXPECT_EQ(voiceManager->getNoteForChannel(6), 64);
    EXPECT_EQ(voiceManager->getVelocityForChannel(6), 110);
    EXPECT_EQ(voiceManager->getNoteForChannel(5), 67);
    EXPECT_EQ(voiceManager->getVelocityForChannel(5), 90);
}

// =============================================================================
// 4. Voice Stealing Testing
// =============================================================================

TEST_F(VoiceManagerTest, VoiceStealingOldestPolicy) {
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::OLDEST);
    
    // Fill all voices - channels will be allocated 7, 6, 5, 4, 3, 2, 1, 0
    // Notes will be 60, 61, 62, 63, 64, 65, 66, 67
    for (int i = 0; i < VoiceManager::MAX_VOICES; ++i) {
        voiceManager->allocateVoice(60 + i, 100);
    }
    
    // Add 9th note - should steal oldest (channel 7, which has note 60)
    int channel = voiceManager->allocateVoice(100, 120);
    EXPECT_EQ(channel, 7);
    EXPECT_EQ(voiceManager->getNoteForChannel(7), 100);
    EXPECT_EQ(voiceManager->getVelocityForChannel(7), 120);
    
    // Original note 60 should no longer be allocated
    EXPECT_EQ(voiceManager->getChannelForNote(60), -1);
    
    // Other voices should remain - check a few key ones
    EXPECT_EQ(voiceManager->getNoteForChannel(6), 61); // Second allocated
    EXPECT_EQ(voiceManager->getNoteForChannel(0), 67); // Last allocated
}

TEST_F(VoiceManagerTest, VoiceStealingQuietestPolicy) {
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::QUIETEST);
    
    // Fill all voices with different velocities - channels 7,6,5,4,3,2,1,0
    // Notes 60,61,62,63,64,65,66,67 with velocities below
    std::vector<uint8_t> velocities = {127, 100, 50, 80, 30, 90, 60, 110}; // Min: 30 at index 4
    for (int i = 0; i < VoiceManager::MAX_VOICES; ++i) {
        voiceManager->allocateVoice(60 + i, velocities[i]);
    }
    
    // Mapping: Note 64 (index 4) goes to channel 3, velocity 30 (quietest)
    // Add 9th note - should steal quietest (channel 3, velocity 30)
    int channel = voiceManager->allocateVoice(100, 120);
    EXPECT_EQ(channel, 3);
    EXPECT_EQ(voiceManager->getNoteForChannel(3), 100);
    EXPECT_EQ(voiceManager->getVelocityForChannel(3), 120);
    
    // Original note 64 should no longer be allocated
    EXPECT_EQ(voiceManager->getChannelForNote(64), -1);
}

TEST_F(VoiceManagerTest, VoiceStealingLowestPolicy) {
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::LOWEST);
    
    // Fill all voices with different notes - channels 7,6,5,4,3,2,1,0
    std::vector<uint8_t> notes = {72, 60, 80, 65, 55, 90, 70, 85}; // Min: 55 at index 4
    for (int i = 0; i < VoiceManager::MAX_VOICES; ++i) {
        voiceManager->allocateVoice(notes[i], 100);
    }
    
    // Note 55 (index 4) goes to channel 3 - should be the lowest note
    // Add 9th note - should steal lowest (channel 3, note 55)
    int channel = voiceManager->allocateVoice(100, 120);
    EXPECT_EQ(channel, 3);
    EXPECT_EQ(voiceManager->getNoteForChannel(3), 100);
    
    // Original note 55 should no longer be allocated
    EXPECT_EQ(voiceManager->getChannelForNote(55), -1);
}

TEST_F(VoiceManagerTest, ConsecutiveVoiceStealing) {
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::OLDEST);
    
    // Fill all voices - channels 7,6,5,4,3,2,1,0 with notes 60,61,62,63,64,65,66,67
    for (int i = 0; i < VoiceManager::MAX_VOICES; ++i) {
        voiceManager->allocateVoice(60 + i, 100);
    }
    
    // Add multiple notes beyond capacity - should steal oldest first (channel 7, then 6, then 5)
    int channel1 = voiceManager->allocateVoice(100, 120); // Should steal channel 7 (oldest)
    int channel2 = voiceManager->allocateVoice(101, 121); // Should steal channel 6 (next oldest)
    int channel3 = voiceManager->allocateVoice(102, 122); // Should steal channel 5 (next oldest)
    
    EXPECT_EQ(channel1, 7);
    EXPECT_EQ(channel2, 6);
    EXPECT_EQ(channel3, 5);
    
    EXPECT_EQ(voiceManager->getNoteForChannel(7), 100);
    EXPECT_EQ(voiceManager->getNoteForChannel(6), 101);
    EXPECT_EQ(voiceManager->getNoteForChannel(5), 102);
}

// =============================================================================
// 5. Noise Priority Voice Allocation Testing  
// =============================================================================

TEST_F(VoiceManagerTest, NoiseVoiceAllocationBasic) {
    // Test basic noise voice allocation
    int channel = voiceManager->allocateVoiceWithNoisePriority(60, 100, true);
    
    EXPECT_GE(channel, 0);
    EXPECT_LT(channel, VoiceManager::MAX_VOICES);
    EXPECT_TRUE(voiceManager->isVoiceActive(channel));
    EXPECT_EQ(voiceManager->getNoteForChannel(channel), 60);
    EXPECT_EQ(voiceManager->getVelocityForChannel(channel), 100);
}

TEST_F(VoiceManagerTest, NoiseVoiceAllocationWithoutNoise) {
    // Test allocation without noise priority (should behave like normal allocation)
    int channel1 = voiceManager->allocateVoiceWithNoisePriority(60, 100, false);
    int channel2 = voiceManager->allocateVoice(64, 110);
    
    // Both methods allocate in descending order starting from channel 7
    // Channel 7 is noise-capable but not noise-exclusive
    EXPECT_EQ(channel1, 7); // First allocation gets channel 7
    EXPECT_EQ(channel2, 6); // Second allocation gets channel 6 (7 is occupied)
}

// =============================================================================
// 6. Edge Cases and Error Handling
// =============================================================================

TEST_F(VoiceManagerTest, InvalidNoteNumbers) {
    // Test edge case note numbers
    int channel1 = voiceManager->allocateVoice(0, 100);   // Minimum valid
    int channel2 = voiceManager->allocateVoice(127, 100); // Maximum valid
    
    EXPECT_GE(channel1, 0);
    EXPECT_GE(channel2, 0);
    EXPECT_EQ(voiceManager->getNoteForChannel(channel1), 0);
    EXPECT_EQ(voiceManager->getNoteForChannel(channel2), 127);
}

TEST_F(VoiceManagerTest, InvalidVelocities) {
    // Test edge case velocities
    int channel1 = voiceManager->allocateVoice(60, 0);   // Minimum valid
    int channel2 = voiceManager->allocateVoice(64, 127); // Maximum valid
    
    EXPECT_GE(channel1, 0);
    EXPECT_GE(channel2, 0);
    EXPECT_EQ(voiceManager->getVelocityForChannel(channel1), 0);
    EXPECT_EQ(voiceManager->getVelocityForChannel(channel2), 127);
}

TEST_F(VoiceManagerTest, ReleaseNonexistentNote) {
    // Releasing a note that was never allocated should not crash
    EXPECT_NO_THROW(voiceManager->releaseVoice(60));
    
    // Allocate a voice then release a different note
    int channel = voiceManager->allocateVoice(60, 100);
    EXPECT_NO_THROW(voiceManager->releaseVoice(64)); // Different note
    
    // Original voice should still be active
    EXPECT_TRUE(voiceManager->isVoiceActive(channel));
    EXPECT_EQ(voiceManager->getNoteForChannel(channel), 60);
}

TEST_F(VoiceManagerTest, InvalidChannelQueries) {
    // Test queries for invalid channel numbers
    EXPECT_FALSE(voiceManager->isVoiceActive(-1));
    EXPECT_FALSE(voiceManager->isVoiceActive(VoiceManager::MAX_VOICES));
    EXPECT_EQ(voiceManager->getNoteForChannel(-1), 0);
    EXPECT_EQ(voiceManager->getNoteForChannel(VoiceManager::MAX_VOICES), 0);
    EXPECT_EQ(voiceManager->getVelocityForChannel(-1), 0);
    EXPECT_EQ(voiceManager->getVelocityForChannel(VoiceManager::MAX_VOICES), 0);
}

// =============================================================================
// 7. Complex Scenarios Testing
// =============================================================================

TEST_F(VoiceManagerTest, ComplexAllocationReleasePattern) {
    // Simulate complex musical pattern
    
    // Play chord (4 notes)
    std::vector<uint8_t> chord1 = {60, 64, 67, 72};
    for (uint8_t note : chord1) {
        voiceManager->allocateVoice(note, 100);
    }
    
    // Add melody notes (another 4 notes)
    std::vector<uint8_t> melody = {74, 76, 77, 79};
    for (uint8_t note : melody) {
        voiceManager->allocateVoice(note, 80);
    }
    
    // Now at max capacity (8 voices)
    for (int ch = 0; ch < VoiceManager::MAX_VOICES; ++ch) {
        EXPECT_TRUE(voiceManager->isVoiceActive(ch));
    }
    
    // Release chord
    for (uint8_t note : chord1) {
        voiceManager->releaseVoice(note);
    }
    
    // Should have 4 free channels
    int activeCount = 0;
    for (int ch = 0; ch < VoiceManager::MAX_VOICES; ++ch) {
        if (voiceManager->isVoiceActive(ch)) {
            activeCount++;
        }
    }
    EXPECT_EQ(activeCount, 4);
    
    // Play new chord
    std::vector<uint8_t> chord2 = {48, 52, 55, 59};
    for (uint8_t note : chord2) {
        int channel = voiceManager->allocateVoice(note, 110);
        EXPECT_GE(channel, 0); // Should reuse freed channels
    }
}

TEST_F(VoiceManagerTest, PolicySwitchingBehavior) {
    // Fill all voices - channels 7,6,5,4,3,2,1,0 with notes 60,61,62,63,64,65,66,67
    // velocities 100,90,80,70,60,50,40,30
    for (int i = 0; i < VoiceManager::MAX_VOICES; ++i) {
        voiceManager->allocateVoice(60 + i, 100 - i * 10);
    }
    
    // Switch policies and test stealing behavior
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::OLDEST);
    int channel1 = voiceManager->allocateVoice(100, 120);
    EXPECT_EQ(channel1, 7); // Should steal oldest (channel 7)
    
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::QUIETEST);
    int channel2 = voiceManager->allocateVoice(101, 121);
    EXPECT_EQ(channel2, 0); // Should steal quietest (channel 0, velocity 30)
    
    voiceManager->setStealingPolicy(VoiceManager::StealingPolicy::LOWEST);
    int channel3 = voiceManager->allocateVoice(102, 122);
    // Should steal lowest remaining note (note 61 at channel 6)
    EXPECT_EQ(channel3, 6);
}