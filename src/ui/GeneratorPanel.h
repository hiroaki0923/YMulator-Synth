#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <functional>
#include "../core/PatchGenerator.h"

class YMulatorSynthAudioProcessor;

/**
 * Body of the GENERATE card: category chips and six direction sliders.
 * The settings persist in the plugin state so they survive reopening the editor.
 */
class GeneratorPanel : public juce::Component
{
public:
    explicit GeneratorPanel(YMulatorSynthAudioProcessor& processor);
    ~GeneratorPanel() override = default;
    
    void resized() override;
    ymulatorsynth::GeneratorInput currentInput() const;
    
private:
    struct Direction {
        const char* property;
        const char* label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> name, value;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::array<std::unique_ptr<juce::TextButton>, static_cast<size_t>(ymulatorsynth::GeneratorCategory::Count)> chips;
    std::array<Direction, 6> directions;
    
    juce::ValueTree settings() const;
    void storeCategory(ymulatorsynth::GeneratorCategory category);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GeneratorPanel)
};
