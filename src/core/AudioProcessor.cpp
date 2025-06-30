#include "AudioProcessor.h"
#include "../dsp/YM2151Registers.h"
#include <cmath>

using namespace ymulatorsynth;

AudioProcessor::AudioProcessor(std::unique_ptr<YmfmWrapperInterface> ymfmWrapper,
                              std::unique_ptr<VoiceManagerInterface> voiceManager,
                              std::unique_ptr<MidiProcessorInterface> midiProcessor,
                              std::unique_ptr<ParameterManager> parameterManager)
    : ymfmWrapper(std::move(ymfmWrapper))
    , voiceManager(std::move(voiceManager))
    , midiProcessor(std::move(midiProcessor))
    , parameterManager(std::move(parameterManager))
{
    CS_DBG("AudioProcessor created");
}

void AudioProcessor::processAudioBlock(juce::AudioBuffer<float>& buffer)
{
    // Assert buffer validity
    CS_ASSERT_BUFFER_SIZE(buffer.getNumSamples());
    CS_ASSERT(buffer.getNumChannels() >= 1 && buffer.getNumChannels() <= 2);
    
    juce::ScopedNoDenormals noDenormals;
    
    // Clear output buffer
    buffer.clear();
    
    // Update parameters periodically (rate limiting handled by ParameterManager)
    parameterManager->updateYmfmParameters();
    
    // Generate audio samples
    generateAudioSamples(buffer);
}

void AudioProcessor::generateAudioSamples(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    
    if (numSamples > 0) {
        // Generate true stereo output
        float* leftBuffer = buffer.getWritePointer(0);
        float* rightBuffer = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : leftBuffer;
        
        ymfmWrapper->generateSamples(leftBuffer, rightBuffer, numSamples);
        
        // Apply moderate gain to prevent clipping
        buffer.applyGain(0, 0, numSamples, 2.0f);
        if (buffer.getNumChannels() > 1) {
            buffer.applyGain(1, 0, numSamples, 2.0f);
        }
    }
}

void AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Assert valid sample rate and buffer size
    CS_ASSERT_SAMPLE_RATE(sampleRate);
    CS_ASSERT_BUFFER_SIZE(samplesPerBlock);
    
    juce::ignoreUnused(samplesPerBlock);
    
    // Initialize ymfm wrapper with OPM for now (only if needed)
    uint32_t currentSampleRate = static_cast<uint32_t>(sampleRate);
    if (!isInitialized || lastSampleRate != currentSampleRate) {
        ymfmWrapper->initialize(YmfmWrapperInterface::ChipType::OPM, currentSampleRate);
        isInitialized = true;
        lastSampleRate = currentSampleRate;
        
        // Apply initial parameters only when truly initializing
        parameterManager->updateYmfmParameters();
    }
    
    CS_DBG("AudioProcessor prepared for playback");
}

void AudioProcessor::releaseResources()
{
    // Clear all voices to prevent audio after stop
    voiceManager->releaseAllVoices();
    
    // Reset ymfm to clear any lingering audio
    ymfmWrapper->reset();
    
    // Reset state
    isInitialized = false;
    lastSampleRate = 0;
    
    CS_DBG("AudioProcessor resources released");
}