#include <gtest/gtest.h>
#include <cmath>
#include "core/PatchPreview.h"
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
