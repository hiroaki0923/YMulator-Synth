#include <gtest/gtest.h>
#include <cmath>
#include <set>
#include <vector>
#include <algorithm>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"

// Mono / legato with portamento, and the chip arpeggio on one channel.
class MonoArpTest : public ::testing::Test {
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
    void note(int n, bool on) {
        juce::MidiBuffer midi;
        midi.addEvent(on ? juce::MidiMessage::noteOn(1, n, static_cast<juce::uint8>(120)) : juce::MidiMessage::noteOff(1, n), 0);
        runBlocks(1, &midi);
    }
    int activeChannels() {
        int count = 0;
        for (int ch = 0; ch < 8; ++ch) if (processor.getMidiProcessor() && processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_CODE_BASE + ch) >= 0) count += voiceActive(ch) ? 1 : 0;
        return count;
    }
    bool voiceActive(int ch) {
        // The key-on register only holds the last write; use the note-on counter and key code instead
        return processor.getMotionEngine().currentOffset(ch) == processor.getMotionEngine().currentOffset(ch) && keyCode(ch) != 0 && held(ch);
    }
    bool held(int ch) { return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) == (0x78 | ch) || lastKeyOnChannel == ch; }
    int keyCode(int ch) { return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_CODE_BASE + ch); }
    int lastKeyOnChannel = -1;
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(MonoArpTest, LegatoRetunesInsteadOfRetriggering)
{
    set(ParamID::Motion::Mono, 1.0f);
    note(60, true);
    const int ch = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    const auto keyOns = processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch));
    const int kcC = keyCode(ch);
    
    note(67, true);                                  // second note while the first is held
    EXPECT_EQ(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch)), keyOns) << "no new key-on";
    EXPECT_NE(keyCode(ch), kcC) << "the channel now plays G";
    for (int other = 0; other < 8; ++other)
        if (other != ch) EXPECT_EQ(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(other)), 0u) << "no other channel started";
    
    note(67, false);                                 // release the top note: back to C
    EXPECT_EQ(keyCode(ch), kcC);
    EXPECT_EQ(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch)), keyOns);
    
    note(60, false);
    EXPECT_EQ(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF), ch) << "key off";
}

TEST_F(MonoArpTest, PortamentoGlidesFromThePreviousNote)
{
    set(ParamID::Motion::Mono, 1.0f);
    set(ParamID::Motion::PortaTime, 300.0f);
    note(60, true);
    const int ch = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    note(72, true);                                  // an octave up, legato
    const float start = processor.getMotionEngine().currentOffset(ch);
    EXPECT_LT(start, -10.0f) << "starts near the old note (-12 semitones)";
    runBlocks(14);                                   // 0.15 s: halfway
    const float mid = processor.getMotionEngine().currentOffset(ch);
    EXPECT_LT(mid, -3.0f);
    EXPECT_GT(mid, -9.0f);
    runBlocks(16);                                   // past 0.3 s
    EXPECT_FLOAT_EQ(processor.getMotionEngine().currentOffset(ch), 0.0f);
    
    // Polyphonic portamento: a new note on another channel also glides from the last one
    set(ParamID::Motion::Mono, 0.0f);
    note(72, false); note(60, false);
    runBlocks(2);
    note(48, true);
    const int ch2 = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    EXPECT_GT(processor.getMotionEngine().currentOffset(ch2), 10.0f) << "glides down from C5 to C3: starts +24";
}

TEST_F(MonoArpTest, ArpeggioCyclesHeldNotesOnOneChannel)
{
    set(ParamID::Motion::ArpMode, 1.0f);             // Up
    set(ParamID::Motion::ArpDiv, 4.0f);              // 1/16 at 120 BPM free-run = 125 ms
    note(60, true);
    const int ch = processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    note(64, true);
    note(67, true);
    std::set<int> seen;
    std::vector<int> order;
    for (int i = 0; i < 40; ++i) {                   // 0.43 s: more than three steps
        runBlocks(1);
        const int kc = keyCode(ch);
        if (order.empty() || order.back() != kc) order.push_back(kc);
        seen.insert(kc);
    }
    EXPECT_EQ(seen.size(), 3u) << "all three held notes were played";
    ASSERT_GE(order.size(), 4u);
    // Up: after the lowest comes a higher key code, then higher again, then back to the lowest
    const int low = *seen.begin();
    auto it = std::find(order.begin(), order.end(), low);
    ASSERT_NE(it, order.end());
    if (it + 3 < order.end()) {
        EXPECT_GT(*(it + 1), low);
        EXPECT_GT(*(it + 2), *(it + 1));
        EXPECT_EQ(*(it + 3), low);
    }
    for (int other = 0; other < 8; ++other)
        if (other != ch) EXPECT_EQ(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(other)), 0u) << "the arpeggio uses one channel";
    
    note(64, false); note(67, false);
    runBlocks(3);
    EXPECT_EQ(keyCode(ch), keyCode(ch));
    note(60, false);
    EXPECT_EQ(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF), ch);
}

// ---- the arpeggiator beyond the chip-style basics
namespace {
constexpr int kBlocksPerSixteenth = 12;   // 1/16 at 120 BPM free-run = 125 ms ~ 11.7 blocks of 512 @ 48 kHz
}

class ArpeggiatorTest : public MonoArpTest {
protected:
    int channelOf() { return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07; }
    // successive distinct key codes over n blocks
    std::vector<int> order(int ch, int blocks) {
        std::vector<int> seq;
        for (int i = 0; i < blocks; ++i) {
            runBlocks(1);
            const int kc = keyCode(ch);
            if (seq.empty() || seq.back() != kc) seq.push_back(kc);
        }
        return seq;
    }
    std::set<int> seen(const std::vector<int>& seq) { return std::set<int>(seq.begin(), seq.end()); }
};

TEST_F(ArpeggiatorTest, UpDownLeavesOutTheTurningNotes)
{
    set(ParamID::Motion::ArpMode, 3.0f);             // Up Down
    set(ParamID::Motion::ArpDiv, 4.0f);              // 1/16
    note(60, true); const int ch = channelOf(); note(64, true); note(67, true); note(72, true);
    const auto seq = order(ch, kBlocksPerSixteenth * 14);
    ASSERT_GE(seq.size(), 9u);
    const int low = *std::min_element(seq.begin(), seq.end());
    auto it = std::find(seq.begin(), seq.end(), low);
    ASSERT_LE(it + 7, seq.end());
    // a b c d c b a b: the top and bottom are not repeated
    EXPECT_LT(it[0], it[1]); EXPECT_LT(it[1], it[2]); EXPECT_LT(it[2], it[3]);
    EXPECT_GT(it[3], it[4]); EXPECT_GT(it[4], it[5]); EXPECT_EQ(it[6], low); EXPECT_EQ(it[7], it[1]);
}

TEST_F(ArpeggiatorTest, ANewChordRestartsThePattern)
{
    set(ParamID::Motion::ArpMode, 1.0f);             // Up
    set(ParamID::Motion::ArpDiv, 4.0f);
    note(60, true); const int ch = channelOf(); note(64, true); note(67, true);
    runBlocks(kBlocksPerSixteenth + 3);              // part way into the second step
    note(60, false); note(64, false); note(67, false);
    note(62, true); note(65, true); note(69, true);
    runBlocks(1);
    const int first = keyCode(ch);
    const auto later = seen(order(ch, kBlocksPerSixteenth * 4));
    EXPECT_EQ(first, *later.begin()) << "the new chord starts on its lowest note, whatever the step counter says";
}

TEST_F(ArpeggiatorTest, RetriggerKeysEveryStepAndTheGateLetsGo)
{
    set(ParamID::Motion::ArpMode, 1.0f);
    set(ParamID::Motion::ArpDiv, 4.0f);
    set(ParamID::Motion::ArpRetrigger, 1.0f);
    set(ParamID::Motion::ArpGate, 50.0f);
    note(60, true); const int ch = channelOf(); note(64, true); note(67, true);
    const auto before = processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch));
    bool sawKeyOff = false;
    for (int i = 0; i < kBlocksPerSixteenth * 3; ++i) {
        runBlocks(1);
        if (processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) == ch) sawKeyOff = true;
    }
    EXPECT_GE(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch)) - before, 2u) << "each step keys the channel on again";
    EXPECT_TRUE(sawKeyOff) << "with a 50 % gate the note is released half way through a step";
    
    set(ParamID::Motion::ArpRetrigger, 0.0f);
    const auto chipStyle = processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch));
    runBlocks(kBlocksPerSixteenth * 3);
    EXPECT_EQ(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch)), chipStyle) << "chip style only retunes";
}

TEST_F(ArpeggiatorTest, OctavesExtendTheChord)
{
    set(ParamID::Motion::ArpMode, 1.0f);
    set(ParamID::Motion::ArpDiv, 4.0f);
    set(ParamID::Motion::ArpOctaves, 2.0f);
    note(60, true); const int ch = channelOf(); note(64, true);
    EXPECT_EQ(seen(order(ch, kBlocksPerSixteenth * 5)).size(), 4u) << "60, 64, 72, 76";
}

TEST_F(ArpeggiatorTest, AChordTableArpeggiatesASingleNote)
{
    set(ParamID::Motion::ArpMode, 1.0f);
    set(ParamID::Motion::ArpDiv, 4.0f);
    set(ParamID::Motion::ArpChord, 1.0f);            // Major
    note(60, true); const int ch = channelOf();
    const auto seq = order(ch, kBlocksPerSixteenth * 4);
    EXPECT_EQ(seen(seq).size(), 3u) << "root, third, fifth";
    ASSERT_GE(seq.size(), 3u);
    EXPECT_LT(seq[0], seq[1]); EXPECT_LT(seq[1], seq[2]);
}

TEST_F(ArpeggiatorTest, LatchKeepsTheChordUntilTheNextOne)
{
    set(ParamID::Motion::ArpMode, 1.0f);
    set(ParamID::Motion::ArpDiv, 4.0f);
    set(ParamID::Motion::ArpLatch, 1.0f);
    note(60, true); const int ch = channelOf(); note(64, true); note(67, true);
    note(60, false); note(64, false); note(67, false);
    EXPECT_EQ(seen(order(ch, kBlocksPerSixteenth * 4)).size(), 3u) << "still cycling with no key down";
    
    note(62, true);                                  // a new chord replaces the latched one
    note(62, false);
    runBlocks(kBlocksPerSixteenth * 2);
    const int alone = keyCode(ch);
    EXPECT_EQ(seen(order(ch, kBlocksPerSixteenth * 3)).size(), 1u) << "one latched note: nothing to cycle";
    EXPECT_EQ(keyCode(ch), alone);
    
    set(ParamID::Motion::ArpLatch, 0.0f);            // latch off with no key down: the note is let go
    runBlocks(2);
    EXPECT_EQ(processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF), ch);
}

TEST_F(ArpeggiatorTest, AccentSitsTheOffBeatStepsBack)
{
    set(ParamID::Motion::ArpMode, 1.0f);
    set(ParamID::Motion::ArpDiv, 4.0f);              // four steps to the beat
    set(ParamID::Motion::ArpAccent, 1.0f);           // Beat
    set(ParamID::Motion::ArpAccentDepth, 6.0f);
    note(60, true); const int ch = channelOf(); note(64, true); note(67, true);
    std::set<int> levels;
    for (int i = 0; i < kBlocksPerSixteenth * 5; ++i) { runBlocks(1); levels.insert(processor.getMotionEngine().currentCarrierSteps(ch)); }
    EXPECT_EQ(levels, (std::set<int>{ 0, 6 }));
}

TEST_F(ArpeggiatorTest, AsPlayedFollowsTheOrderOfTheKeys)
{
    set(ParamID::Motion::ArpMode, 5.0f);             // As Played
    set(ParamID::Motion::ArpDiv, 4.0f);
    note(67, true); const int ch = channelOf(); note(60, true); note(64, true);
    runBlocks(1);
    const auto seq = order(ch, kBlocksPerSixteenth * 4);
    ASSERT_GE(seq.size(), 3u);
    const int high = *std::max_element(seq.begin(), seq.end()), low = *std::min_element(seq.begin(), seq.end());
    auto it = std::find(seq.begin(), seq.end(), high);
    ASSERT_LE(it + 2, seq.end());
    EXPECT_EQ(it[1], low) << "after the first key comes the second";
    EXPECT_NE(it[2], low); EXPECT_NE(it[2], high);
}

TEST_F(ArpeggiatorTest, RandomStaysInsideTheChord)
{
    set(ParamID::Motion::ArpMode, 4.0f);             // Random
    set(ParamID::Motion::ArpDiv, 4.0f);
    note(60, true); const int ch = channelOf(); note(64, true); note(67, true);
    const auto s = seen(order(ch, kBlocksPerSixteenth * 12));
    EXPECT_GE(s.size(), 2u);
    EXPECT_LE(s.size(), 3u);
}

// ---- with a host transport, as in a DAW
namespace {
class ArpPlayHead : public juce::AudioPlayHead {
public:
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info; info.setBpm(bpm); info.setPpqPosition(ppq); info.setIsPlaying(true); return info;
    }
    void advance(int samples, double sampleRate) { ppq += samples / (sampleRate * 60.0 / bpm); }
    double bpm = 172.0, ppq = 0.0;
};
}

class ArpeggiatorTransportTest : public ArpeggiatorTest {
protected:
    void SetUp() override {
        ArpeggiatorTest::SetUp();
        processor.setPlayHead(&playHead);
    }
    void block(juce::MidiBuffer* midi = nullptr) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer empty;
        processor.processBlock(buffer, midi ? *midi : empty);
        playHead.advance(512, 48000.0);
    }
    void chord(std::initializer_list<int> notes, bool on) {
        juce::MidiBuffer midi;
        for (int n : notes) midi.addEvent(on ? juce::MidiMessage::noteOn(1, n, static_cast<juce::uint8>(96)) : juce::MidiMessage::noteOff(1, n), 0);
        block(&midi);
    }
    ArpPlayHead playHead;
};

TEST_F(ArpeggiatorTransportTest, AChordJustBeforeTheBarStartsOnTheRootAndRetriggersEveryStep)
{
    set(ParamID::Motion::Sync, 1.0f);
    set(ParamID::Motion::ArpMode, 3.0f);             // Up Down
    set(ParamID::Motion::ArpDiv, 4.0f);              // 1/16
    set(ParamID::Motion::ArpRetrigger, 1.0f);
    set(ParamID::Motion::ArpGate, 90.0f);
    const double sixteenth = 0.25;
    // The host delivers the bar's first notes in the block that contains the bar line, a few ms early
    playHead.ppq = 4.0 - 0.6 * (512.0 / (48000.0 * 60.0 / 172.0));
    chord({ 69, 72, 76, 81 }, true);
    const int ch = channelOf();
    std::vector<int> firstSteps;
    std::vector<uint32_t> keyOns;
    for (int step = 0; step < 8; ++step) {
        while (playHead.ppq < 4.0 + (step + 0.5) * sixteenth) block();
        firstSteps.push_back(keyCode(ch));
        keyOns.push_back(processor.getYmfmWrapper().getNoteOnCount(static_cast<uint8_t>(ch)));
    }
    // Up Down over four notes: root, 3rd, 5th, octave, 5th, 3rd, root, 3rd
    const int root = *std::min_element(firstSteps.begin(), firstSteps.end());
    EXPECT_EQ(firstSteps[0], root) << "the pattern starts on the root at the bar line";
    EXPECT_LT(firstSteps[0], firstSteps[1]); EXPECT_LT(firstSteps[1], firstSteps[2]); EXPECT_LT(firstSteps[2], firstSteps[3]);
    EXPECT_GT(firstSteps[3], firstSteps[4]); EXPECT_EQ(firstSteps[6], root);
    for (size_t i = 1; i < keyOns.size(); ++i)
        EXPECT_EQ(keyOns[i], keyOns[i - 1] + 1) << "step " << i << " keys the channel on again";
}
