#include <gtest/gtest.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "utils/PresetManager.h"

// Basic test to verify test framework is working
TEST(BasicTest, SanityCheck) {
    EXPECT_EQ(2 + 2, 4);
    EXPECT_TRUE(true);
}

namespace {
// Every PresetManager in the test process reads and writes its banks and user presets under a
// throw-away directory, so a test that imports a bank never touches the user's own data.
struct ScopedTestUserData {
    juce::File directory;
    ScopedTestUserData()
    {
        directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("YMulatorSynthTests-" + juce::String(juce::Time::currentTimeMillis()));
        directory.createDirectory();
        ymulatorsynth::PresetManager::setUserDataDirectoryOverride(directory);
    }
    ~ScopedTestUserData()
    {
        ymulatorsynth::PresetManager::setUserDataDirectoryOverride(juce::File());
        directory.deleteRecursively();
    }
};
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // GUI classes (editors, timers, tooltip windows) need JUCE's GUI subsystem and a message manager;
    // macOS tolerates their absence, Linux does not
    juce::ScopedJuceInitialiser_GUI juceInit;
    ScopedTestUserData userData;
    return RUN_ALL_TESTS();
}
