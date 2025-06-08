#include "PluginProcessor.h"
#include "PluginEditor.h"

ChipSynthAudioProcessorEditor::ChipSynthAudioProcessorEditor(ChipSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    mainComponent = std::make_unique<MainComponent>(audioProcessor);
    addAndMakeVisible(*mainComponent);
    setSize(800, 600);
}

ChipSynthAudioProcessorEditor::~ChipSynthAudioProcessorEditor()
{
}

void ChipSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    // MainComponent handles all painting
}

void ChipSynthAudioProcessorEditor::resized()
{
    mainComponent->setBounds(getLocalBounds());
}