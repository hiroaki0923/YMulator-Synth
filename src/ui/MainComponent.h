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
    std::unique_ptr<juce::Slider> feedbackSlider;
    std::unique_ptr<juce::Label> feedbackLabel;
    
    // Preset selector
    std::unique_ptr<juce::ComboBox> presetComboBox;
    std::unique_ptr<juce::Label> presetLabel;
    
    // LFO controls
    std::unique_ptr<juce::Slider> lfoRateSlider;
    std::unique_ptr<juce::Label> lfoRateLabel;
    std::unique_ptr<juce::Slider> lfoAmdSlider;
    std::unique_ptr<juce::Label> lfoAmdLabel;
    std::unique_ptr<juce::Slider> lfoPmdSlider;
    std::unique_ptr<juce::Label> lfoPmdLabel;
    std::unique_ptr<juce::ComboBox> lfoWaveformComboBox;
    std::unique_ptr<juce::Label> lfoWaveformLabel;
    
    // Noise controls
    std::unique_ptr<juce::ToggleButton> noiseEnableButton;
    std::unique_ptr<juce::Label> noiseEnableLabel;
    std::unique_ptr<juce::Slider> noiseFrequencySlider;
    std::unique_ptr<juce::Label> noiseFrequencyLabel;
    
    // Operator panels
    std::array<std::unique_ptr<OperatorPanel>, 4> operatorPanels;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> algorithmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoRateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoAmdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lfoPmdAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveformAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseEnableAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseFrequencyAttachment;
    
    void setupGlobalControls();
    void setupLfoControls();
    void setupOperatorPanels();
    void setupPresetSelector();
    void updatePresetComboBox();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};