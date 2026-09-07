#include "MotionEngine.h"
#include "../utils/ParameterIDs.h"
#include <algorithm>
#include <cmath>
#include <vector>

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
    arpOctaves = parameters.getParameter(ParamID::Motion::ArpOctaves);
    arpRetrigger = parameters.getParameter(ParamID::Motion::ArpRetrigger);
    arpGate = parameters.getParameter(ParamID::Motion::ArpGate);
    arpLatch = parameters.getParameter(ParamID::Motion::ArpLatch);
    arpChord = parameters.getParameter(ParamID::Motion::ArpChord);
    arpAccent = parameters.getParameter(ParamID::Motion::ArpAccent);
    arpAccentDepth = parameters.getParameter(ParamID::Motion::ArpAccentDepth);
    levelAttack = parameters.getParameter(ParamID::Motion::LevelAttack);
    levelDecay = parameters.getParameter(ParamID::Motion::LevelDecay);
    levelSustain = parameters.getParameter(ParamID::Motion::LevelSustain);
    vibratoWave = parameters.getParameter(ParamID::Motion::VibratoWave);
    timbreWave = parameters.getParameter(ParamID::Motion::TimbreWave);
    lfoOneShot = parameters.getParameter(ParamID::Motion::LfoOneShot);
}

void MotionEngine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    for (auto& c : channels) c = Channel {};
    beat = 0.0;
    samplesIntoBlock = 0.0;
    nextAlternateRight = false;
    lastRandomPan = 1;
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

int MotionEngine::nextRandomPan()
{
    panRandomState ^= panRandomState << 13;
    panRandomState ^= panRandomState >> 17;
    panRandomState ^= panRandomState << 5;
    int pan = static_cast<int>(panRandomState % 2u);          // one of the two positions that differ from the last
    if (pan >= lastRandomPan) ++pan;
    lastRandomPan = pan;
    return pan;
}

void MotionEngine::writePan(int channel, int pan)
{
    auto& c = channels[static_cast<size_t>(channel)];
    if (c.pan == pan) return;
    c.pan = pan;
    ymfm.setChannelPan(static_cast<uint8_t>(channel), pan == 0 ? 0.0f : (pan == 2 ? 1.0f : 0.5f));
}

double MotionEngine::advancePhase(double phase, double increment, bool oneShot)
{
    const double next = phase + increment;
    if (oneShot) return std::min(next, 1.0);
    return std::fmod(next, 1.0);
}

float MotionEngine::waveform(int wave, double phase, int cycleId, int& cycle, float& held, uint32_t& seed)
{
    switch (wave) {
        case 1: return static_cast<float>(1.0 - 4.0 * std::abs(phase - 0.5));              // triangle, starts low
        case 2: return static_cast<float>(2.0 * phase - 1.0);                              // rising saw
        case 3: return phase < 0.5 ? 1.0f : -1.0f;                                         // square
        case 4: {                                                                          // random, held per cycle
            if (cycleId != cycle || cycle < 0) {
                cycle = cycleId;
                seed = seed * 1664525u + 1013904223u;
                held = static_cast<float>((seed >> 8) & 0xFFFF) / 32767.5f - 1.0f;
            }
            return held;
        }
        default: return static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * phase));
    }
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
    
    // Pan: Off and Left / Right / Random place every voice, Alternate and Step move it.
    // With Wide split left / right the chips own the pan bits and nothing is written.
    const int mode = juce::roundToInt(read(panMode, 0.0f));
    const bool wideBlocksPan = lastWideEnabled && lastWidePan == 0;
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
    const double attackSeconds = read(levelAttack, 0.0f) / 1000.0;
    const double decaySeconds = read(levelDecay, 0.0f) / 1000.0;
    const float sustainSteps = read(levelSustain, 0.0f);
    const bool levelEnvelopeOn = attackSeconds > 0.0 || sustainSteps > 0.0f;
    const int vibWave = juce::roundToInt(read(vibratoWave, 0.0f));
    const int timWave = juce::roundToInt(read(timbreWave, 1.0f));
    const bool oneShot = read(lfoOneShot, 0.0f) > 0.5f;
    const double portaSeconds = read(portaTime, 0.0f) / 1000.0;
    
    const float brightness = read(velBright, 0.0f) / 100.0f;
    if (brightness != lastVelBright) { lastVelBright = brightness; ymfm.setVelocityBrightness(brightness); }
    
    runArpeggio(beat);
    
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
            c.pan = -1;   // a fresh voice gets its pan written again
            c.noteOnCount = noteOns;
            // Portamento from the last note played anywhere
            c.glideFrom = (portaSeconds > 0.0 && lastNote >= 0) ? static_cast<float>(lastNote - note) : 0.0f;
            c.glideTime = 0.0;
            c.randomCycle = -1;
            c.timbreRandomCycle = -1;
            c.vibCycles = 0;
            c.timbreCycles = 0;
            c.randomSeed = 0x9E3779B9u * static_cast<uint32_t>(ch + 1) + static_cast<uint32_t>(noteOns);
        } else if (retuned) {
            // Legato: keep whatever glide is left and add the interval to the new note
            c.glideFrom = portaSeconds > 0.0 ? c.glideOffset + static_cast<float>(c.note - note) : 0.0f;
            c.glideTime = 0.0;
        }
        if (noteStarted || retuned) lastNote = note;
        c.active = active;
        c.note = note;
        
        if (wideBlocksPan) {
            c.pan = -1;
        } else {
            int pan = 1;
            switch (mode) {
                case 1:   // Alternate: each new note takes the other side
                    if (noteStarted) {
                        pan = nextAlternateRight ? 2 : 0;
                        nextAlternateRight = !nextAlternateRight;
                    } else pan = c.pan < 0 ? 1 : c.pan;
                    break;
                case 2: pan = stepPan; break;
                case 3: pan = 0; break;
                case 4: pan = 2; break;
                case 5:   // Random: each new note lands somewhere else
                    pan = noteStarted ? nextRandomPan() : (c.pan < 0 ? 1 : c.pan);
                    break;
                default: break;
            }
            writePan(ch, pan);
        }
        
        float offset = 0.0f;
        if (active && (depthSemitones > 0.0f || pitchEnvelopeOn || sweepSteps != 0.0f || levelEnvelopeOn)) c.time += dt;
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
            const double before = c.phase;
            c.phase = synced ? syncedPhase(vibratoDiv) : advancePhase(c.phase, rateHz * dt, oneShot);
            if (c.phase < before) ++c.vibCycles;
            const double sinceDelay = c.time - delay;
            const double envelope = sinceDelay <= 0.0 ? 0.0 : (rise <= 0.0 ? 1.0 : std::min(1.0, sinceDelay / rise));
            const int vibCycleId = synced ? static_cast<int>(std::floor(beat / beatsForDivision(juce::roundToInt(read(vibratoDiv, 0.0f))))) : c.vibCycles;
            offset += static_cast<float>(depthSemitones * envelope) * waveform(vibWave, c.phase, vibCycleId, c.randomCycle, c.randomValue, c.randomSeed);
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
                const double before = c.timbrePhase;
                c.timbrePhase = synced ? syncedPhase(timbreDiv) : advancePhase(c.timbrePhase, timbreHz * dt, oneShot);
                if (c.timbrePhase < before) ++c.timbreCycles;
                const int timbreCycleId = synced ? static_cast<int>(std::floor(beat / beatsForDivision(juce::roundToInt(read(timbreDiv, 0.0f))))) : c.timbreCycles;
                modulatorSteps = juce::roundToInt(timbreSteps * waveform(timWave, c.timbrePhase, timbreCycleId, c.timbreRandomCycle, c.timbreRandomValue, c.randomSeed));
            }
            // Level EG on the carriers: swell in over the attack, then fall to the sustain attenuation
            if (levelEnvelopeOn) {
                if (c.time < attackSeconds) carrierSteps += juce::roundToInt(40.0 * (1.0 - c.time / attackSeconds));
                else if (decaySeconds > 0.0 && c.time < attackSeconds + decaySeconds)
                    carrierSteps += juce::roundToInt(static_cast<double>(sustainSteps) * ((c.time - attackSeconds) / decaySeconds));
                else carrierSteps += juce::roundToInt(sustainSteps);
            }
            if (tremoloSteps > 0.0f) {
                c.tremoloPhase = synced ? syncedPhase(tremoloDiv) : std::fmod(c.tremoloPhase + tremoloHz * dt, 1.0);
                const double dip = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi * c.tremoloPhase);   // 0 .. 1, starts loud
                carrierSteps = juce::roundToInt(static_cast<double>(tremoloSteps) * dip);
            }
            if (ch == arpChannel) carrierSteps += arpAccentSteps;   // unaccented arpeggio steps sit a little back
        }
        if (carrierSteps != c.carrierSteps || modulatorSteps != c.modulatorSteps) {
            c.carrierSteps = carrierSteps;
            c.modulatorSteps = modulatorSteps;
            ymfm.setChannelLevelMotion(static_cast<uint8_t>(ch), carrierSteps, modulatorSteps);
        }
    }
}

namespace {
// Chord tables applied to a single held note, as trackers do: semitones above the root
const std::vector<std::vector<int>> kChordTables = {
    {},                  // None
    { 0, 4, 7 },         // Major
    { 0, 3, 7 },         // Minor
    { 0, 4, 7, 10 },     // 7th
    { 0, 3, 7, 10 },     // m7
    { 0, 4, 7, 11 },     // Maj7
    { 0, 5, 7 },         // Sus4
    { 0, 2, 7 },         // Sus2
    { 0, 3, 6 },         // Dim
    { 0, 4, 8 },         // Aug
    { 0, 7 },            // 5th
    { 0, 12 },           // Octave
};
}

void MotionEngine::runArpeggio(double beat)
{
    // Latch switched off: hand the held chord back to the MIDI processor to release
    const bool latch = read(arpLatch, 0.0f) > 0.5f;
    if (lastArpLatch && !latch && onLatchOff) onLatchOff();
    lastArpLatch = latch;
    
    const int arp = juce::roundToInt(read(arpMode, 0.0f));
    const bool running = arp > 0 && heldNotes != nullptr && heldNotes->count >= 1 && heldNotes->channel >= 0;
    if (!running) {
        arpLastStep = -1;
        arpGateClosed = false;
        if (arpAccentSteps != 0 || arpChannel >= 0) { arpAccentSteps = 0; arpChannel = -1; }
        return;
    }
    
    // The notes to cycle: a chord table on a single note, otherwise the held notes, then more octaves
    std::vector<uint8_t> notes;
    const int chord = juce::roundToInt(read(arpChord, 0.0f));
    if (heldNotes->count == 1 && chord > 0 && chord < static_cast<int>(kChordTables.size())) {
        for (int semis : kChordTables[static_cast<size_t>(chord)]) {
            const int n = heldNotes->notes[0] + semis;
            if (n <= 127) notes.push_back(static_cast<uint8_t>(n));
        }
    } else {
        for (int i = 0; i < heldNotes->count; ++i) notes.push_back(heldNotes->notes[static_cast<size_t>(i)]);
    }
    const int octaves = juce::jlimit(1, 4, juce::roundToInt(read(arpOctaves, 1.0f)));
    const size_t baseCount = notes.size();
    for (int o = 1; o < octaves; ++o)
        for (size_t i = 0; i < baseCount; ++i)
            if (notes[i] + 12 * o <= 127) notes.push_back(static_cast<uint8_t>(notes[i] + 12 * o));
    if (arp != 5) std::sort(notes.begin(), notes.end());   // "As Played" keeps the order the keys went down
    const int count = static_cast<int>(notes.size());
    if (count < 2) return;
    
    const double beats = beatsForDivision(juce::roundToInt(read(arpDiv, 9.0f)));
    const double stepPosition = beat / beats;
    const int step = static_cast<int>(std::floor(stepPosition));
    // A new chord (or a transport jump backwards) restarts the pattern from its first note. Hosts hand over
    // the notes of a bar in the block that contains the bar line, a few milliseconds early, so a chord that
    // arrives in the second half of a step belongs to the step about to start.
    const bool restarted = heldNotes->version != arpHeldVersion || step + 1 < arpOrigin;
    if (restarted) {
        arpHeldVersion = heldNotes->version;
        arpOrigin = (stepPosition - static_cast<double>(step) >= 0.5) ? step + 1 : step;
        arpLastStep = -1;
        arpGateClosed = false;
    }
    const int rel = juce::jmax(0, step - arpOrigin);
    
    int index = 0;
    if (arp == 1 || arp == 5) index = rel % count;
    else if (arp == 2) index = count - 1 - (rel % count);
    else if (arp == 3) { const int cycle = juce::jmax(1, 2 * count - 2); const int s = rel % cycle; index = s < count ? s : cycle - s; }
    else {
        if (step != arpLastStep) {
            // xorshift, never the same note twice in a row
            do {
                arpRandomState ^= arpRandomState << 13; arpRandomState ^= arpRandomState >> 17; arpRandomState ^= arpRandomState << 5;
                index = static_cast<int>(arpRandomState % static_cast<uint32_t>(count));
            } while (index == arpRandomIndex && count > 1);
            arpRandomIndex = index;
        } else index = arpRandomIndex;
    }
    const uint8_t target = notes[static_cast<size_t>(index)];
    const int ch = heldNotes->channel;
    arpChannel = ch;
    
    const bool retrigger = read(arpRetrigger, 0.0f) > 0.5f;
    const bool newStep = step != arpLastStep;
    if (newStep) {
        arpLastStep = step;
        arpGateClosed = false;
        const uint8_t current = voices.getNoteForChannel(ch);
        // The first step of a new chord was keyed by the note-on itself; later steps are keyed again when retriggering
        if (retrigger && !restarted && step >= arpOrigin) {
            ymfm.noteOff(static_cast<uint8_t>(ch), current);
            ymfm.noteOn(static_cast<uint8_t>(ch), target, voices.getVelocityForChannel(ch));
            voices.setNoteForChannel(ch, target);
            channels[static_cast<size_t>(ch)].note = target;
        } else if (current != target) {
            ymfm.retuneChannel(static_cast<uint8_t>(ch), target);
            voices.setNoteForChannel(ch, target);
            channels[static_cast<size_t>(ch)].note = target;   // an arpeggio step is not a glide
        }
    }
    // Gate: a retriggered step lets go part way through, so the next step has an attack of its own
    if (retrigger && !arpGateClosed) {
        const double gate = read(arpGate, 70.0f) / 100.0;
        if (gate < 1.0 && stepPosition - static_cast<double>(step) >= gate) {
            ymfm.noteOff(static_cast<uint8_t>(ch), voices.getNoteForChannel(ch));
            arpGateClosed = true;
        }
    }
    // Accent: steps on the beat (or every n-th step) keep their level, the others sit back by the depth
    const int accent = juce::roundToInt(read(arpAccent, 0.0f));
    bool accented = true;
    if (accent == 1) {
        const int stepsPerBeat = juce::jmax(1, juce::roundToInt(1.0 / beats));
        accented = (step % stepsPerBeat) == 0;
    } else if (accent >= 2) {
        accented = (rel % accent) == 0;
    }
    arpAccentSteps = accented ? 0 : juce::roundToInt(read(arpAccentDepth, 6.0f));
}

} // namespace ymulatorsynth
