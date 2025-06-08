#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/YmfmWrapper.h"
#include "core/VoiceManager.h"
#include "utils/PresetManager.h"
#include <unordered_map>

class ChipSynthAudioProcessor : public juce::AudioProcessor
{
public:
    ChipSynthAudioProcessor();
    ~ChipSynthAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter access for UI
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

private:
    YmfmWrapper ymfmWrapper;
    VoiceManager voiceManager;
    
    // Parameter system
    juce::AudioProcessorValueTreeState parameters;
    std::unordered_map<int, juce::AudioParameterInt*> ccToParameterMap;
    std::atomic<int> parameterUpdateCounter{0};
    static constexpr int PARAMETER_UPDATE_RATE_DIVIDER = 8;
    
    // Preset management
    chipsynth::PresetManager presetManager;
    int currentPreset = 0;
    
    // Methods
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setupCCMapping();
    void handleMidiCC(int ccNumber, int value);
    void updateYmfmParameters();
    void loadPreset(int index);
    void loadPreset(const chipsynth::Preset* preset);
    
public:
    // Preset access for UI
    const chipsynth::PresetManager& getPresetManager() const { return presetManager; }
    chipsynth::PresetManager& getPresetManager() { return presetManager; }
    int getCurrentPresetIndex() const { return currentPreset; }
    void setCurrentPreset(int index);
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipSynthAudioProcessor)
};