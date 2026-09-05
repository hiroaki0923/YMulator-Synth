#include <gtest/gtest.h>
#include "dsp/YmfmWrapper.h"
#include "dsp/YM2151Registers.h"
#include "core/ParameterManager.h"
#include <vector>

// Operator index -> register slot regression test.
//
// Operators are numbered in voice (VOPM) order: 0 = M1, 1 = C1, 2 = M2, 3 = C2.
// The YM2151 register map stores slots as M1, M2, C1, C2, and algorithm 4 wires
// two independent chains M1 -> C1 and M2 -> C2. If the index-to-slot mapping is
// wrong, C1 and M2 swap places and every preset's carriers and modulators trade
// roles. This test drives algorithm 4 and checks which operator brightens which.
namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

using Param = YmfmWrapperInterface::OperatorParameter;

void configureAlgorithm4(YmfmWrapper& wrapper)
{
    wrapper.initialize(YmfmWrapperInterface::ChipType::OPM, static_cast<uint32_t>(kSampleRate));
    wrapper.setChannelPan(0, 0.5f);
    wrapper.setAlgorithm(0, 4);
    wrapper.setFeedback(0, 0);
    for (uint8_t op = 0; op < 4; ++op) {
        wrapper.setOperatorParameter(0, op, Param::TotalLevel, 127);  // silent / no modulation
        wrapper.setOperatorParameter(0, op, Param::AttackRate, 31);
        wrapper.setOperatorParameter(0, op, Param::Decay1Rate, 0);
        wrapper.setOperatorParameter(0, op, Param::SustainLevel, 0);
        wrapper.setOperatorParameter(0, op, Param::Decay2Rate, 0);
        wrapper.setOperatorParameter(0, op, Param::ReleaseRate, 15);
        wrapper.setOperatorParameter(0, op, Param::KeyScale, 0);
        wrapper.setOperatorParameter(0, op, Param::Multiple, 1);
        wrapper.setOperatorParameter(0, op, Param::Detune1, 0);
        wrapper.setOperatorParameter(0, op, Param::Detune2, 0);
    }
}

// Sign changes in one second of output after a short settle. A plain sine at A4
// gives ~880; FM at full modulator level gives several thousand.
int countSignChanges(uint8_t loudCarrier, uint8_t loudModulator)
{
    YmfmWrapper wrapper;
    configureAlgorithm4(wrapper);
    wrapper.setOperatorParameter(0, loudCarrier, Param::TotalLevel, 0);
    wrapper.setOperatorParameter(0, loudModulator, Param::TotalLevel, 0);
    wrapper.noteOn(0, 69, 127);

    std::vector<float> left(kBlockSize), right(kBlockSize), all;
    const int total = static_cast<int>(kSampleRate * 1.2);
    for (int done = 0; done < total; done += kBlockSize) {
        wrapper.generateSamples(left.data(), right.data(), kBlockSize);
        all.insert(all.end(), left.begin(), left.end());
    }
    int changes = 0;
    for (size_t i = static_cast<size_t>(kSampleRate * 0.2) + 1; i < all.size(); ++i) {
        if ((all[i - 1] < 0.0f) != (all[i] < 0.0f)) ++changes;
    }
    return changes;
}

} // namespace

class OperatorSlotOrderTest : public ::testing::Test {
protected:
    void TearDown() override { ymulatorsynth::ParameterManager::resetStaticState(); }
};

TEST_F(OperatorSlotOrderTest, M1ModulatesC1InAlgorithm4)
{
    const int plain = countSignChanges(1, 1);      // C1 alone
    const int withM1 = countSignChanges(1, 0);     // C1 + M1 loud
    const int withM2 = countSignChanges(1, 2);     // C1 + M2 loud (M2 feeds C2, which is silent)
    EXPECT_GT(withM1, plain * 3) << "M1 must modulate C1";
    // Identical register state on the audible chain -> identical deterministic output
    EXPECT_EQ(withM2, plain) << "M2 must not affect C1";
}

TEST_F(OperatorSlotOrderTest, M2ModulatesC2InAlgorithm4)
{
    const int plain = countSignChanges(3, 3);      // C2 alone
    const int withM2 = countSignChanges(3, 2);     // C2 + M2 loud
    const int withM1 = countSignChanges(3, 0);     // C2 + M1 loud (M1 feeds C1, which is silent)
    EXPECT_GT(withM2, plain * 3) << "M2 must modulate C2";
    EXPECT_EQ(withM1, plain) << "M1 must not affect C2";
}

TEST_F(OperatorSlotOrderTest, RegisterHelperUsesVoiceOrderSlots)
{
    EXPECT_EQ(YM2151Regs::getOperatorRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE, 0, 0), 0x60);  // M1
    EXPECT_EQ(YM2151Regs::getOperatorRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE, 1, 0), 0x70);  // C1
    EXPECT_EQ(YM2151Regs::getOperatorRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE, 2, 0), 0x68);  // M2
    EXPECT_EQ(YM2151Regs::getOperatorRegister(YM2151Regs::REG_TOTAL_LEVEL_BASE, 3, 0), 0x78);  // C2
}
