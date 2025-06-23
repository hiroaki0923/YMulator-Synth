#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "OperatorPanel.h"
#include "RotaryKnob.h"
#include "AlgorithmDisplay.h"

class YMulatorSynthAudioProcessor;

class MainComponent : public juce::Component,
                      public juce::ValueTree::Listener
{
public:
    explicit MainComponent(YMulatorSynthAudioProcessor& processor);
    ~MainComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ValueTree::Listener overrides
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                 const juce::Identifier& property) override;

private:
    YMulatorSynthAudioProcessor& audioProcessor;
    
    // Menu bar
    std::unique_ptr<juce::PopupMenu> fileMenu;
    
    // Global controls
    std::unique_ptr<juce::ComboBox> algorithmComboBox;
    std::unique_ptr<juce::Label> algorithmLabel;
    std::unique_ptr<RotaryKnob> feedbackKnob;
    std::unique_ptr<juce::ComboBox> globalPanComboBox;
    std::unique_ptr<juce::Label> globalPanLabel;
    
    // Bank and Preset selectors
    std::unique_ptr<juce::ComboBox> bankComboBox;
    std::unique_ptr<juce::Label> bankLabel;
    std::unique_ptr<juce::ComboBox> presetComboBox;
    std::unique_ptr<juce::Label> presetLabel;
    std::unique_ptr<juce::TextButton> savePresetButton;
    
    // LFO controls
    std::unique_ptr<RotaryKnob> lfoRateKnob;
    std::unique_ptr<RotaryKnob> lfoAmdKnob;
    std::unique_ptr<RotaryKnob> lfoPmdKnob;
    std::unique_ptr<juce::ComboBox> lfoWaveformComboBox;
    std::unique_ptr<juce::Label> lfoWaveformLabel;
    std::unique_ptr<juce::Label> lfoSectionLabel;
    std::unique_ptr<juce::Label> lfoRateLabel;
    std::unique_ptr<juce::Label> lfoAmdLabel;
    std::unique_ptr<juce::Label> lfoPmdLabel;
    
    // Noise controls
    std::unique_ptr<juce::ToggleButton> noiseEnableButton;
    std::unique_ptr<juce::Label> noiseEnableLabel;
    std::unique_ptr<RotaryKnob> noiseFrequencyKnob;
    std::unique_ptr<juce::Label> noiseSectionLabel;
    std::unique_ptr<juce::Label> noiseFreqLabel;
    
    // Operator panels
    std::array<std::unique_ptr<OperatorPanel>, 4> operatorPanels;
    
    // Display components
    std::unique_ptr<AlgorithmDisplay> algorithmDisplay;
    
    // File chooser
    
    // UI update flags
    bool isUpdatingFromState = false;
    
    // Parameter attachments
    std::unique_ptr<juce::Slider> feedbackHiddenSlider;
    std::unique_ptr<juce::Slider> lfoRateHiddenSlider;
    std::unique_ptr<juce::Slider> lfoAmdHiddenSlider;
    std::unique_ptr<juce::Slider> lfoPmdHiddenSlider;
    std::unique_ptr<juce::Slider> noiseFrequencyHiddenSlider;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> globalPanAttachment;
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
    void setupDisplayComponents();
    void updateBankComboBox();
    void updatePresetComboBox();
    void onBankChanged();
    void onPresetChanged();
    void updateAlgorithmDisplay();
    void loadOpmFileDialog();
    void savePresetDialog();
    void savePresetToFile(const juce::File& file, const juce::String& presetName);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};