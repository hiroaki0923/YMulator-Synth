#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/MainComponent.h"

class YMulatorSynthAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit YMulatorSynthAudioProcessorEditor(YMulatorSynthAudioProcessor&);
    ~YMulatorSynthAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    YMulatorSynthAudioProcessor& audioProcessor;
    std::unique_ptr<MainComponent> mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(YMulatorSynthAudioProcessorEditor)
};