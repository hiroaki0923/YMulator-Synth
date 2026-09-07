#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/MacroMapper.h"
#include "core/ParameterManager.h"
#include "dsp/AlgorithmInfo.h"
#include "utils/ParameterIDs.h"

using namespace ymulatorsynth;

namespace {

RawPatch samplePatch()
{
    RawPatch p;
    p.tl  = { 30, 10, 45, 5 };
    p.ar  = { 31, 25, 20, 28 };
    p.d1r = { 12, 8, 14, 6 };
    p.d2r = { 4, 2, 6, 3 };
    p.rr  = { 8, 6, 9, 7 };
    p.dt1 = { 0, 5, 2, 0 };     // 0, -1, +2, 0
    p.mul = { 1, 1, 2, 2 };
    p.feedback = 4;
    return p;
}

} // namespace

// ============================================================================
// Pure mapping
// ============================================================================

TEST(MacroMapperPureTest, CentreIsIdentityForEveryAlgorithm)
{
    const RawPatch anchor = samplePatch();
    for (int alg = 0; alg < 8; ++alg)
        EXPECT_EQ(MacroMapper::apply(anchor, MacroValues{}, alg), anchor) << "algorithm " << alg;
}

TEST(MacroMapperPureTest, BrightnessLowersModulatorLevelOnly)
{
    const RawPatch anchor = samplePatch();
    MacroValues m;
    m.brightness = 0.5f;                        // 20 steps
    const auto out = MacroMapper::apply(anchor, m, 4);   // carriers: op2, op4
    EXPECT_EQ(out.tl[0], 10);
    EXPECT_EQ(out.tl[2], 25);
    EXPECT_EQ(out.tl[1], anchor.tl[1]);
    EXPECT_EQ(out.tl[3], anchor.tl[3]);
    EXPECT_EQ(out.feedback, anchor.feedback);
    
    m.brightness = 1.0f;
    EXPECT_EQ(MacroMapper::apply(anchor, m, 4).tl[0], 0) << "clamped at 0";
    m.brightness = -1.0f;
    EXPECT_EQ(MacroMapper::apply(anchor, m, 4).tl[0], 70);
}

TEST(MacroMapperPureTest, BrightnessMonotonic)
{
    const RawPatch anchor = samplePatch();
    int previous = 128;
    for (float m = -1.0f; m <= 1.0f; m += 0.1f) {
        MacroValues v; v.brightness = m;
        const int tl = MacroMapper::apply(anchor, v, 0).tl[0];
        EXPECT_LE(tl, previous);
        previous = tl;
    }
}

TEST(MacroMapperPureTest, BrightnessDrivesFeedbackWhenNoModulator)
{
    const RawPatch anchor = samplePatch();
    MacroValues m; m.brightness = 1.0f;
    const auto out = MacroMapper::apply(anchor, m, 7);
    EXPECT_EQ(out.feedback, 7);
    EXPECT_EQ(out.tl, anchor.tl);
    m.brightness = -1.0f;
    EXPECT_EQ(MacroMapper::apply(anchor, m, 7).feedback, 0);
}

TEST(MacroMapperPureTest, TargetsFollowAlgorithmRoles)
{
    for (int alg = 0; alg < 8; ++alg) {
        const auto& info = kAlgorithms[static_cast<size_t>(alg)];
        const auto targets = MacroMapper::targetsOf(Macro::Brightness, alg);
        if (alg == 7) {
            ASSERT_EQ(targets.size(), 1u);
            EXPECT_EQ(targets[0], ParamID::Global::Feedback);
            continue;
        }
        std::vector<std::string> expected;
        for (int op = 1; op <= 4; ++op) if (info.isModulator(op - 1)) expected.push_back(ParamID::Op::tl(op));
        EXPECT_EQ(targets, expected) << "algorithm " << alg;
    }
    EXPECT_EQ(MacroMapper::targetsOf(Macro::Spread, 0).size(), 3u);
    EXPECT_EQ(MacroMapper::targetsOf(Macro::Decay, 0).size(), 2u) << "algorithm 0 has one carrier";
    EXPECT_EQ(MacroMapper::targetsOf(Macro::Attack, 7).size(), 4u) << "algorithm 7 is all carriers";
    EXPECT_EQ(MacroMapper::targetsOf(Macro::Release, 4), (std::vector<std::string>{ ParamID::Op::rr(2), ParamID::Op::rr(4) }));
}

TEST(MacroMapperPureTest, HarmonicsTemplatesOnTwoPairs)
{
    const RawPatch anchor = samplePatch();          // mul {1,1,2,2}, alg 4: 1>2, 3>4
    MacroValues m;
    m.harmonics = HarmonicsTemplate::Saw;
    auto out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.mul, (std::array<int, 4>{ 1, 1, 2, 2 }));
    m.harmonics = HarmonicsTemplate::Square;
    out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.mul, (std::array<int, 4>{ 2, 1, 4, 2 }));
    m.harmonics = HarmonicsTemplate::Bell;          // x3.5, rounded
    out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.mul, (std::array<int, 4>{ 4, 1, 7, 2 }));
    m.harmonics = HarmonicsTemplate::Sub;           // carriers halved, MUL 1 -> 0 (x0.5)
    out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.mul, (std::array<int, 4>{ 1, 0, 2, 1 }));
    m.harmonics = HarmonicsTemplate::Octave;
    out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.mul, (std::array<int, 4>{ 2, 2, 4, 4 }));
}

TEST(MacroMapperPureTest, HarmonicsChainUsesNextStage)
{
    RawPatch anchor = samplePatch();
    anchor.mul = { 1, 1, 1, 3 };
    MacroValues m; m.harmonics = HarmonicsTemplate::Square;
    const auto out = MacroMapper::apply(anchor, m, 0);   // 1>2>3>4
    EXPECT_EQ(out.mul[3], 3);
    EXPECT_EQ(out.mul[2], 6);
    EXPECT_EQ(out.mul[1], 12);
    EXPECT_EQ(out.mul[0], 15) << "clamped";
    
    // Without modulators only Sub and Octave have an effect
    EXPECT_EQ(MacroMapper::apply(anchor, m, 7).mul, anchor.mul);
}

TEST(MacroMapperPureTest, EnvelopeMacros)
{
    const RawPatch anchor = samplePatch();
    MacroValues m;
    m.attack = 1.0f; m.decay = 1.0f; m.release = 1.0f;
    const auto out = MacroMapper::apply(anchor, m, 4);
    // the loudness envelope: carriers (op2, op4) move by 12 / 10 / 6 / 6, modulators keep their timbre envelope
    EXPECT_EQ(out.ar, (std::array<int, 4>{ 31, 13, 20, 16 }));
    EXPECT_EQ(out.d1r, (std::array<int, 4>{ 12, 0, 14, 0 }));
    EXPECT_EQ(out.d2r, (std::array<int, 4>{ 4, 0, 6, 0 }));
    EXPECT_EQ(out.rr,  (std::array<int, 4>{ 8, 0, 9, 1 }));
    EXPECT_EQ(out.tl, anchor.tl);
}

TEST(MacroMapperPureTest, SpreadKeepsSignsAndFixesOperator4)
{
    RawPatch anchor = samplePatch();
    MacroValues m; m.spread = 1.0f;
    auto out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.dt1, (std::array<int, 4>{ 3, 7, 3, 0 }));   // +3, -3, +3 (was +2), op4 untouched
    
    anchor.dt1 = { 0, 0, 0, 0 };
    out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.dt1, (std::array<int, 4>{ 3, 7, 3, 0 }));   // default signs +, -, +
    
    m.spread = -1.0f;
    anchor = samplePatch();
    out = MacroMapper::apply(anchor, m, 4);
    EXPECT_EQ(out.dt1, (std::array<int, 4>{ 0, 0, 0, 0 }));
}

TEST(MacroMapperPureTest, DetuneEncodingRoundTrip)
{
    for (int v = 0; v < 8; ++v) {
        const int decoded = MacroMapper::decodeDetune1(v);
        EXPECT_EQ(MacroMapper::encodeDetune1(decoded), v == 4 ? 0 : v) << "register " << v;
    }
}

// ============================================================================
// Against the parameter tree
// ============================================================================

class MacroMapperProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);     // Init: algorithm 7 is all carriers; pick a preset with modulators
        setRaw(ParamID::Global::Algorithm, 4);
        processor.getMacroMapper().captureAnchor();
    }
    void TearDown() override {
        processor.releaseResources();
        ParameterManager::resetStaticState();
    }
    int raw(const std::string& id) {
        auto* p = processor.getParameters().getParameter(id);
        return juce::roundToInt(p->convertFrom0to1(p->getValue()));
    }
    void setRaw(const std::string& id, int value) {
        auto* p = processor.getParameters().getParameter(id);
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
    }
    void setMacro(const char* id, float display) { setRaw(id, static_cast<int>(display)); }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(MacroMapperProcessorTest, MacroMovesRawAndCentreRestoresExactly)
{
    auto& mapper = processor.getMacroMapper();
    setRaw(ParamID::Op::tl(1), 50);
    mapper.captureAnchor();
    const RawPatch anchor = mapper.getAnchor();
    
    setMacro(ParamID::Macro::Brightness, 25.0f);      // m = 0.5 -> 20 steps
    EXPECT_EQ(raw(ParamID::Op::tl(1)), 30);
    EXPECT_EQ(raw(ParamID::Op::tl(2)), anchor.tl[1]) << "carrier untouched";
    EXPECT_TRUE(mapper.isEdited());
    
    setMacro(ParamID::Macro::Brightness, 0.0f);
    EXPECT_EQ(mapper.currentRaw(), anchor);
    EXPECT_FALSE(mapper.isEdited());
}

TEST_F(MacroMapperProcessorTest, DirectEditRebasesAnchorAndRoundTrips)
{
    auto& mapper = processor.getMacroMapper();
    setRaw(ParamID::Op::tl(1), 50);
    mapper.captureAnchor();
    
    setMacro(ParamID::Macro::Brightness, 25.0f);
    setRaw(ParamID::Op::tl(1), 40);                   // Detail edit while the macro is off centre
    EXPECT_EQ(mapper.getAnchor().tl[0], 60);
    EXPECT_FLOAT_EQ(mapper.currentMacros().brightness, 0.5f) << "macro position is kept";
    
    setMacro(ParamID::Macro::Brightness, -25.0f);
    EXPECT_EQ(raw(ParamID::Op::tl(1)), 80);
    setMacro(ParamID::Macro::Brightness, 25.0f);
    EXPECT_EQ(raw(ParamID::Op::tl(1)), 40) << "returns to the edited value";
}

TEST_F(MacroMapperProcessorTest, AlgorithmChangeAppliesOnNextMacroMove)
{
    auto& mapper = processor.getMacroMapper();
    setRaw(ParamID::Op::tl(2), 50);
    mapper.captureAnchor();
    setMacro(ParamID::Macro::Brightness, 25.0f);
    EXPECT_EQ(raw(ParamID::Op::tl(2)), 50) << "op2 is a carrier in algorithm 4";
    setRaw(ParamID::Global::Algorithm, 0);            // op2 becomes a modulator; nothing else moves yet
    EXPECT_EQ(raw(ParamID::Op::tl(2)), 50);
    setMacro(ParamID::Macro::Brightness, 26.0f);      // 0.52 -> 21 steps
    EXPECT_EQ(raw(ParamID::Op::tl(2)), 29);
    EXPECT_EQ(mapper.getAnchor().tl[1], 50);
}

TEST_F(MacroMapperProcessorTest, PresetLoadResetsMacrosAndAnchor)
{
    auto& mapper = processor.getMacroMapper();
    setMacro(ParamID::Macro::Brightness, 25.0f);
    setMacro(ParamID::Macro::Harmonics, 2.0f);
    processor.setCurrentProgram(3);
    EXPECT_EQ(raw(ParamID::Macro::Brightness), 0);
    EXPECT_EQ(raw(ParamID::Macro::Harmonics), 0);
    EXPECT_EQ(mapper.getAnchor(), mapper.currentRaw());
    EXPECT_FALSE(mapper.isEdited());
    EXPECT_FALSE(processor.isInCustomMode());
}

TEST_F(MacroMapperProcessorTest, StateRoundTripKeepsAnchorAndMacros)
{
    auto& mapper = processor.getMacroMapper();
    setMacro(ParamID::Macro::Brightness, 25.0f);
    setRaw(ParamID::Op::tl(1), 33);
    setMacro(ParamID::Macro::Spread, -10.0f);
    const RawPatch anchor = mapper.getAnchor();
    const RawPatch rawBefore = mapper.currentRaw();
    
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    processor.setCurrentProgram(5);
    ASSERT_NE(mapper.getAnchor(), anchor);
    
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    EXPECT_EQ(mapper.getAnchor(), anchor);
    EXPECT_EQ(mapper.currentRaw(), rawBefore);
    EXPECT_EQ(raw(ParamID::Macro::Brightness), 25);
    EXPECT_EQ(raw(ParamID::Macro::Spread), -10);
}

TEST_F(MacroMapperProcessorTest, StateWithoutAnchorNodeAnchorsCurrentRaw)
{
    auto& mapper = processor.getMacroMapper();
    setRaw(ParamID::Op::tl(1), 33);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    
    std::unique_ptr<juce::XmlElement> xml(juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize())));
    ASSERT_NE(xml, nullptr);
    auto* anchorNode = xml->getChildByName(MacroMapper::anchorNodeType.toString());
    ASSERT_NE(anchorNode, nullptr);
    xml->removeChildElement(anchorNode, true);
    juce::MemoryBlock legacy;
    juce::AudioProcessor::copyXmlToBinary(*xml, legacy);
    
    setMacro(ParamID::Macro::Brightness, 25.0f);
    processor.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
    EXPECT_EQ(mapper.getAnchor(), mapper.currentRaw());
    EXPECT_EQ(raw(ParamID::Op::tl(1)), 33);
    EXPECT_FALSE(mapper.isEdited());
}
