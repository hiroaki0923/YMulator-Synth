#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"

// VOPMex-compatible control changes.
//
// Default ("natural") mode: the 0-127 CC value is scaled to the parameter's
// step count (value >> (7 - bits)), and TL, AR, D1R, D2R, D1L and RR run in
// the opposite direction to the register (CC 127 = loudest / slowest), like
// an analogue synth. NRPN 126/127 (or 126/0) with data 127 switches to
// "register" mode, where the CC value is the register value itself.
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
    
    void enterRegisterMode() {
        sendCC(99, 126);
        sendCC(98, 127);
        sendCC(6, 127);
    }
    
    float registerValue(const juce::String& id) {
        auto* param = processor.getParameters().getParameter(id);
        EXPECT_NE(param, nullptr) << id;
        return param ? param->convertFrom0to1(param->getValue()) : -1.0f;
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(MidiCcMappingTest, NaturalModeScalesAndReversesLikeVopmex)
{
    using namespace ParamID;
    // TL: 128 steps, reversed -> CC 127 is loudest (TL 0)
    sendCC(MIDI_CC::Op1_TL, 127); EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 0.0f);
    sendCC(MIDI_CC::Op1_TL, 0);   EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 127.0f);
    sendCC(MIDI_CC::Op2_TL, 100); EXPECT_FLOAT_EQ(registerValue(Op::tl(2)), 27.0f);
    // AR: 32 steps, reversed -> 31 - (value >> 2)
    sendCC(MIDI_CC::Op1_AR, 127); EXPECT_FLOAT_EQ(registerValue(Op::ar(1)), 0.0f);
    sendCC(MIDI_CC::Op1_AR, 0);   EXPECT_FLOAT_EQ(registerValue(Op::ar(1)), 31.0f);
    sendCC(MIDI_CC::Op4_AR, 64);  EXPECT_FLOAT_EQ(registerValue(Op::ar(4)), 15.0f);
    // D1R / D2R (32, reversed), D1L / RR (16, reversed)
    sendCC(MIDI_CC::Op2_D1R, 0);  EXPECT_FLOAT_EQ(registerValue(Op::d1r(2)), 31.0f);
    sendCC(MIDI_CC::Op3_D2R, 127); EXPECT_FLOAT_EQ(registerValue(Op::d2r(3)), 0.0f);
    sendCC(MIDI_CC::Op1_D1L, 0);  EXPECT_FLOAT_EQ(registerValue(Op::d1l(1)), 15.0f);
    sendCC(MIDI_CC::Op1_RR, 127); EXPECT_FLOAT_EQ(registerValue(Op::rr(1)), 0.0f);
    sendCC(MIDI_CC::Op1_RR, 64);  EXPECT_FLOAT_EQ(registerValue(Op::rr(1)), 7.0f);
    // MUL / DT1 / DT2 / KS: scaled, not reversed ("0=0, 16=1, 127=15")
    sendCC(MIDI_CC::Op1_MUL, 0);   EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 0.0f);
    sendCC(MIDI_CC::Op1_MUL, 16);  EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 2.0f);
    sendCC(MIDI_CC::Op1_MUL, 8);   EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 1.0f);
    sendCC(MIDI_CC::Op1_MUL, 127); EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 15.0f);
    sendCC(MIDI_CC::Op3_DT1, 127); EXPECT_FLOAT_EQ(registerValue(Op::dt1(3)), 7.0f);
    sendCC(MIDI_CC::Op3_DT1, 16);  EXPECT_FLOAT_EQ(registerValue(Op::dt1(3)), 1.0f);
    sendCC(MIDI_CC::Op2_DT2, 127); EXPECT_FLOAT_EQ(registerValue(Op::dt2(2)), 3.0f);
    sendCC(MIDI_CC::Op4_KS, 64);   EXPECT_FLOAT_EQ(registerValue(Op::ks(4)), 2.0f);
    // AME: 2 steps
    sendCC(MIDI_CC::Op2_AME, 127); EXPECT_GT(registerValue(Op::ams_en(2)), 0.5f);
    sendCC(MIDI_CC::Op2_AME, 0);   EXPECT_LT(registerValue(Op::ams_en(2)), 0.5f);
    // Algorithm / feedback: 8 steps
    sendCC(MIDI_CC::Algorithm, 127); EXPECT_FLOAT_EQ(registerValue(Global::Algorithm), 7.0f);
    sendCC(MIDI_CC::Algorithm, 64);  EXPECT_FLOAT_EQ(registerValue(Global::Algorithm), 4.0f);
    sendCC(MIDI_CC::Feedback, 16);   EXPECT_FLOAT_EQ(registerValue(Global::Feedback), 1.0f);
}

TEST_F(MidiCcMappingTest, OperatorCcsAreGroupedByParameter)
{
    using namespace ParamID;
    enterRegisterMode();
    sendCC(MIDI_CC::Op2_TL, 64);  EXPECT_FLOAT_EQ(registerValue(Op::tl(2)), 64.0f);
    sendCC(MIDI_CC::Op3_TL, 65);  EXPECT_FLOAT_EQ(registerValue(Op::tl(3)), 65.0f);
    sendCC(MIDI_CC::Op4_AR, 10);  EXPECT_FLOAT_EQ(registerValue(Op::ar(4)), 10.0f);
    sendCC(MIDI_CC::Op1_D1L, 12); EXPECT_FLOAT_EQ(registerValue(Op::d1l(1)), 12.0f);  // 55-58 is D1L
    sendCC(MIDI_CC::Op1_RR, 7);   EXPECT_FLOAT_EQ(registerValue(Op::rr(1)), 7.0f);    // 59-62 is RR
}

TEST_F(MidiCcMappingTest, RegisterModeViaNrpnUsesRawValues)
{
    using namespace ParamID;
    enterRegisterMode();
    sendCC(MIDI_CC::Op1_TL, 0);    EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 0.0f);     // no reversal
    sendCC(MIDI_CC::Op1_TL, 127);  EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 127.0f);
    sendCC(MIDI_CC::Op1_AR, 31);   EXPECT_FLOAT_EQ(registerValue(Op::ar(1)), 31.0f);
    sendCC(MIDI_CC::Op1_MUL, 1);   EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 1.0f);
    sendCC(MIDI_CC::Op1_MUL, 127); EXPECT_FLOAT_EQ(registerValue(Op::mul(1)), 15.0f);  // masked to the range
    sendCC(MIDI_CC::Algorithm, 5); EXPECT_FLOAT_EQ(registerValue(Global::Algorithm), 5.0f);
    // NRPN data 0 returns to natural mode
    sendCC(99, 126); sendCC(98, 127); sendCC(6, 0);
    sendCC(MIDI_CC::Op1_TL, 127);  EXPECT_FLOAT_EQ(registerValue(Op::tl(1)), 0.0f);
}

TEST_F(MidiCcMappingTest, GlobalLfoAndNoiseCcs)
{
    using namespace ParamID;
    sendCC(MIDI_CC::LfoRate, 100);   EXPECT_FLOAT_EQ(registerValue(Global::LfoRate), 200.0f);  // upper 7 bits of 8
    sendCC(MIDI_CC::LfoPmd, 40);     EXPECT_FLOAT_EQ(registerValue(Global::LfoPmd), 40.0f);
    sendCC(MIDI_CC::LfoAmd, 33);     EXPECT_FLOAT_EQ(registerValue(Global::LfoAmd), 33.0f);
    sendCC(MIDI_CC::LfoWaveform, 64); EXPECT_FLOAT_EQ(registerValue(Global::LfoWaveform), 2.0f);
    sendCC(MIDI_CC::NoiseEnable, 64);   EXPECT_GT(registerValue(Global::NoiseEnable), 0.5f);
    sendCC(MIDI_CC::NoiseEnable, 0);    EXPECT_LT(registerValue(Global::NoiseEnable), 0.5f);
    sendCC(MIDI_CC::NoiseFrequency, 127); EXPECT_FLOAT_EQ(registerValue(Global::NoiseFrequency), 31.0f);
    // Legacy YMulator numbers still work (same scaling)
    sendCC(MIDI_CC::LegacyLfoWaveform, 32);   EXPECT_FLOAT_EQ(registerValue(Global::LfoWaveform), 1.0f);
    sendCC(MIDI_CC::LegacyNoiseFrequency, 36); EXPECT_FLOAT_EQ(registerValue(Global::NoiseFrequency), 9.0f);
}
