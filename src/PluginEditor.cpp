#include "PluginProcessor.h"
#include "PluginEditor.h"

YMulatorSynthAudioProcessorEditor::YMulatorSynthAudioProcessorEditor(YMulatorSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    mainComponent = std::make_unique<MainComponent>(audioProcessor);
    addAndMakeVisible(*mainComponent);
    setSize(800, 600);
}

YMulatorSynthAudioProcessorEditor::~YMulatorSynthAudioProcessorEditor()
{
}

void YMulatorSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    // MainComponent handles all painting
}

void YMulatorSynthAudioProcessorEditor::resized()
{
    mainComponent->setBounds(getLocalBounds());
}