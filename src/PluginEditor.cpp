#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "utils/Debug.h"

YMulatorSynthAudioProcessorEditor::YMulatorSynthAudioProcessorEditor(YMulatorSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    CS_FILE_DBG("PluginEditor constructor started");
    
    mainComponent = std::make_unique<MainComponent>(audioProcessor);
    addAndMakeVisible(*mainComponent);
    setSize(800, 600);
    
    CS_FILE_DBG("PluginEditor constructor completed");
}

YMulatorSynthAudioProcessorEditor::~YMulatorSynthAudioProcessorEditor()
{
    CS_FILE_DBG("PluginEditor destructor started");
    
    mainComponent.reset();
    
    CS_FILE_DBG("PluginEditor destructor completed");
}

void YMulatorSynthAudioProcessorEditor::paint([[maybe_unused]] juce::Graphics& g)
{
    CS_FILE_DBG("PluginEditor::paint called - bounds: " + getLocalBounds().toString());
    // MainComponent handles all painting
    CS_FILE_DBG("PluginEditor::paint completed");
}

void YMulatorSynthAudioProcessorEditor::resized()
{
    CS_FILE_DBG("PluginEditor::resized called - bounds: " + getLocalBounds().toString());
    if (mainComponent) {
        mainComponent->setBounds(getLocalBounds());
        CS_FILE_DBG("PluginEditor::resized - MainComponent bounds set to: " + getLocalBounds().toString());
    } else {
        CS_FILE_DBG("PluginEditor::resized - WARNING: mainComponent is null!");
    }
    CS_FILE_DBG("PluginEditor::resized completed");
}