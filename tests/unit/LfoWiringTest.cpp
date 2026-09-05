#include <gtest/gtest.h>
#include <cmath>
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"

// The hardware LFO only reaches the sound when the depth register and the
// channel sensitivity are both set; this renders the same note with and
// without vibrato / tremolo and expects the waveforms to differ.
class LfoWiringTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
        processor.setCurrentProgram(7);
    }
    void TearDown() override {
        processor.releaseResources();
        ymulatorsynth::ParameterManager::resetStaticState();
    }
    void set(const std::string& id, int value) {
        auto* p = processor.getParameters().getParameter(id);
        ASSERT_NE(p, nullptr);
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(value)));
    }
    std::vector<float> render(int blocks) {
        processor.releaseResources();
        processor.prepareToPlay(48000.0, 512);
        std::vector<float> out;
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, static_cast<juce::uint8>(100)), 0);
        for (int i = 0; i < blocks; ++i) {
            processor.processBlock(buffer, midi);
            midi.clear();
            out.insert(out.end(), buffer.getReadPointer(0), buffer.getReadPointer(0) + 512);
        }
        return out;
    }
    static double rmsDifference(const std::vector<float>& a, const std::vector<float>& b) {
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i) sum += (a[i] - b[i]) * (a[i] - b[i]);
        return std::sqrt(sum / static_cast<double>(a.size()));
    }
    
    YMulatorSynthAudioProcessor processor;
};

TEST_F(LfoWiringTest, PitchModulationReachesTheChip)
{
    set(ParamID::Global::LfoRate, 220);
    set(ParamID::Global::LfoWaveform, 2);
    set(ParamID::Global::LfoPms, 0);
    set(ParamID::Global::LfoPmd, 0);
    const auto still = render(40);
    
    set(ParamID::Global::LfoPmd, 127);
    set(ParamID::Global::LfoPms, 7);
    const auto vibrato = render(40);
    
    // Skip the shared attack; compare the sustained part
    std::vector<float> a(still.begin() + 4096, still.end()), b(vibrato.begin() + 4096, vibrato.end());
    EXPECT_GT(rmsDifference(a, b), 0.02) << "PMD 127 / PMS 7 must change the waveform";
    
    set(ParamID::Global::LfoPms, 0);
    const auto insensitive = render(40);
    std::vector<float> c(insensitive.begin() + 4096, insensitive.end());
    EXPECT_LT(rmsDifference(a, c), 1e-6) << "PMS 0 keeps the depth out of the sound";
}

TEST_F(LfoWiringTest, AmplitudeModulationNeedsDepthSensitivityAndOperatorEnable)
{
    set(ParamID::Global::LfoRate, 220);
    set(ParamID::Global::LfoWaveform, 2);
    set(ParamID::Global::LfoAmd, 0);
    set(ParamID::Global::LfoAms, 0);
    const auto still = render(40);
    
    set(ParamID::Global::LfoAmd, 127);
    set(ParamID::Global::LfoAms, 3);
    for (int op = 1; op <= 4; ++op) set(ParamID::Op::ams_en(op), 1);
    const auto tremolo = render(40);
    
    std::vector<float> a(still.begin() + 4096, still.end()), b(tremolo.begin() + 4096, tremolo.end());
    EXPECT_GT(rmsDifference(a, b), 0.01) << "AMD 127 / AMS 3 / AMS-EN must change the waveform";
}
