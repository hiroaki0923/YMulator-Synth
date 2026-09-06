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
    timbreDepth = parameters.getParameter(ParamID::Motion::TimbreDepth);
    timbreRate = parameters.getParameter(ParamID::Motion::TimbreRate);
    tremoloDepth = parameters.getParameter(ParamID::Motion::TremoloDepth);
    tremoloRate = parameters.getParameter(ParamID::Motion::TremoloRate);
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
    const float timbreSteps = read(timbreDepth, 0.0f);
    const double timbreHz = read(timbreRate, 1.0f);
    const float tremoloSteps = read(tremoloDepth, 0.0f);
    const double tremoloHz = read(tremoloRate, 5.0f);
    
    for (int ch = 0; ch < 8; ++ch) {
        auto& c = channels[static_cast<size_t>(ch)];
        const bool active = voices.isVoiceActive(ch);
        const int note = active ? voices.getNoteForChannel(ch) : -1;
        if (active && (!c.active || note != c.note)) {
            c.time = 0.0;
            c.phase = 0.0;
            c.timbrePhase = 0.0;
            c.tremoloPhase = 0.0;
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
        
        // Timbre LFO: triangle on the modulators (both directions); tremolo: carriers only attenuate
        int carrierSteps = 0, modulatorSteps = 0;
        if (active) {
            if (timbreSteps > 0.0f) {
                c.timbrePhase = std::fmod(c.timbrePhase + timbreHz * dt, 1.0);
                const double triangle = 1.0 - 4.0 * std::abs(c.timbrePhase - 0.5);   // -1 .. +1, starts at -1
                modulatorSteps = juce::roundToInt(static_cast<double>(timbreSteps) * triangle);
            }
            if (tremoloSteps > 0.0f) {
                c.tremoloPhase = std::fmod(c.tremoloPhase + tremoloHz * dt, 1.0);
                const double dip = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * c.tremoloPhase);   // 0 .. 1, starts loud
                carrierSteps = juce::roundToInt(static_cast<double>(tremoloSteps) * dip);
            }
        }
        if (carrierSteps != c.carrierSteps || modulatorSteps != c.modulatorSteps) {
            c.carrierSteps = carrierSteps;
            c.modulatorSteps = modulatorSteps;
            ymfm.setChannelLevelMotion(static_cast<uint8_t>(ch), carrierSteps, modulatorSteps);
        }
    }
}

} // namespace ymulatorsynth
