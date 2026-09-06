#include "MotionEngine.h"
#include "../utils/ParameterIDs.h"
#include <algorithm>
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
    pitchEnv2 = parameters.getParameter(ParamID::Motion::PitchEnv2);
    pitchTime2 = parameters.getParameter(ParamID::Motion::PitchTime2);
    echoLevel = parameters.getParameter(ParamID::Motion::EchoLevel);
    echoTime = parameters.getParameter(ParamID::Motion::EchoTime);
    echoDiv = parameters.getParameter(ParamID::Motion::EchoDiv);
    sweepAmount = parameters.getParameter(ParamID::Motion::SweepAmount);
    sweepTime = parameters.getParameter(ParamID::Motion::SweepTime);
    portaTime = parameters.getParameter(ParamID::Motion::PortaTime);
    velBright = parameters.getParameter(ParamID::Motion::VelBright);
    arpMode = parameters.getParameter(ParamID::Motion::ArpMode);
    arpDiv = parameters.getParameter(ParamID::Motion::ArpDiv);
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
    lastEchoEnabled = false;
    lastEchoSeconds = -1.0;
    lastEchoSteps = -1;
    lastNote = -1;
    lastVelBright = -1.0f;
}

double MotionEngine::beatsForDivision(int index)
{
    static constexpr double kBeats[kDivisions] = { 4.0, 2.0, 1.0, 0.5, 0.25, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 0.125, 0.0625, 1.0 / 6.0 };
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
    
    // Echo: level 100 repeats at the note's own loudness, lower levels attenuate the carriers
    const float echo = read(echoLevel, 0.0f);
    const bool echoOn = echo > 0.0f;
    const double echoSeconds = synced ? beatsForDivision(juce::roundToInt(read(echoDiv, 4.0f))) * 60.0 / bpm
                                      : read(echoTime, 120.0f) / 1000.0;
    const int echoSteps = juce::roundToInt((100.0f - echo) * 0.32f);
    if (echoOn != lastEchoEnabled || echoSeconds != lastEchoSeconds || echoSteps != lastEchoSteps) {
        lastEchoEnabled = echoOn;
        lastEchoSeconds = echoSeconds;
        lastEchoSteps = echoSteps;
        ymfm.setEcho(echoOn, echoSeconds, echoSteps);
    }
    
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
    // Two-stage pitch envelope: key-on at pitchStart, to pitchMid after pitchSettle, then to the note after pitchSettle2
    const float pitchStart = read(pitchEnv, 0.0f) / 100.0f;        // semitones at the key-on
    const double pitchSettle = read(pitchTime, 60.0f) / 1000.0;
    const float pitchMid = read(pitchEnv2, 0.0f) / 100.0f;
    const double pitchSettle2 = read(pitchTime2, 0.0f) / 1000.0;
    const bool pitchEnvelopeOn = pitchStart != 0.0f || pitchMid != 0.0f;
    const float sweepSteps = read(sweepAmount, 0.0f);                // modulator TL offset at the key-on
    const double sweepSeconds = read(sweepTime, 1500.0f) / 1000.0;
    const double portaSeconds = read(portaTime, 0.0f) / 1000.0;
    
    const float brightness = read(velBright, 0.0f) / 100.0f;
    if (brightness != lastVelBright) { lastVelBright = brightness; ymfm.setVelocityBrightness(brightness); }
    
    // Arpeggio: the held notes take turns on the one sounding channel, one per step
    const int arp = juce::roundToInt(read(arpMode, 0.0f));
    if (arp > 0 && heldNotes != nullptr && heldNotes->count >= 2 && heldNotes->channel >= 0) {
        std::array<uint8_t, 16> sorted {};
        const int count = heldNotes->count;
        for (int i = 0; i < count; ++i) sorted[static_cast<size_t>(i)] = heldNotes->notes[static_cast<size_t>(i)];
        std::sort(sorted.begin(), sorted.begin() + count);
        const double beats = beatsForDivision(juce::roundToInt(read(arpDiv, 9.0f)));
        const int step = static_cast<int>(std::floor(beat / beats));
        int index = 0;
        if (arp == 1) index = step % count;
        else if (arp == 2) index = count - 1 - (step % count);
        else { const int cycle = juce::jmax(1, 2 * count - 2); const int s = step % cycle; index = s < count ? s : cycle - s; }
        const uint8_t target = sorted[static_cast<size_t>(index)];
        const int ch = heldNotes->channel;
        if (voices.getNoteForChannel(ch) != target) {
            ymfm.retuneChannel(static_cast<uint8_t>(ch), target);
            voices.setNoteForChannel(ch, target);
            channels[static_cast<size_t>(ch)].note = target;   // an arpeggio step is not a glide
        }
    }
    
    for (int ch = 0; ch < 8; ++ch) {
        auto& c = channels[static_cast<size_t>(ch)];
        const bool active = voices.isVoiceActive(ch);
        const int note = active ? voices.getNoteForChannel(ch) : -1;
        const uint32_t noteOns = ymfm.getNoteOnCount(static_cast<uint8_t>(ch));
        const bool noteStarted = active && (!c.active || noteOns != c.noteOnCount);
        const bool retuned = active && !noteStarted && note != c.note && c.note >= 0;
        if (noteStarted) {
            c.time = 0.0;
            c.phase = 0.0;
            c.timbrePhase = 0.0;
            c.tremoloPhase = 0.0;
            c.pan = -1;   // the note-on wrote the global pan; pan motion must write again
            c.noteOnCount = noteOns;
            // Portamento from the last note played anywhere
            c.glideFrom = (portaSeconds > 0.0 && lastNote >= 0) ? static_cast<float>(lastNote - note) : 0.0f;
            c.glideTime = 0.0;
        } else if (retuned) {
            // Legato: keep whatever glide is left and add the interval to the new note
            c.glideFrom = portaSeconds > 0.0 ? c.glideOffset + static_cast<float>(c.note - note) : 0.0f;
            c.glideTime = 0.0;
        }
        if (noteStarted || retuned) lastNote = note;
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
        if (active && (depthSemitones > 0.0f || pitchEnvelopeOn || sweepSteps != 0.0f)) c.time += dt;
        c.glideOffset = 0.0f;
        if (active && c.glideFrom != 0.0f && portaSeconds > 0.0) {
            c.glideTime += dt;
            const double remaining = std::max(0.0, 1.0 - c.glideTime / portaSeconds);
            c.glideOffset = static_cast<float>(c.glideFrom * remaining);
            offset += c.glideOffset;
        }
        if (active && pitchEnvelopeOn) {
            if (c.time < pitchSettle) {
                const double t = pitchSettle <= 0.0 ? 1.0 : c.time / pitchSettle;
                offset += static_cast<float>(pitchStart + (pitchMid - pitchStart) * t);
            } else if (c.time < pitchSettle + pitchSettle2) {
                const double t = (c.time - pitchSettle) / pitchSettle2;
                offset += static_cast<float>(pitchMid * (1.0 - t));
            }
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
        
        // Timbre LFO: triangle on the modulators (both directions); tremolo: carriers only attenuate.
        // Sweep: the modulators start offset and settle on the patch, the FM stand-in for a filter envelope.
        int carrierSteps = 0, modulatorSteps = 0;
        if (active) {
            if (sweepSteps != 0.0f) {
                const double remaining = sweepSeconds <= 0.0 ? 0.0 : std::max(0.0, 1.0 - c.time / sweepSeconds);
                modulatorSteps += juce::roundToInt(static_cast<double>(sweepSteps) * remaining * remaining);   // eases in, like a filter
            }
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
