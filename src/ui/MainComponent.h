#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "OperatorPanel.h"

class ChipSynthAudioProcessor;

class MainComponent : public juce::Component,
                      public juce::ValueTree::Listener
{
public:
    explicit MainComponent(ChipSynthAudioProcessor& processor);
    ~MainComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ValueTree::Listener overrides
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                 const juce::Identifier& property) override;

private:
    ChipSynthAudioProcessor& audioProcessor;
    
    // Global controls
    std::unique_ptr<juce::Slider> algorithmSlider;
    std::unique_ptr<juce::Label> algorithmLabel;
    
    // Preset selector
    std::unique_ptr<juce::ComboBox> presetComboBox;
    std::unique_ptr<juce::Label> presetLabel;
    
    // Operator panels
    std::array<std::unique_ptr<OperatorPanel>, 4> operatorPanels;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> algorithmAttachment;
    
    void setupGlobalControls();
    void setupOperatorPanels();
    void setupPresetSelector();
    void updatePresetComboBox();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};