#include <gtest/gtest.h>
#include <cmath>
#include "dsp/ScopeBuffer.h"
#include "PluginProcessor.h"
#include "core/ParameterManager.h"

using ymulatorsynth::ScopeBuffer;

TEST(ScopeBufferTest, ReadsNewestSamplesInOrder)
{
    ScopeBuffer scope;
    std::vector<float> l(100), r(100);
    for (int i = 0; i < 100; ++i) { l[static_cast<size_t>(i)] = static_cast<float>(i); r[static_cast<size_t>(i)] = static_cast<float>(i); }
    scope.push(l.data(), r.data(), 100);
    
    float out[10];
    EXPECT_EQ(scope.readLatest(out, 10), 10u);
    for (int i = 0; i < 10; ++i) EXPECT_FLOAT_EQ(out[i], static_cast<float>(90 + i));
    
    float few[4];
    ScopeBuffer empty;
    EXPECT_EQ(empty.readLatest(few, 4), 0u);
    for (float v : few) EXPECT_FLOAT_EQ(v, 0.0f);
}

TEST(ScopeBufferTest, MixesLeftAndRightAndWrapsAround)
{
    ScopeBuffer scope;
    std::vector<float> l(ScopeBuffer::kSize + 5, 1.0f), r(ScopeBuffer::kSize + 5, -1.0f);
    scope.push(l.data(), r.data(), static_cast<int>(l.size()));
    float out[ScopeBuffer::kSize];
    EXPECT_EQ(scope.readLatest(out, ScopeBuffer::kSize), ScopeBuffer::kSize);
    EXPECT_FLOAT_EQ(out[0], 0.0f);
    EXPECT_EQ(scope.totalWritten(), ScopeBuffer::kSize + 5);
}

TEST(ScopeBufferTest, ProcessorFeedsTheScope)
{
    YMulatorSynthAudioProcessor processor;
    processor.setPlayConfigDetails(0, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    processor.setCurrentProgram(7);
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(120)), 0);
    for (int i = 0; i < 8; ++i) { processor.processBlock(buffer, midi); midi.clear(); }
    
    float out[512];
    ASSERT_EQ(processor.getScopeBuffer().readLatest(out, 512), 512u);
    float peak = 0.0f;
    for (float v : out) peak = std::max(peak, std::abs(v));
    EXPECT_GT(peak, 0.01f) << "a sounding note must show up in the scope";
    processor.releaseResources();
    ymulatorsynth::ParameterManager::resetStaticState();
}
