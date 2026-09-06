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
    panMode = parameters.getParameter(ParamID::Motion::PanMode);
    panRate = parameters.getParameter(ParamID::Motion::PanRate);
    sync = parameters.getParameter(ParamID::Motion::Sync);
    vibratoDiv = parameters.getParameter(ParamID::Motion::VibratoDiv);
    timbreDiv = parameters.getParameter(ParamID::Motion::TimbreDiv);
    tremoloDiv = parameters.getParameter(ParamID::Motion::TremoloDiv);
    pitchEnv = parameters.getParameter(ParamID::Motion::PitchEnv);
    pitchTime = parameters.getParameter(ParamID::Motion::PitchTime);
}

void MotionEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    for (auto& c : channels) c = Channel {};
    beat = 0.0;
    samplesIntoBlock = 0.0;
    lastPanMode = 0;
    nextAlternateRight = false;
    lastWideEnabled = false;
    lastWideCents = -1.0f;
    lastWidePan = -1;
}

double MotionEngine::beatsForDivision(int index)
{
    static constexpr double kBeats[kDivisions] = { 4.0, 2.0, 1.0, 0.5, 0.25, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0 };
    return kBeats[juce::jlimit(0, kDivisions - 1, index)];
}

void MotionEngine::setTransport(double hostBpm, double ppq, bool playing, bool known)
{
    transportKnown = known;
    transportPlaying = known && playing;
    if (known && hostBpm > 0.0) bpm = hostBpm;
    ppqAtBlockStart = ppq;
    samplesIntoBlock = 0.0;
}

void MotionEngine::writePan(int channel, int pan)
{
    auto& c = channels[static_cast<size_t>(channel)];
    if (c.pan == pan) return;
    c.pan = pan;
    ymfm.setChannelPan(static_cast<uint8_t>(channel), pan == 0 ? 0.0f : (pan == 2 ? 1.0f : 0.5f));
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
    const int widePanMode = juce::roundToInt(read(widePan, 0.0f));
    if (wideEnabled != lastWideEnabled || wideCents != lastWideCents || widePanMode != lastWidePan) {
        lastWideEnabled = wideEnabled;
        lastWideCents = wideCents;
        lastWidePan = widePanMode;
        ymfm.setWide(wideEnabled, wideCents, widePanMode == 0 ? YmfmWrapperInterface::WidePan::LeftRight : YmfmWrapperInterface::WidePan::Centre);
    }
    
    const double dt = static_cast<double>(numSamples) / sampleRate;
    
    // Beat clock: exact while the host plays, free-running at the last tempo otherwise
    if (transportPlaying) beat = ppqAtBlockStart + samplesIntoBlock / (sampleRate * 60.0 / bpm);
    else beat += dt * bpm / 60.0;
    samplesIntoBlock += numSamples;
    
    const bool synced = read(sync, 0.0f) > 0.5f;
    auto syncedPhase = [&](const juce::RangedAudioParameter* div) {
        const double beats = beatsForDivision(juce::roundToInt(read(div, 0.0f)));
        return std::fmod(beat / beats, 1.0);
    };
    
    const int mode = juce::roundToInt(read(panMode, 0.0f));
    const bool wideBlocksPan = lastWideEnabled && lastWidePan == 0;
    const bool panActive = mode != 0 && !wideBlocksPan;
    if (!panActive && lastPanMode != 0) {
        for (auto& c : channels) c.pan = -1;
        if (onPanMotionOff) onPanMotionOff();
    }
    lastPanMode = panActive ? mode : 0;
    const int stepPan = [&]() {
        static constexpr int kPattern[4] = { 0, 1, 2, 1 };   // L, C, R, C
        const double beats = beatsForDivision(juce::roundToInt(read(panRate, 2.0f)));
        return kPattern[static_cast<int>(std::floor(beat / beats)) & 3];
    }();
    
    const float depthSemitones = read(vibratoDepth, 0.0f) / 100.0f * kMaxVibratoCents / 100.0f;
    const double rateHz = read(vibratoRate, 5.0f);
    const double delay = read(vibratoDelay, 0.0f) / 1000.0;
    const double rise = read(vibratoRise, 0.0f) / 1000.0;
    const float timbreSteps = read(timbreDepth, 0.0f);
    const double timbreHz = read(timbreRate, 1.0f);
    const float tremoloSteps = read(tremoloDepth, 0.0f);
    const double tremoloHz = read(tremoloRate, 5.0f);
    const float pitchStart = read(pitchEnv, 0.0f) / 100.0f;        // semitones at the key-on
    const double pitchSettle = read(pitchTime, 60.0f) / 1000.0;
    
    for (int ch = 0; ch < 8; ++ch) {
        auto& c = channels[static_cast<size_t>(ch)];
        const bool active = voices.isVoiceActive(ch);
        const int note = active ? voices.getNoteForChannel(ch) : -1;
        const bool noteStarted = active && (!c.active || note != c.note);
        if (noteStarted) {
            c.time = 0.0;
            c.phase = 0.0;
            c.timbrePhase = 0.0;
            c.tremoloPhase = 0.0;
            c.pan = -1;   // the note-on wrote the global pan; pan motion must write again
        }
        c.active = active;
        c.note = note;
        
        if (panActive) {
            if (mode == 1) {
                if (noteStarted) {
                    writePan(ch, nextAlternateRight ? 2 : 0);
                    nextAlternateRight = !nextAlternateRight;
                }
            } else {
                writePan(ch, stepPan);
            }
        }
        
        float offset = 0.0f;
        if (active && (depthSemitones > 0.0f || pitchStart != 0.0f)) c.time += dt;
        if (active && pitchStart != 0.0f) {
            // Slides from the start offset onto the note, linearly over the settle time
            const double remaining = pitchSettle <= 0.0 ? 0.0 : std::max(0.0, 1.0 - c.time / pitchSettle);
            offset += static_cast<float>(pitchStart * remaining);
        }
        if (active && depthSemitones > 0.0f) {
            c.phase = synced ? syncedPhase(vibratoDiv) : std::fmod(c.phase + rateHz * dt, 1.0);
            const double sinceDelay = c.time - delay;
            const double envelope = sinceDelay <= 0.0 ? 0.0 : (rise <= 0.0 ? 1.0 : std::min(1.0, sinceDelay / rise));
            offset += static_cast<float>(depthSemitones * envelope * std::sin(juce::MathConstants<double>::twoPi * c.phase));
        }
        if (offset != c.offset) {
            c.offset = offset;
            ymfm.setChannelPitchOffset(static_cast<uint8_t>(ch), offset);
        }
        
        // Timbre LFO: triangle on the modulators (both directions); tremolo: carriers only attenuate
        int carrierSteps = 0, modulatorSteps = 0;
        if (active) {
            if (timbreSteps > 0.0f) {
                c.timbrePhase = synced ? syncedPhase(timbreDiv) : std::fmod(c.timbrePhase + timbreHz * dt, 1.0);
                const double triangle = 1.0 - 4.0 * std::abs(c.timbrePhase - 0.5);   // -1 .. +1, starts at -1
                modulatorSteps = juce::roundToInt(static_cast<double>(timbreSteps) * triangle);
            }
            if (tremoloSteps > 0.0f) {
                c.tremoloPhase = synced ? syncedPhase(tremoloDiv) : std::fmod(c.tremoloPhase + tremoloHz * dt, 1.0);
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
