#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include "../dsp/YmfmWrapperInterface.h"
#include "VoiceManagerInterface.h"
#include "HeldNotes.h"

namespace ymulatorsynth {

/**
 * Time-variant expression written straight to the chip at control rate:
 * per-voice delayed vibrato now, wide / timbre LFO / pan motion later.
 * Runs on the audio thread between chunks of kChunk samples; reads its
 * parameters through cached handles and never touches the parameter tree.
 * Design: docs/ymulatorsynth-motion-design.md
 */
class MotionEngine
{
public:
    static constexpr int kChunk = 64;
    static constexpr float kMaxVibratoCents = 50.0f;   // depth 100 = +/- 50 cents
    static constexpr float kMaxWideCents = 25.0f;      // wide 100 = +/- 25 cents between the chips
    
    MotionEngine(YmfmWrapperInterface& ymfm, VoiceManagerInterface& voices);
    
    void bindParameters(juce::AudioProcessorValueTreeState& parameters);
    void prepare(double sampleRate);
    /** Host transport for the coming block; call once per processBlock before the ticks. */
    void setTransport(double bpm, double ppqAtBlockStart, bool playing, bool known);
    void tick(int numSamples);
    
    /** Called when pan motion switches off so the global pan can be put back. */
    std::function<void()> onPanMotionOff;
    /** Called when the arpeggio latch parameter goes off, so a latched chord can be let go. */
    std::function<void()> onLatchOff;
    /** Where the MIDI processor keeps the notes held in mono / arpeggio mode. */
    void setHeldNotes(const HeldNotes* notes) { heldNotes = notes; }
    
    static constexpr int kDivisions = 11;
    static double beatsForDivision(int index);
    double currentBeat() const { return beat; }
    
    /** Current pitch offset of a channel in semitones (tests and displays). */
    float currentOffset(int channel) const { return channels[static_cast<size_t>(channel)].offset; }
    int currentCarrierSteps(int channel) const { return channels[static_cast<size_t>(channel)].carrierSteps; }
    int currentModulatorSteps(int channel) const { return channels[static_cast<size_t>(channel)].modulatorSteps; }
    
private:
    struct Channel {
        bool active = false;
        int note = -1;
        double time = 0.0;      // seconds since the note started
        double phase = 0.0;     // vibrato cycles
        double timbrePhase = 0.0;
        double tremoloPhase = 0.0;
        float offset = 0.0f;    // semitones written to the chip
        int carrierSteps = 0;   // tremolo written to the chip
        int modulatorSteps = 0; // timbre LFO written to the chip
        int pan = -1;           // 0 left, 1 centre, 2 right as written by pan motion; -1 untouched
        uint32_t noteOnCount = 0;
        float glideFrom = 0.0f; // portamento start offset, semitones
        double glideTime = 0.0; // seconds since the glide started
        float glideOffset = 0.0f;
        uint32_t randomSeed = 1;   // per-channel sample-and-hold source
        int randomCycle = -1;
        int vibCycles = 0;      // completed vibrato cycles, the random wave's clock
        int timbreCycles = 0;
        float randomValue = 0.0f;
        int timbreRandomCycle = -1;
        float timbreRandomValue = 0.0f;
    };
    
    float read(const juce::RangedAudioParameter* param, float fallback) const;
    
    YmfmWrapperInterface& ymfm;
    VoiceManagerInterface& voices;
    std::array<Channel, 8> channels;
    double sampleRate = 48000.0;
    const juce::RangedAudioParameter* vibratoDepth = nullptr;
    const juce::RangedAudioParameter* vibratoRate = nullptr;
    const juce::RangedAudioParameter* vibratoDelay = nullptr;
    const juce::RangedAudioParameter* vibratoRise = nullptr;
    const juce::RangedAudioParameter* wide = nullptr;
    const juce::RangedAudioParameter* widePan = nullptr;
    const juce::RangedAudioParameter* timbreDepth = nullptr;
    const juce::RangedAudioParameter* timbreRate = nullptr;
    const juce::RangedAudioParameter* tremoloDepth = nullptr;
    const juce::RangedAudioParameter* tremoloRate = nullptr;
    const juce::RangedAudioParameter* panMode = nullptr;
    const juce::RangedAudioParameter* panRate = nullptr;
    const juce::RangedAudioParameter* sync = nullptr;
    const juce::RangedAudioParameter* vibratoDiv = nullptr;
    const juce::RangedAudioParameter* timbreDiv = nullptr;
    const juce::RangedAudioParameter* tremoloDiv = nullptr;
    const juce::RangedAudioParameter* pitchEnv = nullptr;
    const juce::RangedAudioParameter* pitchTime = nullptr;
    const juce::RangedAudioParameter* pitchEnv2 = nullptr;
    const juce::RangedAudioParameter* pitchTime2 = nullptr;
    const juce::RangedAudioParameter* echoLevel = nullptr;
    const juce::RangedAudioParameter* echoTime = nullptr;
    const juce::RangedAudioParameter* echoDiv = nullptr;
    const juce::RangedAudioParameter* sweepAmount = nullptr;
    const juce::RangedAudioParameter* sweepTime = nullptr;
    const juce::RangedAudioParameter* portaTime = nullptr;
    const juce::RangedAudioParameter* velBright = nullptr;
    const juce::RangedAudioParameter* arpMode = nullptr;
    const juce::RangedAudioParameter* arpDiv = nullptr;
    const juce::RangedAudioParameter* arpOctaves = nullptr;
    const juce::RangedAudioParameter* arpRetrigger = nullptr;
    const juce::RangedAudioParameter* arpGate = nullptr;
    const juce::RangedAudioParameter* arpLatch = nullptr;
    const juce::RangedAudioParameter* arpChord = nullptr;
    const juce::RangedAudioParameter* arpAccent = nullptr;
    const juce::RangedAudioParameter* arpAccentDepth = nullptr;
    // Arpeggio state: the step the current chord started on, the last step played, the gate, the accent
    uint32_t arpHeldVersion = 0;
    int arpOrigin = 0;
    int arpLastStep = -1;
    bool arpGateClosed = false;
    int arpChannel = -1;
    int arpAccentSteps = 0;      // TL steps taken off the sounding channel on an unaccented step
    int arpRandomIndex = -1;
    uint32_t arpRandomState = 0x2545F491u;
    bool lastArpLatch = false;
    void runArpeggio(double beat);
    const juce::RangedAudioParameter* levelAttack = nullptr;
    const juce::RangedAudioParameter* levelDecay = nullptr;
    const juce::RangedAudioParameter* levelSustain = nullptr;
    const juce::RangedAudioParameter* vibratoWave = nullptr;
    const juce::RangedAudioParameter* timbreWave = nullptr;
    const juce::RangedAudioParameter* lfoOneShot = nullptr;
    const HeldNotes* heldNotes = nullptr;
    int lastNote = -1;
    float lastVelBright = -1.0f;
    bool lastEchoEnabled = false;
    double lastEchoSeconds = -1.0;
    int lastEchoSteps = -1;
    
    // Beat clock: follows the host while it plays, free-runs otherwise
    double bpm = 120.0;
    double beat = 0.0;
    double ppqAtBlockStart = 0.0;
    double samplesIntoBlock = 0.0;
    bool transportPlaying = false;
    bool transportKnown = false;
    int lastPanMode = 0;
    bool nextAlternateRight = false;
    
    void writePan(int channel, int pan);
    /** -1..+1 for the wave at the phase; random holds one value per cycle. */
    static float waveform(int wave, double phase, int cycleId, int& cycle, float& held, uint32_t& seed);
    /** Advances a phase; one-shot phases stop at the end of the first cycle. */
    static double advancePhase(double phase, double increment, bool oneShot);
    bool lastWideEnabled = false;
    float lastWideCents = -1.0f;
    int lastWidePan = -1;
};

} // namespace ymulatorsynth
