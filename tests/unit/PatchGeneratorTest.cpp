#include <gtest/gtest.h>
#include <algorithm>
#include "core/PatchGenerator.h"
#include "dsp/AlgorithmInfo.h"

using namespace ymulatorsynth;

namespace {
bool sameOperator(const GeneratedPatch::Operator& a, const GeneratedPatch::Operator& b)
{
    return a.tl == b.tl && a.ar == b.ar && a.d1r == b.d1r && a.d1l == b.d1l && a.d2r == b.d2r && a.rr == b.rr
        && a.ks == b.ks && a.mul == b.mul && a.dt1 == b.dt1 && a.dt2 == b.dt2 && a.amsEnable == b.amsEnable;
}
bool samePatch(const GeneratedPatch& a, const GeneratedPatch& b)
{
    for (size_t i = 0; i < 4; ++i) if (!sameOperator(a.ops[i], b.ops[i])) return false;
    return a.algorithm == b.algorithm && a.feedback == b.feedback && a.lfoRate == b.lfoRate && a.lfoAmd == b.lfoAmd
        && a.lfoPmd == b.lfoPmd && a.lfoWaveform == b.lfoWaveform && a.lfoAms == b.lfoAms && a.lfoPms == b.lfoPms
        && a.noiseEnable == b.noiseEnable && a.noiseFrequency == b.noiseFrequency;
}
constexpr int kCategories = static_cast<int>(GeneratorCategory::Count);
}

TEST(PatchGeneratorTest, SameSeedSamePatch)
{
    GeneratorInput input;
    input.category = GeneratorCategory::Brass;
    input.bright = 0.7f;
    EXPECT_TRUE(samePatch(PatchGenerator::generate(input, 42), PatchGenerator::generate(input, 42)));
    EXPECT_FALSE(samePatch(PatchGenerator::generate(input, 42), PatchGenerator::generate(input, 43)));
}

TEST(PatchGeneratorTest, EveryPatchIsAudibleAndInRange)
{
    for (int c = 0; c < kCategories; ++c) {
        GeneratorInput input;
        input.category = static_cast<GeneratorCategory>(c);
        for (int seed = 0; seed < 200; ++seed) {
            input.bright = (seed % 7) / 6.0f;
            input.complex = (seed % 5) / 4.0f;
            input.hardAttack = (seed % 3) / 2.0f;
            input.length = (seed % 4) / 3.0f;
            input.moving = (seed % 6) / 5.0f;
            input.metallic = (seed % 8) / 7.0f;
            const auto p = PatchGenerator::generate(input, seed);
            SCOPED_TRACE("category " + std::to_string(c) + " seed " + std::to_string(seed));
            
            const auto& info = algorithmInfo(p.algorithm);
            bool audible = false;
            for (int op = 0; op < 4; ++op) {
                const auto& o = p.ops[static_cast<size_t>(op)];
                EXPECT_GE(o.tl, 0); EXPECT_LE(o.tl, 127);
                EXPECT_GE(o.ar, 0); EXPECT_LE(o.ar, 31);
                EXPECT_GE(o.d1r, 0); EXPECT_LE(o.d1r, 31);
                EXPECT_GE(o.d1l, 0); EXPECT_LE(o.d1l, 15);
                EXPECT_GE(o.d2r, 0); EXPECT_LE(o.d2r, 31);
                EXPECT_GE(o.rr, 0); EXPECT_LE(o.rr, 15);
                EXPECT_GE(o.ks, 0); EXPECT_LE(o.ks, 3);
                EXPECT_GE(o.mul, 0); EXPECT_LE(o.mul, 15);
                EXPECT_GE(o.dt1, 0); EXPECT_LE(o.dt1, 7);
                EXPECT_GE(o.dt2, 0); EXPECT_LE(o.dt2, 3);
                if (info.isCarrier(op) && o.tl <= 40 && o.ar >= 6) audible = true;
            }
            EXPECT_TRUE(audible) << "a carrier must be loud enough with a usable attack";
            EXPECT_GE(p.feedback, 0); EXPECT_LE(p.feedback, 7);
            EXPECT_LE(p.lfoPmd, 127); EXPECT_LE(p.lfoAmd, 127); EXPECT_LE(p.lfoRate, 255);
            EXPECT_LE(p.lfoPms, 7); EXPECT_LE(p.lfoAms, 3); EXPECT_LE(p.noiseFrequency, 31);
        }
    }
}

TEST(PatchGeneratorTest, AlgorithmStaysInsideTheCategory)
{
    for (int c = 0; c < kCategories; ++c) {
        GeneratorInput input;
        input.category = static_cast<GeneratorCategory>(c);
        const auto& allowed = PatchGenerator::algorithmCandidates(input.category);
        for (int seed = 0; seed < 100; ++seed) {
            input.complex = (seed % 11) / 10.0f;
            const int alg = PatchGenerator::generate(input, seed).algorithm;
            EXPECT_NE(std::find(allowed.begin(), allowed.end(), alg), allowed.end())
                << "category " << c << " produced algorithm " << alg;
        }
    }
}

TEST(PatchGeneratorTest, BrightnessLowersModulatorLevels)
{
    for (int c = 0; c < kCategories; ++c) {
        double darkSum = 0.0, brightSum = 0.0;
        int count = 0;
        for (int seed = 0; seed < 100; ++seed) {
            GeneratorInput input;
            input.category = static_cast<GeneratorCategory>(c);
            input.bright = 0.0f;
            const auto dark = PatchGenerator::generate(input, seed);
            input.bright = 1.0f;
            const auto bright = PatchGenerator::generate(input, seed);
            const auto& info = algorithmInfo(dark.algorithm);
            for (int op = 0; op < 4; ++op) {
                if (info.isCarrier(op)) continue;
                darkSum += dark.ops[static_cast<size_t>(op)].tl;
                ++count;
            }
            const auto& infoB = algorithmInfo(bright.algorithm);
            for (int op = 0; op < 4; ++op)
                if (!infoB.isCarrier(op)) brightSum += bright.ops[static_cast<size_t>(op)].tl;
        }
        if (count == 0) continue;     // categories that may land on algorithm 7 only
        EXPECT_LT(brightSum, darkSum) << "category " << c;
    }
}

TEST(PatchGeneratorTest, MovingTurnsTheLfoOn)
{
    GeneratorInput input;
    input.category = GeneratorCategory::Pad;
    input.moving = 0.0f;
    EXPECT_EQ(PatchGenerator::generate(input, 1).lfoPmd, 0);
    input.moving = 1.0f;
    const auto p = PatchGenerator::generate(input, 1);
    EXPECT_GT(p.lfoPmd, 0);
    EXPECT_GT(p.lfoPms, 0);
    EXPECT_GT(p.lfoRate, 0);
}
