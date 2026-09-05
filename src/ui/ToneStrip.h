#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"
#include "AlgorithmDisplay.h"
#include "../core/MacroMapper.h"

class YMulatorSynthAudioProcessor;

/**
 * The TONE row: the seven macro knobs (Brightness, Harmonics, Feedback,
 * Attack, Decay, Release, Spread) and the algorithm picker with its diagram.
 * Reports which macro is being touched so the Detail view can highlight the
 * raw knobs it drives.
 */
class ToneStrip : public juce::Component
{
public:
    explicit ToneStrip(YMulatorSynthAudioProcessor& processor);
    ~ToneStrip() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setAlgorithm(int algorithm);
    void setFeedback(int feedback);
    void setHighlightedParameters(const std::set<std::string>& parameterIds);
    std::vector<std::string> highlightedParameterIds() const;
    
    /** Called with the macro under the pointer or being dragged, or nullopt when none. */
    std::function<void(std::optional<ymulatorsynth::Macro>)> onMacroFocus;
    
private:
    struct MacroKnob {
        std::optional<ymulatorsynth::Macro> macro;   // nullopt for the raw Feedback knob
        const char* parameterId;
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
        bool hovered = false;
        bool dragging = false;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::vector<MacroKnob> knobs;
    std::unique_ptr<juce::Label> sectionLabel;
    std::unique_ptr<juce::Label> hintLabel;
    std::unique_ptr<AlgorithmDisplay> algorithmDisplay;
    std::unique_ptr<juce::ComboBox> algorithmComboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmAttachment;
    std::unique_ptr<juce::Label> algorithmLabel;
    int currentAlgorithm = 0;
    
    void addMacroKnob(std::optional<ymulatorsynth::Macro> macro, const char* parameterId, const juce::String& label, juce::Colour accent);
    void updateFocus();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ToneStrip)
};
