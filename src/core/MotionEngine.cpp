#include "MotionEngine.h"
#include "../utils/ParameterIDs.h"
#include <cmath>

namespace ymulatorsynth {

MotionEngine::MotionEngine(YmfmWrapperInterface& wrapper, VoiceManagerInterface& voiceManager)
    : ymfm(wrapper), voices(voiceManager)
{
}

void MotionEngine::bindParameters(juce::AudioProcessorValueTreeState& parameters)
{
    vibratoDepth = parameters.getParameter(ParamID::Motion::VibratoDepth);
    vibratoRate = parameters.getParameter(ParamID::Motion::VibratoRate);
    vibratoDelay = parameters.getParameter(ParamID::Motion::VibratoDelay);
    vibratoRise = parameters.getParameter(ParamID::Motion::VibratoRise);
}

void MotionEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    for (auto& c : channels) c = Channel {};
}

float MotionEngine::read(const juce::RangedAudioParameter* param, float fallback) const
{
    return param ? param->convertFrom0to1(param->getValue()) : fallback;
}

void MotionEngine::tick(int numSamples)
{
    const double dt = static_cast<double>(numSamples) / sampleRate;
    const float depthSemitones = read(vibratoDepth, 0.0f) / 100.0f * kMaxVibratoCents / 100.0f;
    const double rateHz = read(vibratoRate, 5.0f);
    const double delay = read(vibratoDelay, 0.0f) / 1000.0;
    const double rise = read(vibratoRise, 0.0f) / 1000.0;
    
    for (int ch = 0; ch < 8; ++ch) {
        auto& c = channels[static_cast<size_t>(ch)];
        const bool active = voices.isVoiceActive(ch);
        const int note = active ? voices.getNoteForChannel(ch) : -1;
        if (active && (!c.active || note != c.note)) {
            c.time = 0.0;
            c.phase = 0.0;
        }
        c.active = active;
        c.note = note;
        
        float offset = 0.0f;
        if (active && depthSemitones > 0.0f) {
            c.time += dt;
            c.phase = std::fmod(c.phase + rateHz * dt, 1.0);
            const double sinceDelay = c.time - delay;
            const double envelope = sinceDelay <= 0.0 ? 0.0 : (rise <= 0.0 ? 1.0 : std::min(1.0, sinceDelay / rise));
            offset = static_cast<float>(depthSemitones * envelope * std::sin(juce::MathConstants<double>::twoPi * c.phase));
        }
        if (offset != c.offset) {
            c.offset = offset;
            ymfm.setChannelPitchOffset(static_cast<uint8_t>(ch), offset);
        }
    }
}

} // namespace ymulatorsynth
