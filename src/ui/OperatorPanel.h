#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <set>
#include <string>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"
#include "EnvelopeDisplay.h"

class YMulatorSynthAudioProcessor;

/**
 * One operator row of the Detail view: role tag, the three primary knobs
 * (Level / Ratio / Detune), the envelope with its five rate knobs, and the
 * remaining register controls. Colours follow the operator's role.
 */
class OperatorPanel : public juce::Component
{
public:
    enum class Role { Modulator, Carrier, Noise };
    
    OperatorPanel(YMulatorSynthAudioProcessor& processor, int operatorNumber);
    ~OperatorPanel() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setRole(Role role, const juce::String& hint);
    Role getRole() const { return role; }
    
    /** Draws the amber ring on the knobs whose parameter ids are listed. */
    void setHighlightedParameters(const std::set<std::string>& parameterIds);
    std::vector<std::string> highlightedParameterIds() const;
    
private:
    struct ControlSpec {
        const char* suffix;
        const char* label;
        RotaryKnob::Style style;
        int group;              // 0 primary, 1 envelope, 2 other
    };
    struct Control {
        ControlSpec spec;
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
    };
    static const std::vector<ControlSpec> controlSpecs;
    
    YMulatorSynthAudioProcessor& audioProcessor;
    int operatorNum;
    juce::String operatorId;
    Role role = Role::Carrier;
    juce::String roleHint;
    
    std::vector<Control> controls;
    std::vector<int> separatorXs;
    std::unique_ptr<juce::ToggleButton> slotEnableButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> slotEnableAttachment;
    std::unique_ptr<juce::ToggleButton> amsEnableButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> amsEnableAttachment;
    std::unique_ptr<EnvelopeDisplay> envelopeDisplay;
    
    juce::Colour roleColour() const;
    void createControl(const ControlSpec& spec);
    void applyRoleColours();
    void updateEnvelopeDisplay();
    void updateSubLabels();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OperatorPanel)
};
