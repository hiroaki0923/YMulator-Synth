#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include "../dsp/YmfmWrapperInterface.h"
#include "VoiceManagerInterface.h"

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
    
    static constexpr int kDivisions = 8;
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
    bool lastWideEnabled = false;
    float lastWideCents = -1.0f;
    int lastWidePan = -1;
};

} // namespace ymulatorsynth
