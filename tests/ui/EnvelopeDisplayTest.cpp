#include <gtest/gtest.h>
#include "../../src/ui/EnvelopeDisplay.h"

namespace {
float sustainOf(const EnvelopeDisplay::Shape& s)
{
    // the level at the moment the key is released
    for (const auto& p : s.points) if (p.time == s.keyOffTime) return p.attenuation;
    return -1.0f;
}
}

TEST(EnvelopeDisplayTest, SustainSitsBelowThePeakByFourTimesD1L)
{
    // TL 20, D1L 2: the chip holds at 20 + 8 = 28, a 6 dB step down that the old drawing left out
    const auto s = EnvelopeDisplay::computeShape(20, 31, 10, 2, 0, 7);
    EXPECT_FALSE(s.silent);
    EXPECT_FLOAT_EQ(s.points[1].attenuation, 20.0f) << "peak at TL";
    EXPECT_FLOAT_EQ(s.points[2].attenuation, 28.0f) << "decay 1 ends at TL + 4*D1L";
    EXPECT_FLOAT_EQ(sustainOf(s), 28.0f) << "no D2R: held there";
}

TEST(EnvelopeDisplayTest, D1LFifteenGoesAllTheWayDown)
{
    const auto s = EnvelopeDisplay::computeShape(0, 31, 10, 15, 0, 7);
    EXPECT_FLOAT_EQ(s.points[2].attenuation, 127.0f);
}

TEST(EnvelopeDisplayTest, NoDecayOneHoldsThePeak)
{
    // D1R 0: the envelope never leaves the peak, so D2R cannot start either
    const auto s = EnvelopeDisplay::computeShape(10, 31, 0, 8, 20, 7);
    EXPECT_FLOAT_EQ(sustainOf(s), 10.0f);
}

TEST(EnvelopeDisplayTest, RatesSetTheTimes)
{
    EXPECT_FLOAT_EQ(EnvelopeDisplay::timeForRate(31), 1.0f);
    EXPECT_FLOAT_EQ(EnvelopeDisplay::timeForRate(27), 2.0f) << "four steps slower takes twice as long";
    EXPECT_TRUE(std::isinf(EnvelopeDisplay::timeForRate(0)));
    const auto fast = EnvelopeDisplay::computeShape(0, 31, 31, 4, 0, 15);
    const auto slow = EnvelopeDisplay::computeShape(0, 15, 15, 4, 0, 3);
    EXPECT_LT(fast.points[1].time, slow.points[1].time) << "attack";
    EXPECT_LT(fast.points.back().time - fast.keyOffTime, slow.points.back().time - slow.keyOffTime) << "release";
}

TEST(EnvelopeDisplayTest, DecayTwoSlopesWhileHeldAndCanReachSilence)
{
    const auto gentle = EnvelopeDisplay::computeShape(0, 31, 31, 4, 8, 7);
    EXPECT_GT(sustainOf(gentle), 16.0f) << "D2R pulls the level down during the hold";
    EXPECT_LT(sustainOf(gentle), 127.0f);
    const auto steep = EnvelopeDisplay::computeShape(0, 31, 31, 4, 31, 7);
    EXPECT_FLOAT_EQ(sustainOf(steep), 127.0f) << "a fast D2R silences the note before release";
}

TEST(EnvelopeDisplayTest, SilentOperators)
{
    EXPECT_TRUE(EnvelopeDisplay::computeShape(0, 0, 31, 0, 0, 7).silent) << "AR 0 never attacks";
    EXPECT_TRUE(EnvelopeDisplay::computeShape(127, 31, 31, 0, 0, 7).silent) << "TL 127 is inaudible";
}
