#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"

// The per-block parameter update must only write registers that changed.
// Before this, every block rewrote all 8 x 4 x 11 operator registers.
class RegisterUpdateTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    void runBlocks(int n, juce::MidiBuffer* midi = nullptr) {
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer empty;
        for (int i = 0; i < n; ++i) processor.processBlock(buffer, midi ? *midi : empty);
        if (midi) midi->clear();
    }
    uint64_t writes() const { return processor.getYmfmWrapper().getRegisterWriteCount(); }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(RegisterUpdateTest, IdleBlocksWriteNothing)
{
    runBlocks(2);                       // settle: first block flushes every parameter
    const auto before = writes();
    runBlocks(20);
    EXPECT_EQ(writes(), before) << "no parameter changed, so no register may be written";
}

TEST_F(RegisterUpdateTest, ChangedParameterWritesOnlyItsRegisters)
{
    runBlocks(2);
    const auto before = writes();
    auto* tl = processor.getParameters().getParameter(ParamID::Op::tl(1));
    ASSERT_NE(tl, nullptr);
    tl->setValueNotifyingHost(tl->convertTo0to1(40.0f));
    runBlocks(1);
    // One TL register per channel, all 8 channels share the voice
    EXPECT_EQ(writes() - before, 8u);
    runBlocks(5);
    EXPECT_EQ(writes() - before, 8u) << "the change must not be re-sent";
}

TEST_F(RegisterUpdateTest, EveryParameterIsFlushedAfterPrepareToPlay)
{
    // A chip reset (releaseResources + prepareToPlay) must be followed by a full rewrite
    runBlocks(2);
    processor.releaseResources();
    processor.prepareToPlay(48000.0, 512);
    const auto before = writes();
    runBlocks(1);
    // 8 channels x (4 ops x 11 params) plus algorithm/feedback per channel and LFO/noise
    EXPECT_GE(writes() - before, 8u * 4u * 10u);
}
