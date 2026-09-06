#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "core/PatchWorkspace.h"
#include "utils/ParameterIDs.h"

using namespace ymulatorsynth;

class PatchWorkspaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(3);
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
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(PatchWorkspaceTest, GenerateWritesThePatchAndReanchors)
{
    auto& ws = processor.getPatchWorkspace();
    auto& mapper = processor.getMacroMapper();
    setRaw(ParamID::Macro::Brightness, 20);
    
    GeneratorInput input;
    input.category = GeneratorCategory::Brass;
    const auto patch = ws.generate(input, 7);
    
    EXPECT_EQ(raw(ParamID::Global::Algorithm), patch.algorithm);
    EXPECT_EQ(raw(ParamID::Global::Feedback), patch.feedback);
    for (int op = 1; op <= 4; ++op) {
        const auto& o = patch.ops[static_cast<size_t>(op - 1)];
        EXPECT_EQ(raw(ParamID::Op::tl(op)), o.tl);
        EXPECT_EQ(raw(ParamID::Op::mul(op)), o.mul);
        EXPECT_EQ(raw(ParamID::Op::dt1(op)), o.dt1);
        EXPECT_EQ(raw(ParamID::Op::ar(op)), o.ar);
    }
    EXPECT_EQ(raw(ParamID::Global::LfoPmd), patch.lfoPmd);
    EXPECT_EQ(raw(ParamID::Macro::Brightness), 0) << "macros return to centre";
    EXPECT_EQ(mapper.getAnchor(), mapper.currentRaw());
    EXPECT_TRUE(processor.isInCustomMode());
    EXPECT_EQ(ws.activeSlot(), PatchWorkspace::Slot::B);
    EXPECT_TRUE(ws.hasSlot(PatchWorkspace::Slot::A));
}

TEST_F(PatchWorkspaceTest, UndoRestoresExactlyWhatWasThere)
{
    auto& ws = processor.getPatchWorkspace();
    setRaw(ParamID::Macro::Brightness, 10);
    setRaw(ParamID::Op::tl(1), 33);              // direct edit after the macro: raw stays 33
    const auto before = ws.capture();
    
    GeneratorInput input;
    ws.generate(input, 99);
    ASSERT_FALSE(ws.capture() == before);
    
    EXPECT_TRUE(ws.canUndo());
    EXPECT_TRUE(ws.undo());
    EXPECT_TRUE(ws.capture() == before);
    EXPECT_EQ(raw(ParamID::Op::tl(1)), 33);
    EXPECT_EQ(raw(ParamID::Macro::Brightness), 10);
    EXPECT_FALSE(ws.undo()) << "nothing left";
}

TEST_F(PatchWorkspaceTest, CompareSlotsSwitchBetweenBeforeAndAfter)
{
    auto& ws = processor.getPatchWorkspace();
    const auto before = ws.capture();
    GeneratorInput input;
    input.category = GeneratorCategory::Bell;
    ws.generate(input, 5);
    const auto generated = ws.capture();
    
    ws.selectSlot(PatchWorkspace::Slot::A);
    EXPECT_TRUE(ws.capture() == before);
    setRaw(ParamID::Op::tl(2), 50);              // tweak A, it must stay with A
    ws.selectSlot(PatchWorkspace::Slot::B);
    EXPECT_TRUE(ws.capture() == generated);
    ws.selectSlot(PatchWorkspace::Slot::A);
    EXPECT_EQ(raw(ParamID::Op::tl(2)), 50);
}

TEST_F(PatchWorkspaceTest, ToneGesturePushesAnUndoPoint)
{
    auto& ws = processor.getPatchWorkspace();
    EXPECT_FALSE(ws.canUndo());
    auto* bright = processor.getParameters().getParameter(ParamID::Macro::Brightness);
    const auto before = ws.capture();
    bright->beginChangeGesture();
    bright->setValueNotifyingHost(bright->convertTo0to1(30.0f));
    bright->endChangeGesture();
    EXPECT_TRUE(ws.canUndo());
    EXPECT_TRUE(ws.undo());
    EXPECT_TRUE(ws.capture() == before);
}

TEST_F(PatchWorkspaceTest, UndoDepthIsBounded)
{
    auto& ws = processor.getPatchWorkspace();
    GeneratorInput input;
    for (int i = 0; i < 20; ++i) ws.generate(input, i);
    int undone = 0;
    while (ws.undo()) ++undone;
    EXPECT_EQ(undone, static_cast<int>(SnapshotStore::kMaxUndo));
}
