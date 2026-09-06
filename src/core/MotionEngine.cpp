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
    wide = parameters.getParameter(ParamID::Motion::Wide);
    widePan = parameters.getParameter(ParamID::Motion::WidePan);
}

void MotionEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    for (auto& c : channels) c = Channel {};
    lastWideEnabled = false;
    lastWideCents = -1.0f;
    lastWidePan = -1;
}

float MotionEngine::read(const juce::RangedAudioParameter* param, float fallback) const
{
    return param ? param->convertFrom0to1(param->getValue()) : fallback;
}

void MotionEngine::tick(int numSamples)
{
    // Wide: forwarded to the chip pair only when it changes
    const float wideAmount = read(wide, 0.0f);
    const float wideCents = wideAmount / 100.0f * kMaxWideCents;
    const bool wideEnabled = wideAmount > 0.0f;
    const int panMode = juce::roundToInt(read(widePan, 0.0f));
    if (wideEnabled != lastWideEnabled || wideCents != lastWideCents || panMode != lastWidePan) {
        lastWideEnabled = wideEnabled;
        lastWideCents = wideCents;
        lastWidePan = panMode;
        ymfm.setWide(wideEnabled, wideCents, panMode == 0 ? YmfmWrapperInterface::WidePan::LeftRight : YmfmWrapperInterface::WidePan::Centre);
    }
    
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
