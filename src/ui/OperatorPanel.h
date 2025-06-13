#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"
#include "RotaryKnob.h"
#include "EnvelopeDisplay.h"

class ChipSynthAudioProcessor;

// Control specification for data-driven UI generation
struct ControlSpec {
    std::string paramIdSuffix;
    std::string labelText;
    int minValue;
    int maxValue;
    int defaultValue;
    int column;  // 0 = left, 1 = right
    int row;     // 0-based row in column
};

class OperatorPanel : public juce::Component
{
public:
    OperatorPanel(ChipSynthAudioProcessor& processor, int operatorNumber);
    ~OperatorPanel() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ChipSynthAudioProcessor& audioProcessor;
    int operatorNum;
    juce::String operatorId;
    
    // Data-driven control storage
    struct ControlPair {
        std::unique_ptr<RotaryKnob> knob;
        std::unique_ptr<juce::Slider> hiddenSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        ControlSpec spec;
    };
    
    std::vector<ControlPair> controls;
    
    // SLOT enable checkbox (in title bar)
    std::unique_ptr<juce::ToggleButton> slotEnableButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slotEnableAttachment;
    
    // AMS enable checkbox
    std::unique_ptr<juce::ToggleButton> amsEnableButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> amsEnableAttachment;
    
    // Envelope display
    std::unique_ptr<EnvelopeDisplay> envelopeDisplay;
    
    // Static control specifications
    static const std::vector<ControlSpec> controlSpecs;
    
    void setupControls();
    void createControlFromSpec(const ControlSpec& spec);
    void updateEnvelopeDisplay();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OperatorPanel)
};