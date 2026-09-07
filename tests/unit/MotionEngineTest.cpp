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

TEST_F(MotionEngineTest, TimbreLfoMovesModulatorsOnly)
{
    set(ParamID::Global::Algorithm, 4.0f);            // carriers op2, op4
    for (int op = 1; op <= 4; ++op) set(ParamID::Op::tl(op).c_str(), 50.0f);
    set(ParamID::Motion::TimbreDepth, 40.0f);
    set(ParamID::Motion::TimbreRate, 4.0f);
    runBlocks(1);
    const int ch = noteOn();
    auto tl = [&](int op) { return static_cast<int>(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[op] + ch)); };
    int lo = 200, hi = -1;
    for (int i = 0; i < 30; ++i) {
        runBlocks(1);
        lo = std::min(lo, tl(0)); hi = std::max(hi, tl(0));
        EXPECT_EQ(tl(1), 50) << "carrier untouched";
        EXPECT_EQ(tl(3), 50) << "carrier untouched";
        EXPECT_EQ(tl(0), tl(2)) << "both modulators move together";
    }
    // Registers are sampled once per 512-sample block, so the triangle's tips can be missed by a few steps
    EXPECT_LE(lo, 14) << "swings about 40 steps brighter";
    EXPECT_GE(hi, 86) << "and about 40 steps darker";
}

TEST_F(MotionEngineTest, TremoloAttenuatesCarriersOnTopOfVelocity)
{
    set(ParamID::Global::Algorithm, 4.0f);
    for (int op = 1; op <= 4; ++op) set(ParamID::Op::tl(op).c_str(), 50.0f);
    set(ParamID::Motion::TremoloDepth, 24.0f);
    set(ParamID::Motion::TremoloRate, 4.0f);
    runBlocks(1);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(64)), 0);   // velocity 64: +16
    runBlocks(1, &midi);
    const int ch = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    auto tl = [&](int op) { return static_cast<int>(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[op] + ch)); };
    int lo = 200, hi = -1;
    for (int i = 0; i < 30; ++i) {
        runBlocks(1);
        lo = std::min(lo, tl(1)); hi = std::max(hi, tl(1));
        EXPECT_EQ(tl(0), 50) << "modulator untouched";
        EXPECT_EQ(tl(1), tl(3));
    }
    EXPECT_EQ(lo, 66) << "loudest point: parameter TL 50 + velocity 16";
    EXPECT_EQ(hi, 90) << "quietest point adds the full 24 steps";
}

TEST_F(MotionEngineTest, PitchEnvelopeSlidesOntoTheNote)
{
    set(ParamID::Motion::PitchEnv, -100.0f);          // start a semitone low
    set(ParamID::Motion::PitchTime, 200.0f);
    const int ch = noteOn();
    const float atStart = processor.getMotionEngine().currentOffset(ch);
    EXPECT_LT(atStart, -0.85f) << "one block in, still close to the start offset";
    runBlocks(8);                                     // about 0.1 s: halfway
    const float halfway = processor.getMotionEngine().currentOffset(ch);
    EXPECT_GT(halfway, -0.7f);
    EXPECT_LT(halfway, -0.3f);
    runBlocks(12);                                    // past 0.2 s
    EXPECT_FLOAT_EQ(processor.getMotionEngine().currentOffset(ch), 0.0f);
    EXPECT_EQ(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_FRACTION_BASE + ch), 0) << "settled exactly on the note";
}

TEST_F(MotionEngineTest, PitchEnvelopeAndVibratoAddUp)
{
    set(ParamID::Motion::PitchEnv, 100.0f);
    set(ParamID::Motion::PitchTime, 500.0f);
    set(ParamID::Motion::VibratoDepth, 100.0f);
    set(ParamID::Motion::VibratoRate, 4.0f);
    set(ParamID::Motion::VibratoDelay, 0.0f);
    set(ParamID::Motion::VibratoRise, 0.0f);
    const int ch = noteOn();
    const auto [lo, hi] = offsetRange(ch, 12);        // first 0.13 s: envelope still above +0.7
    EXPECT_GT(hi, 1.2f) << "envelope plus vibrato peak";
    EXPECT_GT(lo, 0.2f) << "envelope keeps the trough above the note";
}

TEST_F(MotionEngineTest, SweepOpensTheModulatorsOverTime)
{
    set(ParamID::Global::Algorithm, 4.0f);
    for (int op = 1; op <= 4; ++op) set(ParamID::Op::tl(op).c_str(), 30.0f);
    set(ParamID::Motion::SweepAmount, 40.0f);         // starts 40 steps darker
    set(ParamID::Motion::SweepTime, 500.0f);
    runBlocks(1);
    const int ch = noteOn();
    auto tl = [&](int op) { return static_cast<int>(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[op] + ch)); };
    EXPECT_GE(tl(0), 66) << "just after the key-on the modulators are near the full offset";
    EXPECT_EQ(tl(1), 30) << "carriers untouched";
    runBlocks(23);                                    // about 0.25 s: half way, eased
    const int midway = tl(0);
    EXPECT_GT(midway, 30);
    EXPECT_LT(midway, 66);
    runBlocks(30);                                    // past 0.5 s
    EXPECT_EQ(tl(0), 30) << "settled on the patch";
    EXPECT_EQ(tl(2), 30);
}

TEST_F(MotionEngineTest, TwoStagePitchEnvelopeForDrums)
{
    set(ParamID::Motion::PitchEnv, 1200.0f);          // an octave up at the key-on
    set(ParamID::Motion::PitchTime, 20.0f);
    set(ParamID::Motion::PitchEnv2, -500.0f);         // then below the note
    set(ParamID::Motion::PitchTime2, 150.0f);
    const int ch = noteOn(48);
    float first = processor.getMotionEngine().currentOffset(ch);
    EXPECT_GT(first, 0.0f) << "still above the note after the first block (10 ms of 20)";
    runBlocks(2);                                     // about 32 ms: past the first stage, near -5
    const float dip = processor.getMotionEngine().currentOffset(ch);
    EXPECT_LT(dip, -3.5f);
    EXPECT_GT(dip, -5.5f);
    runBlocks(6);                                     // about 96 ms: rising back
    const float rising = processor.getMotionEngine().currentOffset(ch);
    EXPECT_GT(rising, dip);
    EXPECT_LT(rising, 0.0f);
    runBlocks(10);                                    // past 170 ms
    EXPECT_FLOAT_EQ(processor.getMotionEngine().currentOffset(ch), 0.0f);
}

TEST_F(MotionEngineTest, LevelEnvelopeSwellsThenFallsToSustain)
{
    set(ParamID::Global::Algorithm, 4.0f);
    for (int op = 1; op <= 4; ++op) set(ParamID::Op::tl(op).c_str(), 20.0f);
    set(ParamID::Motion::LevelAttack, 200.0f);
    set(ParamID::Motion::LevelDecay, 200.0f);
    set(ParamID::Motion::LevelSustain, 12.0f);
    runBlocks(1);
    const int ch = noteOn();
    auto tl = [&](int op) { return static_cast<int>(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[op] + ch)); };
    EXPECT_GE(tl(1), 50) << "swell starts about 40 steps down";
    EXPECT_EQ(tl(0), 20) << "modulators untouched";
    runBlocks(18);                                    // about 0.2 s: attack done
    EXPECT_LE(tl(1), 22);
    runBlocks(20);                                    // about 0.41 s: decayed to the sustain attenuation
    EXPECT_EQ(tl(1), 20 + 12);
    EXPECT_EQ(tl(3), 20 + 12);
}

TEST_F(MotionEngineTest, SawOneShotVibratoRampsUpAndHolds)
{
    set(ParamID::Motion::VibratoDepth, 100.0f);
    set(ParamID::Motion::VibratoRate, 4.0f);
    set(ParamID::Motion::VibratoDelay, 0.0f);
    set(ParamID::Motion::VibratoRise, 0.0f);
    set(ParamID::Motion::VibratoWave, 2.0f);          // saw
    set(ParamID::Motion::LfoOneShot, 1.0f);
    const int ch = noteOn();
    const float start = processor.getMotionEngine().currentOffset(ch);
    EXPECT_LT(start, -0.4f) << "a rising saw begins at -depth";
    runBlocks(10);                                    // 0.11 s: halfway through the 0.25 s cycle
    const float mid = processor.getMotionEngine().currentOffset(ch);
    EXPECT_GT(mid, start);
    runBlocks(30);                                    // well past one cycle
    EXPECT_NEAR(processor.getMotionEngine().currentOffset(ch), 0.5f, 0.02f) << "holds at +depth";
    runBlocks(10);
    EXPECT_NEAR(processor.getMotionEngine().currentOffset(ch), 0.5f, 0.02f);
}

TEST_F(MotionEngineTest, RandomVibratoHoldsOneValuePerCycle)
{
    set(ParamID::Motion::VibratoDepth, 100.0f);
    set(ParamID::Motion::VibratoRate, 2.0f);          // 0.5 s per cycle
    set(ParamID::Motion::VibratoDelay, 0.0f);
    set(ParamID::Motion::VibratoRise, 0.0f);
    set(ParamID::Motion::VibratoWave, 4.0f);          // random
    const int ch = noteOn();
    const float a = processor.getMotionEngine().currentOffset(ch);
    runBlocks(10);                                    // still inside the first cycle
    EXPECT_FLOAT_EQ(processor.getMotionEngine().currentOffset(ch), a);
    runBlocks(40);                                    // next cycle
    const float b = processor.getMotionEngine().currentOffset(ch);
    EXPECT_NE(a, b);
    EXPECT_LE(std::abs(b), 0.5f);
}

TEST_F(MotionEngineTest, SweepAndTimbreLfoAddUp)
{
    // Sweep starts the modulators 40 steps back; a square timbre LFO adds its 20 on top during its high half
    set(ParamID::Motion::SweepAmount, 40.0f);
    set(ParamID::Motion::SweepTime, 4000.0f);
    set(ParamID::Motion::TimbreDepth, 20.0f);
    set(ParamID::Motion::TimbreRate, 0.1f);
    set(ParamID::Motion::TimbreWave, 3.0f);         // square: +1 for the first half cycle
    runBlocks(1);
    const int ch = noteOn();
    EXPECT_GE(processor.getMotionEngine().currentModulatorSteps(ch), 55) << "sweep (about 40) plus timbre LFO (20)";
}

TEST_F(MotionEngineTest, LevelEgAndTremoloAddUp)
{
    // A sustain-only level EG holds the carriers 20 steps back; the tremolo dip adds up to 24 more
    set(ParamID::Motion::LevelSustain, 20.0f);
    set(ParamID::Motion::TremoloDepth, 24.0f);
    set(ParamID::Motion::TremoloRate, 0.5f);         // dip peaks after one second
    runBlocks(1);
    const int ch = noteOn();
    int peak = 0;
    for (int i = 0; i < 100; ++i) { runBlocks(1); peak = std::max(peak, processor.getMotionEngine().currentCarrierSteps(ch)); }
    EXPECT_GE(peak, 40) << "level EG (20) plus the tremolo dip (24)";
    EXPECT_LE(peak, 44);
}
