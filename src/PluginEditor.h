#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ChipSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ChipSynthAudioProcessorEditor(ChipSynthAudioProcessor&);
    ~ChipSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ChipSynthAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChipSynthAudioProcessorEditor)
};