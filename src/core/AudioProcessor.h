#pragma once

#include "AudioProcessingInterface.h"
#include "MidiProcessorInterface.h"
#include "ParameterManager.h"
#include "../dsp/YmfmWrapperInterface.h"
#include "../core/VoiceManagerInterface.h"
#include "../utils/Debug.h"
#include <memory>

namespace ymulatorsynth {

/**
 * @brief Concrete implementation of AudioProcessingInterface
 * 
 * This class handles the core audio processing operations,
 * extracted from PluginProcessor as part of Phase 3 refactoring.
 */
class AudioProcessor : public AudioProcessingInterface {
public:
    AudioProcessor(std::unique_ptr<YmfmWrapperInterface> ymfmWrapper,
                  std::unique_ptr<VoiceManagerInterface> voiceManager,
                  std::unique_ptr<MidiProcessorInterface> midiProcessor,
                  std::unique_ptr<ParameterManager> parameterManager);
    
    ~AudioProcessor() override = default;
    
    // AudioProcessingInterface implementation
    void processAudioBlock(juce::AudioBuffer<float>& buffer) override;
    void generateAudioSamples(juce::AudioBuffer<float>& buffer) override;
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    
    // Access to components for configuration
    YmfmWrapperInterface& getYmfmWrapper() { return *ymfmWrapper; }
    VoiceManagerInterface& getVoiceManager() { return *voiceManager; }
    MidiProcessorInterface& getMidiProcessor() { return *midiProcessor; }
    ParameterManager& getParameterManager() { return *parameterManager; }
    
private:
    std::unique_ptr<YmfmWrapperInterface> ymfmWrapper;
    std::unique_ptr<VoiceManagerInterface> voiceManager;
    std::unique_ptr<MidiProcessorInterface> midiProcessor;
    std::unique_ptr<ParameterManager> parameterManager;
    
    // State tracking
    bool isInitialized = false;
    uint32_t lastSampleRate = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioProcessor)
};

} // namespace ymulatorsynth