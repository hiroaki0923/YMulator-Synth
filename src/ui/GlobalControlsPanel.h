#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "RotaryKnob.h"

class YMulatorSynthAudioProcessor;

/**
 * GlobalControlsPanel - Extracted from MainComponent
 * 
 * Handles all global synthesis control functionality:
 * - Algorithm ComboBox selection
 * - Feedback knob control
 * - Global pan ComboBox selection
 * - Parameter attachments for DAW automation
 * 
 * Part of Phase 2 refactoring to split MainComponent responsibilities.
 */
class GlobalControlsPanel : public juce::Component
{
public:
    explicit GlobalControlsPanel(YMulatorSynthAudioProcessor& processor);
    ~GlobalControlsPanel() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    YMulatorSynthAudioProcessor& audioProcessor;
    
    // UI Components
    std::unique_ptr<juce::ComboBox> algorithmComboBox;
    std::unique_ptr<juce::Label> algorithmLabel;
    std::unique_ptr<RotaryKnob> feedbackKnob;
    std::unique_ptr<juce::ComboBox> globalPanComboBox;
    std::unique_ptr<juce::Label> globalPanLabel;
    
    // Parameter attachments for DAW automation
    std::unique_ptr<juce::Slider> feedbackHiddenSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> globalPanAttachment;
    
    // Setup methods
    void setupComponents();
    void setupParameterAttachments();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalControlsPanel)
};