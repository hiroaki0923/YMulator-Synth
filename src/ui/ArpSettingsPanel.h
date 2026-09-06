#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "RotaryKnob.h"
#include "KnobBinding.h"

class YMulatorSynthAudioProcessor;

/** The arpeggiator's finer settings, shown in a callout from the Detail MOTION row: chord table, octaves,
    retrigger and gate, latch, accent. */
class ArpSettingsPanel : public juce::Component
{
public:
    explicit ArpSettingsPanel(YMulatorSynthAudioProcessor& processor);
    ~ArpSettingsPanel() override { setLookAndFeel(nullptr); }
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    static constexpr int kWidth = 344, kHeight = 150;
    
private:
    struct Knob { std::unique_ptr<RotaryKnob> knob; KnobBinding binding; };
    YMulatorSynthAudioProcessor& audioProcessor;
    std::unique_ptr<juce::Label> title, chordLabel, accentLabel;
    std::unique_ptr<juce::ComboBox> chordBox, accentBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordAttachment, accentAttachment;
    std::unique_ptr<juce::ToggleButton> retriggerButton, latchButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> retriggerAttachment, latchAttachment;
    Knob octaves, gate, accentDepth;
    
    void makeKnob(Knob& target, const char* parameterId, const juce::String& label, juce::Colour accent,
                  std::function<juce::String(double)> formatter = {});
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpSettingsPanel)
};
