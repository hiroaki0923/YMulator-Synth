#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
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
    void tick(int numSamples);
    
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
    bool lastWideEnabled = false;
    float lastWideCents = -1.0f;
    int lastWidePan = -1;
};

} // namespace ymulatorsynth
