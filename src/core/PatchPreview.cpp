#include "PatchPreview.h"
#include "../dsp/YM2151Registers.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace ymulatorsynth {

PatchPreview::PatchPreview()
{
    chip.initialize(YmfmWrapperInterface::ChipType::OPM, static_cast<uint32_t>(kSampleRate));
}

double PatchPreview::periodInSamples()
{
    return kSampleRate / juce::MidiMessage::getMidiNoteInHertz(kNote);
}

std::vector<float> PatchPreview::render(const Preset& preset, int numSamples)
{
    return render(preset, numSamples, numSamples);
}

std::vector<float> PatchPreview::render(const Preset& preset, int holdSamples, int numSamples)
{
    chip.reset();
    constexpr uint8_t ch = 0;
    using P = YmfmWrapperInterface::OperatorParameter;
    
    chip.setAlgorithm(ch, static_cast<uint8_t>(preset.algorithm));
    chip.setFeedback(ch, static_cast<uint8_t>(preset.feedback));
    chip.setChannelPan(ch, 0.5f);
    uint8_t slotMask = 0;
    for (uint8_t op = 0; op < 4; ++op) {
        const auto& o = preset.operators[op];
        chip.setOperatorParameter(ch, op, P::TotalLevel, static_cast<uint8_t>(o.totalLevel));
        chip.setOperatorParameter(ch, op, P::AttackRate, static_cast<uint8_t>(o.attackRate));
        chip.setOperatorParameter(ch, op, P::Decay1Rate, static_cast<uint8_t>(o.decay1Rate));
        chip.setOperatorParameter(ch, op, P::SustainLevel, static_cast<uint8_t>(o.sustainLevel));
        chip.setOperatorParameter(ch, op, P::Decay2Rate, static_cast<uint8_t>(o.decay2Rate));
        chip.setOperatorParameter(ch, op, P::ReleaseRate, static_cast<uint8_t>(o.releaseRate));
        chip.setOperatorParameter(ch, op, P::KeyScale, static_cast<uint8_t>(o.keyScale));
        chip.setOperatorParameter(ch, op, P::Multiple, static_cast<uint8_t>(o.multiple));
        chip.setOperatorParameter(ch, op, P::Detune1, static_cast<uint8_t>(o.detune1));
        chip.setOperatorParameter(ch, op, P::Detune2, static_cast<uint8_t>(o.detune2));
        chip.setOperatorAmsEnable(ch, op, o.amsEnable);
        if (o.slotEnable) slotMask = static_cast<uint8_t>(slotMask | (1u << op));
    }
    chip.setChannelSlotMask(ch, slotMask);
    chip.setLfoParameters(static_cast<uint8_t>(preset.lfo.rate), static_cast<uint8_t>(preset.lfo.amd),
                          static_cast<uint8_t>(preset.lfo.pmd), static_cast<uint8_t>(preset.lfo.waveform));
    chip.setChannelAmsPms(ch, static_cast<uint8_t>(preset.channels[0].ams), static_cast<uint8_t>(preset.channels[0].pms));
    chip.setNoiseParameters(preset.channels[0].noiseEnable != 0, static_cast<uint8_t>(preset.lfo.noiseFreq));
    
    chip.noteOn(ch, static_cast<uint8_t>(kNote), YM2151Regs::MAX_VELOCITY);
    left.assign(static_cast<size_t>(numSamples), 0.0f);
    right.assign(static_cast<size_t>(numSamples), 0.0f);
    const int hold = juce::jlimit(0, numSamples, holdSamples);
    chip.generateSamples(left.data(), right.data(), hold);
    if (hold < numSamples) {
        chip.noteOff(ch, static_cast<uint8_t>(kNote));
        chip.generateSamples(left.data() + hold, right.data() + hold, numSamples - hold);
    }
    
    std::vector<float> mono(static_cast<size_t>(numSamples));
    for (size_t i = 0; i < mono.size(); ++i) mono[i] = 0.5f * (left[i] + right[i]);
    return mono;
}

} // namespace ymulatorsynth
