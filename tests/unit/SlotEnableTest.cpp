#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"
#include "utils/PresetManager.h"
#include "utils/VOPMParser.h"

// Per-operator slot enable: the key-on register must list exactly the enabled
// operators, in hardware slot order (M1, M2, C1, C2 = bits 3..6).
class SlotEnableTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    void setSlot(int op, bool on) {
        auto* p = processor.getParameters().getParameter(ParamID::Op::slot_en(op));
        ASSERT_NE(p, nullptr);
        p->setValueNotifyingHost(on ? 1.0f : 0.0f);
    }
    uint8_t keyOnAfterNote() {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);                       // flush parameters
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        processor.processBlock(buffer, midi);
        return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF);
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(SlotEnableTest, KeyOnBitsFollowTheChainOrder)
{
    // ymfm keys operator n of its (0, 16, 8, 24) map from key-on bit n: M1, C1, M2, C2
    EXPECT_EQ(YM2151Regs::keyOnBitsForSlotMask(0x0F), YM2151Regs::KEY_ON_ALL_OPS);
    EXPECT_EQ(YM2151Regs::keyOnBitsForSlotMask(0x01), 0x08);   // op1 = M1 -> bit 3
    EXPECT_EQ(YM2151Regs::keyOnBitsForSlotMask(0x02), 0x10);   // op2 = C1 -> bit 4
    EXPECT_EQ(YM2151Regs::keyOnBitsForSlotMask(0x04), 0x20);   // op3 = M2 -> bit 5
    EXPECT_EQ(YM2151Regs::keyOnBitsForSlotMask(0x08), 0x40);   // op4 = C2 -> bit 6
}

TEST_F(SlotEnableTest, OnlyTheEnabledOperatorsSound)
{
    // Algorithm 6: op1 modulates op2; op2, op3 and op4 are carriers. Op3 and op4 at TL 127 are silent,
    // so the sound has to come from op2, which only keys on if its slot bit is the right one.
    auto set = [&](const juce::String& id, float value) {
        auto* p = processor.getParameters().getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    set(ParamID::Global::Algorithm, 6.0f);
    set(ParamID::Op::tl(1), 22.0f); set(ParamID::Op::tl(2), 0.0f);
    set(ParamID::Op::tl(3), 127.0f); set(ParamID::Op::tl(4), 127.0f);
    for (int op = 1; op <= 4; ++op) { set(ParamID::Op::ar(op), 31.0f); set(ParamID::Op::rr(op), 10.0f); }
    auto peakAfterNote = [&](bool s1, bool s2, bool s3, bool s4) {
        setSlot(1, s1); setSlot(2, s2); setSlot(3, s3); setSlot(4, s4);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        float peak = 0.0f;
        for (int b = 0; b < 8; ++b) { processor.processBlock(buffer, midi); midi.clear(); peak = juce::jmax(peak, buffer.getMagnitude(0, 0, 512)); }
        midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        for (int b = 0; b < 40; ++b) { processor.processBlock(buffer, midi); midi.clear(); }
        return peak;
    };
    EXPECT_GT(peakAfterNote(true, true, false, false), 0.02f) << "M1 and C1 on: the carrier C1 sounds";
    EXPECT_LT(peakAfterNote(true, false, true, false), 1e-4f) << "M1 and M2 on: M2 is a carrier at TL 127, C1 is not keyed";
    EXPECT_GT(peakAfterNote(false, true, false, false), 0.02f) << "C1 alone still sounds (unmodulated)";
}

TEST_F(SlotEnableTest, DisabledOperatorIsLeftOutOfKeyOn)
{
    const uint8_t all = keyOnAfterNote();
    EXPECT_EQ(all & 0x78, YM2151Regs::KEY_ON_ALL_OPS);
    
    setSlot(2, false);                                           // C1 off
    const uint8_t without2 = keyOnAfterNote();
    EXPECT_EQ(without2 & 0x78, YM2151Regs::KEY_ON_ALL_OPS & ~0x10);
    
    setSlot(2, true);
    EXPECT_EQ(keyOnAfterNote() & 0x78, YM2151Regs::KEY_ON_ALL_OPS);
}

TEST_F(SlotEnableTest, PresetSlotMaskMapsKeyOnBitsToOperators)
{
    ymulatorsynth::VOPMVoice voice;
    voice.channel.slotMask = ymulatorsynth::VOPMParser::convertOpmSlotToInternal(0x10);   // key-on bit 4 = C1
    const auto preset = ymulatorsynth::Preset::fromVOPM(voice);
    EXPECT_FALSE(preset.operators[0].slotEnable);
    EXPECT_TRUE(preset.operators[1].slotEnable) << "C1 is operator 2 in voice order";
    EXPECT_FALSE(preset.operators[2].slotEnable);
    EXPECT_FALSE(preset.operators[3].slotEnable);
    
    const auto back = preset.toVOPM();
    EXPECT_EQ(ymulatorsynth::VOPMParser::convertInternalSlotToOpm(back.channel.slotMask), 0x10);
}

TEST_F(SlotEnableTest, PresetLoadSetsSlotParameters)
{
    for (int op = 1; op <= 4; ++op) {
        auto* p = processor.getParameters().getParameter(ParamID::Op::slot_en(op));
        ASSERT_NE(p, nullptr);
        EXPECT_GT(p->getValue(), 0.5f) << "bundled presets enable every operator";
    }
}
