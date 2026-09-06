#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"

// MIDI velocity attenuates the carriers only, on top of the parameter TL,
// and must neither be undone by the per-block parameter update nor pile up
// from one note to the next.
class VelocityTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
        set(ParamID::Global::Algorithm, 4);          // carriers: op2, op4
        for (int op = 1; op <= 4; ++op) set(ParamID::Op::tl(op), 10 * op);
        runBlocks(2);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    void set(const std::string& id, int value) {
        auto* p = processor.getParameters().getParameter(id);
        ASSERT_NE(p, nullptr);
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
    }
    void runBlocks(int n, juce::MidiBuffer* midi = nullptr) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer empty;
        for (int i = 0; i < n; ++i) { processor.processBlock(buffer, midi ? *midi : empty); if (midi) midi->clear(); }
    }
    int noteOn(int velocity) {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(velocity)), 0);
        runBlocks(1, &midi);
        return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_KEY_ON_OFF) & 0x07;
    }
    void noteOff() {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        runBlocks(1, &midi);
    }
    int tl(int channel, int op) {
        return processor.getYmfmWrapper().readCurrentRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE + YM2151Regs::OPERATOR_SLOT_OFFSET[op] + channel);
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(VelocityTest, FullVelocityPlaysTheParameterLevel)
{
    const int ch = noteOn(127);
    for (int op = 0; op < 4; ++op) EXPECT_EQ(tl(ch, op), 10 * (op + 1)) << "op " << op + 1;
}

TEST_F(VelocityTest, LowerVelocityAttenuatesCarriersOnlyAndSurvivesBlocks)
{
    const int ch = noteOn(64);
    const int expected = juce::roundToInt((1.0f - 64.0f / 127.0f) * YM2151Regs::VELOCITY_TL_RANGE);   // 16
    EXPECT_EQ(tl(ch, 0), 10) << "modulator M1 untouched";
    EXPECT_EQ(tl(ch, 1), 20 + expected) << "carrier C1";
    EXPECT_EQ(tl(ch, 2), 30) << "modulator M2 untouched";
    EXPECT_EQ(tl(ch, 3), 40 + expected) << "carrier C2";
    
    runBlocks(10);
    EXPECT_EQ(tl(ch, 1), 20 + expected) << "the parameter update must not undo velocity";
    
    set(ParamID::Op::tl(2), 25);
    runBlocks(1);
    EXPECT_EQ(tl(ch, 1), 25 + expected) << "a TL edit during the note keeps the velocity";
}

TEST_F(VelocityTest, VelocityDoesNotAccumulateAcrossNotes)
{
    noteOn(1);
    noteOff();
    runBlocks(4);
    const int ch = noteOn(127);
    EXPECT_EQ(tl(ch, 1), 20);
    EXPECT_EQ(tl(ch, 3), 40);
}

TEST_F(VelocityTest, AlgorithmChangeMovesTheAttenuationToTheNewCarriers)
{
    const int ch = noteOn(1);                    // maximum attenuation
    EXPECT_EQ(tl(ch, 0), 10);
    EXPECT_EQ(tl(ch, 3), 40 + YM2151Regs::VELOCITY_TL_RANGE);
    set(ParamID::Global::Algorithm, 7);          // every operator is a carrier
    runBlocks(1);
    for (int op = 0; op < 4; ++op) EXPECT_EQ(tl(ch, op), 10 * (op + 1) + YM2151Regs::VELOCITY_TL_RANGE) << "op " << op + 1;
}

TEST_F(VelocityTest, VelocityBrightnessDarkensTheModulators)
{
    auto* p = processor.getParameters().getParameter(ParamID::Motion::VelBright);
    ASSERT_NE(p, nullptr);
    p->setValueNotifyingHost(p->convertTo0to1(100.0f));
    runBlocks(1);
    EXPECT_FLOAT_EQ(processor.getYmfmWrapper().getVelocityBrightness(), 1.0f);
    // quiet 0.496: carriers +16 (32-step range), modulators +20 (40-step range at full brightness)
    const int ch = noteOn(64);
    EXPECT_EQ(tl(ch, 0), 10 + 20) << "modulator M1";
    EXPECT_EQ(tl(ch, 2), 30 + 20) << "modulator M2";
    EXPECT_EQ(tl(ch, 1), 20 + 16) << "carrier C1 keeps the plain velocity";
    p->setValueNotifyingHost(0.0f);
    runBlocks(1);
    noteOff();
    runBlocks(2);
    const int again = noteOn(64);
    EXPECT_EQ(tl(again, 0), 10) << "brightness off: modulators back to the patch";
}
