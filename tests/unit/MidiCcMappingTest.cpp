#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"

// VOPMex-compatible control changes: one CC per parameter and operator, and
// the CC value is the register value itself (TL 0 = loudest, MUL 0-15, AR 0-31, ...).
class MidiCcMappingTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    
    void sendCC(int cc, int value) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, value), 0);
        processor.processBlock(buffer, midi);
    }
    
    // Register value currently held by a parameter (exact: discrete parameters)
    float registerValue(const juce::String& id) {
        auto* param = processor.getParameters().getParameter(id);
        EXPECT_NE(param, nullptr) << id;
        return param ? param->convertFrom0to1(param->getValue()) : -1.0f;
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(MidiCcMappingTest, OperatorCcsAreGroupedByParameter)
{
    using namespace ParamID;
    sendCC(MIDI_CC::Op1_TL, 0);   EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 0.0f);
    sendCC(MIDI_CC::Op2_TL, 64);  EXPECT_FLOAT_EQ(registerValue(Op::tl(2)), 64.0f);
    sendCC(MIDI_CC::Op4_TL, 127); EXPECT_FLOAT_EQ(registerValue(Op::tl(4)), 127.0f);
    sendCC(MIDI_CC::Op1_MUL, 15); EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 15.0f);
    sendCC(MIDI_CC::Op3_DT1, 5);  EXPECT_FLOAT_EQ(registerValue(Op::dt1(3)), 5.0f);
    sendCC(MIDI_CC::Op2_DT2, 3);  EXPECT_FLOAT_EQ(registerValue(Op::dt2(2)), 3.0f);
    sendCC(MIDI_CC::Op4_KS, 2);   EXPECT_FLOAT_EQ(registerValue(Op::ks(4)), 2.0f);
    sendCC(MIDI_CC::Op1_AR, 31);  EXPECT_FLOAT_EQ(registerValue(Op::ar(1)), 31.0f);
    sendCC(MIDI_CC::Op4_AR, 10);  EXPECT_FLOAT_EQ(registerValue(Op::ar(4)), 10.0f);
    sendCC(MIDI_CC::Op2_D1R, 20); EXPECT_FLOAT_EQ(registerValue(Op::d1r(2)), 20.0f);
    sendCC(MIDI_CC::Op3_D2R, 7);  EXPECT_FLOAT_EQ(registerValue(Op::d2r(3)), 7.0f);
    sendCC(MIDI_CC::Op1_D1L, 12); EXPECT_FLOAT_EQ(registerValue(Op::d1l(1)), 12.0f);
    sendCC(MIDI_CC::Op1_RR, 7);   EXPECT_FLOAT_EQ(registerValue(Op::rr(1)), 7.0f);
    sendCC(MIDI_CC::Op2_AME, 1);  EXPECT_GT(registerValue(Op::ams_en(2)), 0.5f);
}

TEST_F(MidiCcMappingTest, CcValueIsTheRegisterValueNotScaled)
{
    using namespace ParamID;
    // On the previous scaled mapping CC 43 = 31 became AR 7; VOPMex sends AR 31.
    sendCC(MIDI_CC::Op1_AR, 31);
    EXPECT_FLOAT_EQ(registerValue(Op::ar(1)), 31.0f);
    // TL keeps the hardware direction: 0 is loudest (VOPMex: "0 最大, 127 無音")
    sendCC(MIDI_CC::Op1_TL, 0);
    EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 0.0f);
}

TEST_F(MidiCcMappingTest, OutOfRangeValuesAreClamped)
{
    using namespace ParamID;
    sendCC(MIDI_CC::Op1_MUL, 127); EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 15.0f);
    sendCC(MIDI_CC::Op1_RR, 127);  EXPECT_FLOAT_EQ(registerValue(Op::rr(1)), 15.0f);
    sendCC(MIDI_CC::Algorithm, 127); EXPECT_FLOAT_EQ(registerValue(Global::Algorithm), 7.0f);
}

TEST_F(MidiCcMappingTest, GlobalLfoAndNoiseCcs)
{
    using namespace ParamID;
    sendCC(MIDI_CC::Algorithm, 5);   EXPECT_FLOAT_EQ(registerValue(Global::Algorithm), 5.0f);
    sendCC(MIDI_CC::Feedback, 7);    EXPECT_FLOAT_EQ(registerValue(Global::Feedback), 7.0f);
    sendCC(MIDI_CC::LfoRate, 100);   EXPECT_FLOAT_EQ(registerValue(Global::LfoRate), 200.0f);  // upper 7 bits of 8
    sendCC(MIDI_CC::LfoPmd, 40);     EXPECT_FLOAT_EQ(registerValue(Global::LfoPmd), 40.0f);
    sendCC(MIDI_CC::LfoAmd, 33);     EXPECT_FLOAT_EQ(registerValue(Global::LfoAmd), 33.0f);
    sendCC(MIDI_CC::LfoWaveform, 2); EXPECT_FLOAT_EQ(registerValue(Global::LfoWaveform), 2.0f);
    sendCC(MIDI_CC::NoiseEnable, 127);   EXPECT_GT(registerValue(Global::NoiseEnable), 0.5f);
    sendCC(MIDI_CC::NoiseFrequency, 31); EXPECT_FLOAT_EQ(registerValue(Global::NoiseFrequency), 31.0f);
    // Legacy YMulator numbers still work
    sendCC(MIDI_CC::LegacyLfoWaveform, 1);   EXPECT_FLOAT_EQ(registerValue(Global::LfoWaveform), 1.0f);
    sendCC(MIDI_CC::LegacyNoiseFrequency, 9); EXPECT_FLOAT_EQ(registerValue(Global::NoiseFrequency), 9.0f);
}
