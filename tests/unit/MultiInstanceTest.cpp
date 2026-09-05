#include <gtest/gtest.h>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include <cmath>
#include <memory>

// Several plugin instances in one process (a DAW with two tracks of the synth)
// prepare on the same thread with the same sample rate. Initialisation state
// must therefore be per instance: a shared flag made every instance after the
// first skip ymfm initialisation and stay silent.
namespace {

float peakBlockRms(YMulatorSynthAudioProcessor& processor)
{
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
    float peak = 0.0f;
    for (int block = 0; block < 40; ++block) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        midi.clear();
        double acc = 0.0;
        for (int i = 0; i < 512; ++i) acc += buffer.getSample(0, i) * buffer.getSample(0, i);
        peak = std::max(peak, static_cast<float>(std::sqrt(acc / 512.0)));
    }
    return peak;
}

} // namespace

class MultiInstanceTest : public ::testing::Test {
protected:
    void TearDown() override { ymulatorsynth::ParameterManager::resetStaticState(); }
};

TEST_F(MultiInstanceTest, SecondInstanceOnSameThreadProducesAudio)
{
    auto first = std::make_unique<YMulatorSynthAudioProcessor>();
    auto second = std::make_unique<YMulatorSynthAudioProcessor>();
    for (auto* p : {first.get(), second.get()}) {
        p->setPlayConfigDetails(0, 2, 48000.0, 512);
        p->prepareToPlay(48000.0, 512);
    }

    EXPECT_GT(peakBlockRms(*first), 0.01f) << "first instance must sound";
    EXPECT_GT(peakBlockRms(*second), 0.01f) << "second instance must sound too";

    second->releaseResources();
    first->releaseResources();
}

TEST_F(MultiInstanceTest, InstanceCreatedAfterAnotherWasReleasedProducesAudio)
{
    {
        YMulatorSynthAudioProcessor earlier;
        earlier.setPlayConfigDetails(0, 2, 48000.0, 512);
        earlier.prepareToPlay(48000.0, 512);
        EXPECT_GT(peakBlockRms(earlier), 0.01f);
        earlier.releaseResources();
    }
    YMulatorSynthAudioProcessor later;
    later.setPlayConfigDetails(0, 2, 48000.0, 512);
    later.prepareToPlay(48000.0, 512);
    EXPECT_GT(peakBlockRms(later), 0.01f) << "a fresh instance must not inherit initialisation state";
    later.releaseResources();
}
