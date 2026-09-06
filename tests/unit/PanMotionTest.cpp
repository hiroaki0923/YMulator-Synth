#include <gtest/gtest.h>
#include <cmath>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"

namespace {
// A transport that reports a tempo and advances with the samples the test renders
class TestPlayHead : public juce::AudioPlayHead {
public:
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setBpm(bpm);
        info.setPpqPosition(ppq);
        info.setIsPlaying(playing);
        return info;
    }
    void advance(int samples, double sampleRate) { if (playing) ppq += samples / (sampleRate * 60.0 / bpm); }
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
};
}

class PanMotionTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
        processor.setPlayHead(&playHead);
        runBlocks(1);
    }
    void TearDown() override {
        processor.setPlayHead(nullptr);
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
        for (int i = 0; i < n; ++i) {
            processor.processBlock(buffer, midi ? *midi : empty);
            if (midi) midi->clear();
            playHead.advance(512, 48000.0);
        }
    }
    int noteOn(int note) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(127)), 0);
        runBlocks(1, &midi);
        return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    }
    int panBits(int ch) { return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + ch) & YM2151Regs::MASK_PAN_LR; }
    
    YMulatorSynthAudioProcessor processor;
    TestPlayHead playHead;
};

TEST_F(PanMotionTest, AlternateSendsSuccessiveNotesLeftAndRight)
{
    set(ParamID::Motion::PanMode, 1.0f);
    const int a = noteOn(60), b = noteOn(64), c = noteOn(67);
    EXPECT_EQ(panBits(a), YM2151Regs::PAN_LEFT_ONLY);
    EXPECT_EQ(panBits(b), YM2151Regs::PAN_RIGHT_ONLY);
    EXPECT_EQ(panBits(c), YM2151Regs::PAN_LEFT_ONLY);
}

TEST_F(PanMotionTest, StepFollowsTheHostBeat)
{
    set(ParamID::Motion::PanMode, 2.0f);
    set(ParamID::Motion::PanRate, 2.0f);           // 1/4: one step per beat = 0.5 s at 120 BPM
    playHead.ppq = 0.0;
    const int ch = noteOn(60);
    std::vector<int> seen;
    for (int step = 0; step < 4; ++step) {
        // sample in the middle of each beat: 0.25 s, 0.75 s, ...
        while (playHead.ppq < step + 0.5) runBlocks(1);
        seen.push_back(panBits(ch));
    }
    const std::vector<int> expected { YM2151Regs::PAN_LEFT_ONLY, YM2151Regs::PAN_CENTER, YM2151Regs::PAN_RIGHT_ONLY, YM2151Regs::PAN_CENTER };
    EXPECT_EQ(seen, expected);
}

TEST_F(PanMotionTest, OffPutsTheGlobalPanBack)
{
    set(ParamID::Motion::PanMode, 1.0f);
    const int ch = noteOn(60);
    ASSERT_EQ(panBits(ch), YM2151Regs::PAN_LEFT_ONLY);
    set(ParamID::Motion::PanMode, 0.0f);
    runBlocks(1);
    EXPECT_EQ(panBits(ch), YM2151Regs::PAN_CENTER);
}

TEST_F(PanMotionTest, WideLeftRightWinsOverPanMotion)
{
    set(ParamID::Motion::PanMode, 1.0f);
    set(ParamID::Motion::Wide, 50.0f);
    set(ParamID::Motion::WidePan, 0.0f);
    runBlocks(1);
    const int ch = noteOn(60);
    EXPECT_EQ(panBits(ch), YM2151Regs::PAN_CENTER) << "parameter view stays centre; the chips are split by Wide";
}

TEST_F(PanMotionTest, SyncedVibratoRunsAtTheTempo)
{
    set(ParamID::Motion::VibratoDepth, 100.0f);
    set(ParamID::Motion::VibratoDelay, 0.0f);
    set(ParamID::Motion::VibratoRise, 0.0f);
    set(ParamID::Motion::Sync, 1.0f);
    set(ParamID::Motion::VibratoDiv, 2.0f);         // 1/4: one cycle per beat
    auto crossingsPerSecond = [&](double bpm) {
        playHead.bpm = bpm;
        playHead.ppq = 0.0;
        const int ch = noteOn(69);
        int crossings = 0;
        float previous = 0.0f;
        const int blocks = 48000 / 512;              // one second
        for (int i = 0; i < blocks; ++i) {
            runBlocks(1);
            const float o = processor.getMotionEngine().currentOffset(ch);
            if ((previous < 0.0f && o >= 0.0f) || (previous > 0.0f && o <= 0.0f)) ++crossings;
            previous = o;
        }
        juce::MidiBuffer off;
        off.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
        runBlocks(1, &off);
        runBlocks(10);
        return crossings;
    };
    EXPECT_NEAR(crossingsPerSecond(120.0), 4, 1);   // 2 Hz
    EXPECT_NEAR(crossingsPerSecond(60.0), 2, 1);    // 1 Hz
}
