#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"

class YMulatorSynthAudioProcessor;

/**
 * Body of the MOTION card. Each feature is a chip that switches it on with a
 * sensible setting (or off again); the knobs below set the main amount of
 * every feature, so a "chip-style" sound can be built without the Detail view.
 */
class MotionPanel : public juce::Component
{
public:
    explicit MotionPanel(YMulatorSynthAudioProcessor& processor);
    ~MotionPanel() override = default;
    
    void resized() override;
    
    struct Feature {
        const char* name;
        const char* mainParameter;                                  // non-zero means "on"
        std::vector<std::pair<const char*, float>> onValues;
        std::vector<std::pair<const char*, float>> offValues;
    };
    static const std::vector<Feature>& features();
    
    /** Turns a feature on or off, as clicking its chip does. Returns false for an unknown name. */
    bool toggleFeature(const juce::String& name);
    bool isFeatureOn(const juce::String& name) const;
    /** Switches every motion feature off. */
    void allOff();
    /** Re-reads the parameters and lights the chips accordingly. */
    void refresh();
    
private:
    struct Knob {
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<juce::TextButton>> chips;
    std::unique_ptr<juce::TextButton> offButton;
    Knob wide, vibrato, timbre, echo, rate, sweep, swell, porta, pitch, bright;
    std::unique_ptr<juce::ComboBox> panModeBox, arpModeBox, rateDivisionBox;
    std::unique_ptr<juce::Label> rateDivisionLabel;   // names the note-value box while it stands in for the Vib rate knob
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> panModeAttachment, arpModeAttachment, rateDivisionAttachment;
    std::unique_ptr<juce::ToggleButton> syncButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    
    void makeKnob(Knob& target, const char* parameterId, const juce::String& label, juce::Colour accent, std::function<juce::String(double)> formatter = {});
    void apply(const std::vector<std::pair<const char*, float>>& values);
    float value(const char* parameterId) const;
    void updateSyncVisibility();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionPanel)
};
