#include <gtest/gtest.h>
#include <cmath>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"

// Delayed per-voice vibrato written to KC/KF at control rate.
class MotionEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
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
    /** Runs `blocks` and returns the extreme offsets seen. */
    std::pair<float, float> offsetRange(int channel, int blocks) {
        float lo = 1.0f, hi = -1.0f;
        for (int i = 0; i < blocks; ++i) {
            runBlocks(1);
            const float o = processor.getMotionEngine().currentOffset(channel);
            lo = std::min(lo, o); hi = std::max(hi, o);
        }
        return { lo, hi };
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(MotionEngineTest, NoVibratoLeavesPitchRegistersAlone)
{
    const int ch = noteOn();
    const auto kf = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_FRACTION_BASE + ch);
    const auto writes = processor.getYmfmWrapper().getRegisterWriteCount();
    runBlocks(50);
    EXPECT_EQ(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_FRACTION_BASE + ch), kf);
    EXPECT_EQ(processor.getYmfmWrapper().getRegisterWriteCount(), writes);
    EXPECT_FLOAT_EQ(processor.getMotionEngine().currentOffset(ch), 0.0f);
}

TEST_F(MotionEngineTest, VibratoSwingsAroundTheNote)
{
    set(ParamID::Motion::VibratoDepth, 100.0f);   // +/- 50 cents
    set(ParamID::Motion::VibratoRate, 4.0f);
    set(ParamID::Motion::VibratoDelay, 0.0f);
    set(ParamID::Motion::VibratoRise, 0.0f);
    const int ch = noteOn();
    const auto kcBefore = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_CODE_BASE + ch);
    
    const auto [lo, hi] = offsetRange(ch, 24);   // about 0.26 s: one full cycle at 4 Hz
    EXPECT_NEAR(hi, 0.5f, 0.03f);
    EXPECT_NEAR(lo, -0.5f, 0.03f);
    
    // A negative offset crosses into the semitone below: KC must have moved at some point
    bool kcChanged = false;
    for (int i = 0; i < 24; ++i) {
        runBlocks(1);
        if (processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_CODE_BASE + ch) != kcBefore) kcChanged = true;
    }
    EXPECT_TRUE(kcChanged);
}

TEST_F(MotionEngineTest, DelayAndRiseShapeTheOnset)
{
    set(ParamID::Motion::VibratoDepth, 100.0f);
    set(ParamID::Motion::VibratoRate, 8.0f);
    set(ParamID::Motion::VibratoDelay, 500.0f);
    set(ParamID::Motion::VibratoRise, 1000.0f);
    const int ch = noteOn();
    
    const auto during = offsetRange(ch, 40);        // first 0.43 s: inside the delay
    EXPECT_FLOAT_EQ(during.first, 0.0f);
    EXPECT_FLOAT_EQ(during.second, 0.0f);
    
    runBlocks(10);                                  // past the delay
    const auto early = offsetRange(ch, 20);         // 0.53-0.75 s: depth still rising
    const auto late = offsetRange(ch, 90);          // up to about 1.7 s: fully risen
    EXPECT_GT(early.second, 0.02f);
    EXPECT_LT(early.second, 0.35f);
    EXPECT_NEAR(late.second, 0.5f, 0.03f);
}

TEST_F(MotionEngineTest, EachNoteStartsItsOwnDelay)
{
    set(ParamID::Motion::VibratoDepth, 100.0f);
    set(ParamID::Motion::VibratoDelay, 300.0f);
    set(ParamID::Motion::VibratoRise, 0.0f);
    const int first = noteOn(60);
    runBlocks(40);                                   // 0.43 s: first note vibrates
    EXPECT_NE(processor.getMotionEngine().currentOffset(first), 0.0f);
    const int second = noteOn(64);
    ASSERT_NE(second, first);
    const auto fresh = offsetRange(second, 20);      // 0.21 s: still inside the second note's delay
    EXPECT_FLOAT_EQ(fresh.first, 0.0f);
    EXPECT_FLOAT_EQ(fresh.second, 0.0f);
}
