#include <gtest/gtest.h>
#include <cmath>
#include <set>
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
