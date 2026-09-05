#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "RotaryKnob.h"
#include "KnobBinding.h"

class YMulatorSynthAudioProcessor;

/** Footer row: hardware LFO (rate, depths, waveform) and the noise generator, plus a status line. */
class LfoNoiseStrip : public juce::Component
{
public:
    explicit LfoNoiseStrip(YMulatorSynthAudioProcessor& processor);
    ~LfoNoiseStrip() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    void setStatusText(const juce::String& text);
    
private:
    struct Knob {
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::unique_ptr<juce::Label> lfoLabel, noiseLabel, statusLabel;
    Knob lfoRate, lfoAmd, lfoPmd, noiseFrequency;
    std::unique_ptr<juce::ComboBox> lfoWaveformComboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoWaveformAttachment;
    std::unique_ptr<juce::ToggleButton> noiseEnableButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> noiseEnableAttachment;
    int separatorX = 0;
    
    void makeKnob(Knob& target, const char* parameterId, const juce::String& label);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LfoNoiseStrip)
};
