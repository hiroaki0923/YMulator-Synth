#include <gtest/gtest.h>
#include <cmath>
#include "core/PatchPreview.h"
#include "ui/OutputScope.h"
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"

using ymulatorsynth::PatchPreview;

namespace {
float peakOf(const std::vector<float>& v) { float p = 0.0f; for (float x : v) p = std::max(p, std::abs(x)); return p; }
}

TEST(PatchPreviewTest, RendersTheCurrentSoundDeterministically)
{
    YMulatorSynthAudioProcessor processor;
    processor.setCurrentProgram(3);
    ymulatorsynth::Preset preset;
    processor.extractCurrentPreset(preset);
    
    PatchPreview preview;
    const auto a = preview.render(preset, 24000);
    const auto b = preview.render(preset, 24000);
    ASSERT_EQ(a.size(), 24000u);
    EXPECT_GT(peakOf(a), 0.05f) << "a loaded preset must sound";
    EXPECT_EQ(a, b) << "same patch, same render";
    
    auto* tl = processor.getParameters().getParameter(ParamID::Op::tl(4));
    tl->setValueNotifyingHost(tl->convertTo0to1(40.0f));
    processor.extractCurrentPreset(preset);
    const auto quieter = preview.render(preset, 24000);
    EXPECT_LT(peakOf(quieter), peakOf(a)) << "carrier TL 40 is quieter";
    ymulatorsynth::ParameterManager::resetStaticState();
}

TEST(PatchPreviewTest, PeriodMatchesC4)
{
    EXPECT_NEAR(PatchPreview::periodInSamples(), 48000.0 / 261.6256, 0.01);
}

TEST(PatchPreviewTest, ReleaseFollowsTheKeyOff)
{
    YMulatorSynthAudioProcessor processor;
    processor.setCurrentProgram(7);                  // Init: sustains at full level, RR 15 releases fast
    ymulatorsynth::Preset preset;
    processor.extractCurrentPreset(preset);
    PatchPreview preview;
    const auto held = preview.render(preset, 24000);
    const auto released = preview.render(preset, 12000, 24000);
    ASSERT_EQ(released.size(), 24000u);
    EXPECT_EQ(std::vector<float>(held.begin(), held.begin() + 12000), std::vector<float>(released.begin(), released.begin() + 12000))
        << "identical until the key-off";
    EXPECT_GT(peakOf(std::vector<float>(held.begin() + 20000, held.end())), 0.05f) << "held: still sounding";
    EXPECT_LT(peakOf(std::vector<float>(released.begin() + 20000, released.end())), 1e-3f) << "released: gone";
    ymulatorsynth::ParameterManager::resetStaticState();
}

TEST(PatchPreviewTest, ScopeFollowsThePlayheadThroughTheRender)
{
    // A decaying tone: the trace at the start is larger than later on, and the envelope strip shows it
    std::vector<float> samples(48000);
    const double period = 48000.0 / 261.6256;
    for (size_t i = 0; i < samples.size(); ++i)
        samples[i] = static_cast<float>(std::exp(-3.0 * i / 48000.0) * std::sin(2.0 * juce::MathConstants<double>::pi * i / period));
    OutputScope scope;
    scope.setWaveform(samples, period, 48000.0, 24000);
    EXPECT_NEAR(scope.peakLevel(), 1.0f, 0.01f);   // a sampled, decaying sine only comes close to its analytic peak
    ASSERT_EQ(scope.shownFrame().size(), static_cast<size_t>(juce::roundToInt(period * OutputScope::kPeriods)));
    EXPECT_GE(scope.shownFrame()[1], 0.0f) << "the frame starts at a rising zero crossing";
    const float early = peakOf(scope.shownFrame());
    scope.setPlayhead(0.8);
    const float late = peakOf(scope.shownFrame());
    EXPECT_GT(early, late * 5.0f) << "the trace fades with the envelope";
    ASSERT_EQ(scope.envelope().size(), static_cast<size_t>(OutputScope::kEnvelopeBins));
    EXPECT_GT(scope.envelope().front(), scope.envelope().back() * 5.0f);
}
