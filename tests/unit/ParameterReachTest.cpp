#include <gtest/gtest.h>
#include <array>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"

// Every sound parameter must change at least one chip register when it moves,
// so a parameter can never be exposed to the host and then silently ignored.
class ParameterReachTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
        runBlock();
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    void runBlock(juce::MidiBuffer* midi = nullptr) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer empty;
        processor.processBlock(buffer, midi ? *midi : empty);
    }
    std::array<uint8_t, 256> registers() {
        std::array<uint8_t, 256> r {};
        for (int a = 0; a < 256; ++a) r[static_cast<size_t>(a)] = processor.getYmfmWrapper().readCurrentRegister(a);
        return r;
    }
    YMulatorSynthAudioProcessor processor;
};

TEST_F(ParameterReachTest, EveryParameterChangesARegister)
{
    std::vector<std::string> unreached;
    for (auto* param : processor.getParameters().processor.getParameters()) {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (ranged == nullptr) continue;
        const std::string id = ranged->paramID.toStdString();
        if (id == ParamID::Global::PitchBendRange) continue;      // only audible with a pitch bend (PitchAccuracyTest)
        if (id == ParamID::Global::LfoAmd) continue;              // shares 0x19 with PMD; LfoWiringTest hears it
        if (id.find("_slot_en") != std::string::npos) continue;   // key-on register only (SlotEnableTest)
        
        const float original = ranged->getValue();
        const auto before = registers();
        // Move to the far end of the range from where it is
        const float target = original < 0.5f ? 1.0f : 0.0f;
        ranged->setValueNotifyingHost(target);
        runBlock();
        const bool changed = registers() != before;
        ranged->setValueNotifyingHost(original);
        runBlock();
        if (!changed) unreached.push_back(id);
    }
    EXPECT_TRUE(unreached.empty()) << "parameters that never reach the chip: " << [&]{ std::string s; for (auto& u : unreached) s += u + " "; return s; }();
}
