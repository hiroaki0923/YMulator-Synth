#include "PluginProcessor.h"
#include "PluginEditor.h"

ChipSynthAudioProcessorEditor::ChipSynthAudioProcessorEditor(ChipSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(800, 600);
}

ChipSynthAudioProcessorEditor::~ChipSynthAudioProcessorEditor()
{
}

void ChipSynthAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("ChipSynth AU - Coming Soon!", getLocalBounds(), juce::Justification::centred, 1);
}

void ChipSynthAudioProcessorEditor::resized()
{
}