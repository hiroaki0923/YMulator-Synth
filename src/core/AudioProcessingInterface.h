#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace ymulatorsynth {

/**
 * @brief Interface for audio processing operations
 * 
 * This interface provides an abstraction layer for audio processing,
 * allowing different implementations and improved testability.
 * Part of Phase 3 Enhanced Abstraction refactoring.
 */
class AudioProcessingInterface {
public:
    virtual ~AudioProcessingInterface() = default;
    
    /**
     * @brief Process a complete audio block with MIDI
     * @param buffer Audio buffer to process (stereo or mono)
     */
    virtual void processAudioBlock(juce::AudioBuffer<float>& buffer) = 0;
    
    /**
     * @brief Generate audio samples without MIDI processing
     * @param buffer Audio buffer to fill with generated samples
     */
    virtual void generateAudioSamples(juce::AudioBuffer<float>& buffer) = 0;
    
    /**
     * @brief Prepare the audio processor for playback
     * @param sampleRate The sample rate to prepare for
     * @param samplesPerBlock Maximum number of samples per block
     */
    virtual void prepareToPlay(double sampleRate, int samplesPerBlock) = 0;
    
    /**
     * @brief Release audio processing resources
     */
    virtual void releaseResources() = 0;
};

} // namespace ymulatorsynth