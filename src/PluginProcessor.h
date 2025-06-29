#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/YmfmWrapper.h"
#include "dsp/YmfmWrapperInterface.h"
#include "core/VoiceManager.h"
#include "core/VoiceManagerInterface.h"
#include "core/MidiProcessor.h"
#include "core/MidiProcessorInterface.h"
#include "core/ParameterManager.h"
#include "core/StateManager.h"
#include "core/PanProcessor.h"
#include "utils/PresetManager.h"
#include "core/PresetManagerInterface.h"
#include <unordered_map>
#include <memory>

class YMulatorSynthAudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorParameter::Listener,
                               public juce::ValueTree::Listener
{
public:
    // Default constructor (creates concrete implementations)
    YMulatorSynthAudioProcessor();
    
    // Dependency injection constructor (for testing)
    YMulatorSynthAudioProcessor(std::unique_ptr<YmfmWrapperInterface> ymfmWrapper,
                               std::unique_ptr<VoiceManagerInterface> voiceManager,
                               std::unique_ptr<ymulatorsynth::MidiProcessorInterface> midiProcessor,
                               std::unique_ptr<ymulatorsynth::ParameterManager> parameterManager,
                               std::unique_ptr<PresetManagerInterface> presetManager);
    
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

    int getNumPrograms() override { return stateManager ? stateManager->getNumPrograms() : 1; }
    int getCurrentProgram() override { return stateManager ? stateManager->getCurrentProgram() : 0; }
    void setCurrentProgram(int index) override { if (stateManager) stateManager->setCurrentProgram(index); }
    const juce::String getProgramName(int index) override { return stateManager ? stateManager->getProgramName(index) : "Unknown"; }
    void changeProgramName(int index, const juce::String& newName) override { if (stateManager) stateManager->changeProgramName(index, newName); }

    void getStateInformation(juce::MemoryBlock& destData) override { if (stateManager) stateManager->getStateInformation(destData); }
    void setStateInformation(const void* data, int sizeInBytes) override { if (stateManager) stateManager->setStateInformation(data, sizeInBytes); }
    
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
    // Dependency injection - use interfaces for testability
    std::unique_ptr<YmfmWrapperInterface> ymfmWrapper;
    std::unique_ptr<VoiceManagerInterface> voiceManager;
    std::unique_ptr<ymulatorsynth::MidiProcessorInterface> midiProcessor;
    std::shared_ptr<ymulatorsynth::PanProcessor> panProcessor;
    std::unique_ptr<ymulatorsynth::ParameterManager> parameterManager;
    std::unique_ptr<PresetManagerInterface> presetManager;
    std::unique_ptr<ymulatorsynth::StateManager> stateManager;
    
    // Parameter system
    juce::AudioProcessorValueTreeState parameters;
    bool needsPresetReapply = false;
    
    // Legacy MIDI state (deprecated - TODO: remove after full migration)
    std::unordered_map<int, juce::RangedAudioParameter*> ccToParameterMap;
    int currentPitchBend = 8192;
    
    // State management delegation methods
    void loadPreset(int index) { if (stateManager) stateManager->loadPreset(index); }
    
    // Temporary parameter management (until full migration)
    void updateYmfmParameters() { if (parameterManager) parameterManager->updateYmfmParameters(); }
    void applyGlobalPanToAllChannels() { if (parameterManager) parameterManager->applyGlobalPanToAllChannels(); }
    void setupParameterListeners(bool enable) { if (parameterManager) parameterManager->setupParameterListeners(enable); }
    void loadPresetParameters(const ymulatorsynth::Preset* preset, float& preservedGlobalPan) { 
        if (parameterManager) parameterManager->loadPresetParameters(preset, preservedGlobalPan); 
    }
    void applyPresetToYmfm(const ymulatorsynth::Preset* preset) { 
        if (parameterManager) parameterManager->applyPresetToYmfm(preset); 
    }
    void applyGlobalPan(int channel) { if (parameterManager) parameterManager->applyGlobalPan(channel); }
    void setChannelRandomPan(int channel) { if (parameterManager) parameterManager->setChannelRandomPan(channel); }
    
    // Deprecated MIDI methods (for backward compatibility)
    void setupCCMapping() {} // No-op - handled by MidiProcessor
    void handleMidiCC(int ccNumber, int value) { if (midiProcessor) midiProcessor->handleMidiCC(ccNumber, value); }
    void handlePitchBend(int pitchBendValue) { if (midiProcessor) midiProcessor->handlePitchBend(pitchBendValue); }
    
    // Deprecated parameter layout method (for tests that might still call it)
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Custom preset state access (removed - duplicated in public section)
    
    // Audio processing helper methods
    void processMidiMessages(juce::MidiBuffer& midiMessages);
    void processMidiNoteOn(const juce::MidiMessage& message);
    void processMidiNoteOff(const juce::MidiMessage& message);
    void generateAudioSamples(juce::AudioBuffer<float>& buffer);
    
    // Test isolation helper
    void resetProcessBlockStaticState();
    
    
public:
    // Preset access for UI
    const PresetManagerInterface& getPresetManager() const { return *presetManager; }
    PresetManagerInterface& getPresetManager() { return *presetManager; }
    int getCurrentPresetIndex() const { return stateManager ? stateManager->getCurrentPresetIndex() : 0; }
    juce::StringArray getPresetNames() const { return presetManager->getPresetNames(); }
    
    // Bank access for UI
    juce::StringArray getBankNames() const;
    juce::StringArray getPresetsForBank(int bankIndex) const { return presetManager->getPresetsForBank(bankIndex); }
    void setCurrentPresetInBank(int bankIndex, int presetIndex);
    
    // Custom preset management (delegated to ParameterManager)
    bool isInCustomMode() const { return parameterManager ? parameterManager->isInCustomMode() : false; }
    const juce::String& getCustomPresetName() const { 
        static juce::String empty; 
        return parameterManager ? parameterManager->getCustomPresetName() : empty; 
    }
    void setCustomMode(bool custom, const juce::String& name = juce::String()) { 
        if (parameterManager) parameterManager->setCustomMode(custom, name); 
    }
    
    // OPM file operations
    int loadOpmFile(const juce::File& file);
    bool saveCurrentPresetAsOpm(const juce::File& file, const juce::String& presetName);
    
    // User preset management
    bool saveCurrentPresetToUserBank(const juce::String& presetName);
    
    // Testing interface
    ymulatorsynth::MidiProcessorInterface* getMidiProcessor() { return midiProcessor.get(); }
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YMulatorSynthAudioProcessor)
};