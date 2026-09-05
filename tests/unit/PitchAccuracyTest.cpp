#include <gtest/gtest.h>
#include "dsp/YmfmWrapper.h"
#include "PluginProcessor.h"
#include "core/ParameterManager.h"
#include "utils/ParameterIDs.h"
#include "dsp/YM2151Registers.h"
#include <cmath>
#include <vector>

// Pitch accuracy regression tests.
//
// The YM2151 core in ymfm runs at its own internal rate (clock / 64 = 55930 Hz
// for the standard 3.58 MHz clock). The wrapper must resample that stream to the
// host sample rate; if it does not, every note is transposed by
// 12 * log2(hostRate / 55930) semitones (about -4.1 at 44.1 kHz).
namespace {

constexpr int kBlockSize = 512;
constexpr double kSettleSeconds = 0.5;
constexpr double kMeasureSeconds = 1.5;

// Tolerance justification: pitch is estimated by counting positive zero
// crossings over 1.5 s, which resolves ~0.67 Hz (~2.6 cents at 440 Hz).
// The YM2151 KC/KF tuning table is itself quantised to 1/64 semitone (~1.6 cents).
// 10 cents covers both while still catching any semitone-level error.
constexpr double kToleranceCents = 10.0;

double midiNoteToHz(int note)
{
    return 440.0 * std::pow(2.0, (note - 69) / 12.0);
}

double centsBetween(double measuredHz, double expectedHz)
{
    return 1200.0 * std::log2(measuredHz / expectedHz);
}

// Estimate the fundamental of a (near) sinusoidal signal from positive-going
// zero crossings, ignoring the initial attack.
double estimateFrequency(const std::vector<float>& samples, double sampleRate)
{
    const size_t start = static_cast<size_t>(sampleRate * kSettleSeconds);
    if (samples.size() <= start + 2) return 0.0;

    int crossings = 0;
    for (size_t i = start + 1; i < samples.size(); ++i) {
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f) ++crossings;
    }
    const double seconds = static_cast<double>(samples.size() - start) / sampleRate;
    return crossings / seconds;
}

// Configure channel 0 as a single sine carrier: algorithm 7 (all carriers),
// operators 1-3 muted, operator 4 at full level with MUL=1, no detune.
void configureSineCarrier(YmfmWrapperInterface& wrapper, uint8_t channel = 0)
{
    using P = YmfmWrapperInterface::OperatorParameter;
    wrapper.setAlgorithm(channel, 7);
    wrapper.setFeedback(channel, 0);
    wrapper.setChannelPan(channel, 0.5f); // L/R output bits are clear after chip reset
    for (uint8_t op = 0; op < 4; ++op) {
        const bool carrier = (op == 3);
        wrapper.setOperatorParameter(channel, op, P::TotalLevel, carrier ? 0 : 127);
        wrapper.setOperatorParameter(channel, op, P::AttackRate, 31);
        wrapper.setOperatorParameter(channel, op, P::Decay1Rate, 0);
        wrapper.setOperatorParameter(channel, op, P::SustainLevel, 0);
        wrapper.setOperatorParameter(channel, op, P::Decay2Rate, 0);
        wrapper.setOperatorParameter(channel, op, P::ReleaseRate, 15);
        wrapper.setOperatorParameter(channel, op, P::KeyScale, 0);
        wrapper.setOperatorParameter(channel, op, P::Multiple, 1);
        wrapper.setOperatorParameter(channel, op, P::Detune1, 0);
        wrapper.setOperatorParameter(channel, op, P::Detune2, 0);
    }
}

double measureWrapperPitch(double sampleRate, int note, float pitchBendSemitones = 0.0f)
{
    YmfmWrapper wrapper;
    wrapper.initialize(YmfmWrapperInterface::ChipType::OPM, static_cast<uint32_t>(sampleRate));
    configureSineCarrier(wrapper);
    wrapper.noteOn(0, static_cast<uint8_t>(note), 127);
    if (pitchBendSemitones != 0.0f) wrapper.setPitchBend(0, pitchBendSemitones);

    std::vector<float> left, right(kBlockSize);
    const int totalSamples = static_cast<int>(sampleRate * (kSettleSeconds + kMeasureSeconds));
    std::vector<float> block(kBlockSize);
    for (int generated = 0; generated < totalSamples; generated += kBlockSize) {
        wrapper.generateSamples(block.data(), right.data(), kBlockSize);
        left.insert(left.end(), block.begin(), block.end());
    }
    return estimateFrequency(left, sampleRate);
}

double measureProcessorPitch(double sampleRate, int note)
{
    YMulatorSynthAudioProcessor processor;
    processor.setPlayConfigDetails(0, 2, sampleRate, kBlockSize);
    processor.prepareToPlay(sampleRate, kBlockSize);

    auto set = [&](const juce::String& id, float normalised) {
        auto* param = processor.getParameters().getParameter(id);
        ASSERT_NE(param, nullptr) << id;
        param->setValueNotifyingHost(normalised);
    };
    set(ParamID::Global::Algorithm, 1.0f);
    set(ParamID::Global::Feedback, 0.0f);
    set(ParamID::Global::LfoPmd, 0.0f);
    set(ParamID::Global::LfoAmd, 0.0f);
    for (int op = 1; op <= 4; ++op) {
        set(ParamID::Op::tl(op), op == 4 ? 0.0f : 1.0f);
        set(ParamID::Op::ar(op), 1.0f);
        set(ParamID::Op::d1r(op), 0.0f);
        set(ParamID::Op::d2r(op), 0.0f);
        set(ParamID::Op::mul(op), 1.0f / 15.0f);
        set(ParamID::Op::dt1(op), 0.0f);
        set(ParamID::Op::dt2(op), 0.0f);
    }

    juce::AudioBuffer<float> buffer(2, kBlockSize);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(127)), 0);

    std::vector<float> left;
    const int totalSamples = static_cast<int>(sampleRate * (kSettleSeconds + kMeasureSeconds));
    for (int generated = 0; generated < totalSamples; generated += kBlockSize) {
        buffer.clear();
        processor.processBlock(buffer, midi);
        midi.clear();
        const float* data = buffer.getReadPointer(0);
        left.insert(left.end(), data, data + kBlockSize);
    }
    processor.releaseResources();
    return estimateFrequency(left, sampleRate);
}

} // namespace

class PitchAccuracyTest : public ::testing::Test {
protected:
    void TearDown() override { ymulatorsynth::ParameterManager::resetStaticState(); }
};

TEST_F(PitchAccuracyTest, WrapperA4IsConcertPitchAtCommonSampleRates)
{
    for (double sampleRate : {44100.0, 48000.0, 96000.0}) {
        const double measured = measureWrapperPitch(sampleRate, 69);
        EXPECT_NEAR(centsBetween(measured, 440.0), 0.0, kToleranceCents)
            << "sampleRate=" << sampleRate << " measured=" << measured << " Hz";
    }
}

TEST_F(PitchAccuracyTest, WrapperOctavesTrackMidiNotes)
{
    for (int note : {45, 57, 69, 81, 93}) {
        const double measured = measureWrapperPitch(48000.0, note);
        EXPECT_NEAR(centsBetween(measured, midiNoteToHz(note)), 0.0, kToleranceCents)
            << "note=" << note << " measured=" << measured << " Hz";
    }
}

TEST_F(PitchAccuracyTest, WrapperPitchBendUsesKeyFraction)
{
    // Tolerance justification: KF resolution is 1/64 semitone (~1.6 cents), and the
    // fractional part is truncated, so a bend can land up to one KF step flat.
    constexpr double kBendToleranceCents = kToleranceCents + 1200.0 / 64.0 / 12.0;
    for (float bend : {2.0f, -2.0f, 0.5f, -7.0f}) {
        const double measured = measureWrapperPitch(48000.0, 69, bend);
        const double expected = 440.0 * std::pow(2.0, bend / 12.0);
        EXPECT_NEAR(centsBetween(measured, expected), 0.0, kBendToleranceCents)
            << "bend=" << bend << " measured=" << measured << " Hz";
    }
}

TEST_F(PitchAccuracyTest, ProcessorA4IsConcertPitchAtCommonSampleRates)
{
    for (double sampleRate : {44100.0, 48000.0}) {
        const double measured = measureProcessorPitch(sampleRate, 69);
        EXPECT_NEAR(centsBetween(measured, 440.0), 0.0, kToleranceCents)
            << "sampleRate=" << sampleRate << " measured=" << measured << " Hz";
    }
}
