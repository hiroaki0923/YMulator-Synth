#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/YmfmWrapper.h"
#include "core/VoiceManager.h"
#include "utils/PresetManager.h"
#include <unordered_map>

class YMulatorSynthAudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorParameter::Listener,
                               public juce::ValueTree::Listener
{
public:
    YMulatorSynthAudioProcessor();
    ~YMulatorSynthAudioProcessor() override;

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
    
    // AudioProcessorParameter::Listener (required by interface)
    void parameterValueChanged(int parameterIndex, float newValue) override;
    void parameterGestureChanged(int parameterIndex, bool gestureIsStarting) override;
    
    // ValueTree::Listener
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                const juce::Identifier& property) override;
    void valueTreeChildAdded(juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded) override {}
    void valueTreeChildRemoved(juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) override {}
    void valueTreeChildOrderChanged(juce::ValueTree& parentTreeWhoseChildrenHaveMoved, int oldIndex, int newIndex) override {}
    void valueTreeParentChanged(juce::ValueTree& treeWhoseParentHasChanged) override {}

    // Parameter access for UI
    juce::AudioProcessorValueTreeState& getParameters() { return parameters; }

private:
    YmfmWrapper ymfmWrapper;
    VoiceManager voiceManager;
    
    // Parameter system
    juce::AudioProcessorValueTreeState parameters;
    std::unordered_map<int, juce::RangedAudioParameter*> ccToParameterMap;
    std::atomic<int> parameterUpdateCounter{0};
    static constexpr int PARAMETER_UPDATE_RATE_DIVIDER = 8;
    
    // Preset management
    ymulatorsynth::PresetManager presetManager;
    int currentPreset = 0;
    bool needsPresetReapply = false;
    bool isCustomPreset = false;
    juce::String customPresetName = "Custom";
    bool userGestureInProgress = false;
    
    // Pitch bend state
    int currentPitchBend = 8192; // MIDI pitch bend center (0-16383)
    
    // Methods
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void setupCCMapping();
    void handleMidiCC(int ccNumber, int value);
    void handlePitchBend(int pitchBendValue);
    void updateYmfmParameters();
    void loadPreset(int index);
    void loadPreset(const ymulatorsynth::Preset* preset);
    void applyGlobalPan(int channel);
    void applyGlobalPanToAllChannels();
    
public:
    // Preset access for UI
    const ymulatorsynth::PresetManager& getPresetManager() const { return presetManager; }
    ymulatorsynth::PresetManager& getPresetManager() { return presetManager; }
    int getCurrentPresetIndex() const { return currentPreset; }
    void setCurrentPreset(int index);
    juce::StringArray getPresetNames() const { return presetManager.getPresetNames(); }
    
    // Bank access for UI
    juce::StringArray getBankNames() const;
    juce::StringArray getPresetsForBank(int bankIndex) const { return presetManager.getPresetsForBank(bankIndex); }
    void setCurrentPresetInBank(int bankIndex, int presetIndex);
    
    // Custom preset management
    bool isInCustomMode() const { return isCustomPreset; }
    juce::String getCustomPresetName() const { return customPresetName; }
    
    // OPM file operations
    int loadOpmFile(const juce::File& file);
    bool saveCurrentPresetAsOpm(const juce::File& file, const juce::String& presetName);
    
    // User preset management
    bool saveCurrentPresetToUserBank(const juce::String& presetName);
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YMulatorSynthAudioProcessor)
};