#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"

class YMulatorSynthAudioProcessor;

/** Detail-view row with every motion parameter: vibrato, wide, timbre LFO, tremolo, pan motion, pitch envelope, sync. */
class MotionStrip : public juce::Component
{
public:
    explicit MotionStrip(YMulatorSynthAudioProcessor& processor);
    ~MotionStrip() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
private:
    struct Knob {
        const char* parameterId;
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
        bool isRate = false;
    };
    struct Group {
        juce::String title;
        std::vector<Knob> knobs;
        std::vector<juce::ComboBox*> boxes;
        juce::Rectangle<int> bounds;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::unique_ptr<juce::Label> sectionLabel;
    std::vector<Group> groups;
    std::unique_ptr<juce::ComboBox> widePanBox, panModeBox, panRateBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> widePanAttachment, panModeAttachment, panRateAttachment;
    std::unique_ptr<juce::ToggleButton> syncButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    
    Group& addGroup(const juce::String& title);
    void addKnob(Group& group, const char* parameterId, const juce::String& label, juce::Colour accent, bool isRate = false,
                 std::function<juce::String(double)> formatter = {});
    void updateRateKnobs();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionStrip)
};
