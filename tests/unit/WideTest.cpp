#include <gtest/gtest.h>
#include <cmath>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "dsp/YM2151Registers.h"
#include "utils/ParameterIDs.h"

// Wide: a second chip mirrors every register, plays each note detuned the
// other way and sits on the other side. Polyphony stays at 8.
class WideTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(3);
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
    std::pair<double, double> renderLevels(int blocks) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        double l = 0.0, r = 0.0, diff = 0.0;
        for (int b = 0; b < blocks; ++b) {
            processor.processBlock(buffer, midi);
            for (int i = 0; i < 512; ++i) {
                l += std::abs(buffer.getSample(0, i));
                r += std::abs(buffer.getSample(1, i));
                diff += std::abs(buffer.getSample(0, i) - buffer.getSample(1, i));
            }
        }
        return { (l + r) / (2.0 * blocks * 512), diff / (blocks * 512) };
    }
    uint8_t main(int a) { return processor.getYmfmWrapper().readCurrentRegister(a); }
    uint8_t shadow(int a) { return processor.getYmfmWrapper().readShadowRegister(a); }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(WideTest, OffKeepsBothChipsIdenticalAndTheOutputCentred)
{
    const int ch = noteOn();
    runBlocks(2);
    for (int a = 0x20; a < 0x100; ++a)
        EXPECT_EQ(main(a), shadow(a)) << "register " << a;
    EXPECT_FALSE(processor.getYmfmWrapper().isWideEnabled());
    const auto [level, diff] = renderLevels(8);
    EXPECT_GT(level, 0.001);
    EXPECT_LT(diff, 1e-6) << "centre pan: left equals right";
    (void) ch;
}

TEST_F(WideTest, LeftRightSplitsTheChipsAndDetunesThem)
{
    set(ParamID::Motion::Wide, 100.0f);           // +/- 25 cents = 16 key-fraction steps
    set(ParamID::Motion::WidePan, 0.0f);
    const int ch = noteOn(69);
    runBlocks(1);
    EXPECT_TRUE(processor.getYmfmWrapper().isWideEnabled());
    
    // The main cache keeps the parameter view (centre); the left-only pan is applied at the chip.
    // The shadow cache holds what its chip received: right only.
    const int alg = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + ch;
    EXPECT_EQ(main(alg) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_CENTER);
    EXPECT_EQ(shadow(alg) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_RIGHT_ONLY);
    EXPECT_EQ(main(alg) & ~YM2151Regs::MASK_PAN_LR, shadow(alg) & ~YM2151Regs::MASK_PAN_LR);
    
    // A4 sits on a whole key code; -16 steps wraps into the semitone below, +16 stays
    const int kf = YM2151Regs::REG_KEY_FRACTION_BASE + ch, kc = YM2151Regs::REG_KEY_CODE_BASE + ch;
    EXPECT_EQ(shadow(kf) >> YM2151Regs::SHIFT_KEY_FRACTION, 16);
    EXPECT_EQ(main(kf) >> YM2151Regs::SHIFT_KEY_FRACTION, 48);
    EXPECT_NE(main(kc), shadow(kc));
    
    // Everything that is not pitch or pan is mirrored
    for (int a = 0x20; a < 0x100; ++a) {
        if (a >= 0x20 && a < 0x38) continue;
        EXPECT_EQ(main(a), shadow(a)) << "register " << a;
    }
    
    const auto [level, diff] = renderLevels(8);
    EXPECT_GT(level, 0.001);
    EXPECT_GT(diff, 0.01) << "the two sides carry different chips";
}

TEST_F(WideTest, CentreModeKeepsPanAndMixesBothChips)
{
    set(ParamID::Motion::Wide, 40.0f);
    set(ParamID::Motion::WidePan, 1.0f);
    const int ch = noteOn(69);
    runBlocks(1);
    const int alg = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + ch;
    EXPECT_EQ(main(alg) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_CENTER);
    EXPECT_EQ(shadow(alg) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_CENTER);
    const auto [level, diff] = renderLevels(8);
    EXPECT_GT(level, 0.001);
    EXPECT_LT(diff, 1e-6);
}

TEST_F(WideTest, TurningWideOffRestoresThePansAndPitch)
{
    set(ParamID::Motion::Wide, 100.0f);
    const int ch = noteOn(69);
    runBlocks(1);
    set(ParamID::Motion::Wide, 0.0f);
    runBlocks(1);
    const int alg = YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + ch;
    EXPECT_EQ(main(alg) & YM2151Regs::MASK_PAN_LR, YM2151Regs::PAN_CENTER);
    EXPECT_EQ(main(YM2151Regs::REG_KEY_FRACTION_BASE + ch), 0);
    EXPECT_FALSE(processor.getYmfmWrapper().isWideEnabled());
}

TEST_F(WideTest, PolyphonyStaysAtEight)
{
    set(ParamID::Motion::Wide, 100.0f);
    std::set<int> channels;
    for (int n = 0; n < 8; ++n) channels.insert(noteOn(48 + n));
    EXPECT_EQ(channels.size(), 8u) << "eight notes get eight channels with Wide on";
}
