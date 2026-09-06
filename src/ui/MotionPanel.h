#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"

class YMulatorSynthAudioProcessor;

/**
 * Body of the MOTION card: ready-made motions as chips, the main amounts as
 * knobs, the pan motion picker, and tempo sync with its rate.
 */
class MotionPanel : public juce::Component
{
public:
    explicit MotionPanel(YMulatorSynthAudioProcessor& processor);
    ~MotionPanel() override = default;
    
    void resized() override;
    
    struct MotionPreset {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // parameter id -> plain value
    };
    static const std::vector<MotionPreset>& presets();
    void applyPreset(int index);
    
private:
    struct Knob {
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<juce::TextButton>> chips;
    Knob wide, vibrato, timbre, rate;
    std::unique_ptr<juce::ComboBox> panModeBox, rateDivisionBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> panModeAttachment, rateDivisionAttachment;
    std::unique_ptr<juce::ToggleButton> syncButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    
    void makeKnob(Knob& target, const char* parameterId, const juce::String& label, juce::Colour accent);
    void updateSyncVisibility();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionPanel)
};
