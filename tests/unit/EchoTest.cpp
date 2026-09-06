#include <gtest/gtest.h>
#include <cmath>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"

// Echo: the shadow chip repeats each note after the delay, carriers attenuated.
class EchoTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
        set(ParamID::Global::Algorithm, 4.0f);
        for (int op = 1; op <= 4; ++op) set(ParamID::Op::tl(op).c_str(), 20.0f);
        runBlocks(1);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    void set(const char* id, float value) {
        auto* p = processor.getParameters().getParameter(id);
        ASSERT_NE(p, nullptr);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    }
    void runBlocks(int n, juce::MidiBuffer* midi = nullptr) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer empty;
        for (int i = 0; i < n; ++i) { processor.processBlock(buffer, midi ? *midi : empty); if (midi) midi->clear(); }
    }
    int noteOn(int note = 69) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(127)), 0);
        runBlocks(1, &midi);
        return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    }
    uint8_t main(int a) { return processor.getYmfmWrapper().readCurrentRegister(a); }
    uint8_t shadow(int a) { return processor.getYmfmWrapper().readShadowRegister(a); }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(EchoTest, ShadowKeysOnAfterTheDelayWithQuieterCarriers)
{
    set(ParamID::Motion::EchoLevel, 50.0f);        // 16 steps quieter
    set(ParamID::Motion::EchoTime, 100.0f);
    runBlocks(1);
    EXPECT_TRUE(processor.getYmfmWrapper().isEchoEnabled());
    const int ch = noteOn(69);
    const int keyOn = main(YM2151Regs::REG_KEY_ON_OFF);
    EXPECT_NE(shadow(YM2151Regs::REG_KEY_ON_OFF), keyOn) << "one block in (10 ms), the shadow has not keyed on";
    runBlocks(12);                                  // 138 ms
    EXPECT_EQ(shadow(YM2151Regs::REG_KEY_ON_OFF), keyOn);
    EXPECT_EQ(shadow(YM2151Regs::REG_KEY_CODE_BASE + ch), main(YM2151Regs::REG_KEY_CODE_BASE + ch));
    // carriers of algorithm 4 are op2 and op4
    EXPECT_EQ(shadow(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[1] + ch), 20 + 16);
    EXPECT_EQ(shadow(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[0] + ch), 20) << "modulators keep the timbre";
}

TEST_F(EchoTest, EchoIsAudibleOnTheOtherSideAndStopsWhenOff)
{
    auto render = [&](int blocks) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        double l = 0.0, r = 0.0;
        for (int b = 0; b < blocks; ++b) {
            processor.processBlock(buffer, midi);
            l += buffer.getMagnitude(0, 0, 512);
            r += buffer.getMagnitude(1, 0, 512);
        }
        return std::make_pair(l / blocks, r / blocks);
    };
    set(ParamID::Motion::EchoLevel, 80.0f);
    set(ParamID::Motion::EchoTime, 150.0f);
    set(ParamID::Motion::WidePan, 0.0f);            // note centred, first echo right
    runBlocks(1);
    noteOn(69);
    const auto early = render(8);                    // 10-95 ms: only the note, on both sides
    EXPECT_GT(early.first, 0.02);
    EXPECT_NEAR(early.first, early.second, 1e-4) << "the note itself stays centred";
    runBlocks(6);                                    // past 150 ms
    const auto late = render(6);
    EXPECT_GT(late.second, late.first + 0.005) << "the echo adds on the right: L " << late.first << " R " << late.second;
    
    set(ParamID::Motion::EchoLevel, 0.0f);
    runBlocks(1);
    EXPECT_FALSE(processor.getYmfmWrapper().isEchoEnabled());
    for (int a = 0x08; a < 0x100; ++a)
        if (a < 0x20 || a >= 0x28) EXPECT_EQ(main(a), shadow(a)) << "register " << a << " mirrors again";
}

TEST_F(EchoTest, EchoesTakeSidesInTurnWhileTheNoteKeepsItsPan)
{
    set(ParamID::Motion::EchoLevel, 50.0f);
    set(ParamID::Motion::EchoTime, 50.0f);
    set(ParamID::Motion::WidePan, 0.0f);
    runBlocks(1);
    const int first = noteOn(69);
    const int second = noteOn(72);
    runBlocks(8);                                    // past 50 ms: the pans have reached the shadow
    const int panFirst = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + first;
    const int panSecond = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + second;
    EXPECT_EQ(main(panFirst) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_CENTER) << "the note keeps the global pan";
    EXPECT_EQ(shadow(panFirst) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_RIGHT_ONLY);
    EXPECT_EQ(shadow(panSecond) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_LEFT_ONLY) << "the next echo takes the other side";
    EXPECT_EQ(shadow(panFirst) & ~YM2151Regs::MASK_PAN_LR, main(panFirst) & ~YM2151Regs::MASK_PAN_LR) << "algorithm and feedback still mirror";
}

TEST_F(EchoTest, SyncedEchoFollowsTheTempoDivision)
{
    set(ParamID::Motion::EchoLevel, 60.0f);
    set(ParamID::Motion::Sync, 1.0f);
    set(ParamID::Motion::EchoDiv, 3.0f);             // 1/8 at 120 BPM (no transport: free-run tempo) = 250 ms
    runBlocks(1);
    noteOn(69);
    const int keyOn = main(YM2151Regs::REG_KEY_ON_OFF);
    runBlocks(20);                                   // 224 ms since the note
    EXPECT_NE(shadow(YM2151Regs::REG_KEY_ON_OFF), keyOn);
    runBlocks(4);                                    // 267 ms
    EXPECT_EQ(shadow(YM2151Regs::REG_KEY_ON_OFF), keyOn);
}
