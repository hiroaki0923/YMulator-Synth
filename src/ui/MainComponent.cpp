#include "MainComponent.h"
#include "../PluginProcessor.h"

MainComponent::MainComponent(ChipSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupGlobalControls();
    setupOperatorPanels();
    setupPresetSelector();
    
    setSize(800, 600);
}

void MainComponent::paint(juce::Graphics& g)
{
    // Dark blue-gray background similar to VOPM
    g.fillAll(juce::Colour(0xff2d3748));
    
    // Title area
    juce::Rectangle<int> titleArea = getLocalBounds().removeFromTop(40);
    g.setColour(juce::Colour(0xff1a202c));
    g.fillRect(titleArea);
    
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("ChipSynth AU - YM2151 FM Synthesizer", titleArea, juce::Justification::centred);
    
    // Section dividers
    g.setColour(juce::Colour(0xff4a5568));
    g.drawHorizontalLine(40, 0.0f, static_cast<float>(getWidth()));
    g.drawHorizontalLine(120, 0.0f, static_cast<float>(getWidth()));
    g.drawVerticalLine(getWidth() / 2, 120.0f, static_cast<float>(getHeight()));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Title area
    bounds.removeFromTop(40);
    
    // Global controls area
    auto globalArea = bounds.removeFromTop(80);
    auto globalLeft = globalArea.removeFromLeft(getWidth() / 2).reduced(10);
    auto globalRight = globalArea.reduced(10);
    
    // Algorithm on the left
    auto algorithmArea = globalLeft.removeFromTop(35);
    algorithmLabel->setBounds(algorithmArea.removeFromLeft(80));
    algorithmSlider->setBounds(algorithmArea);
    
    // Preset selector on the right
    auto presetArea = globalRight.removeFromTop(35);
    presetLabel->setBounds(presetArea.removeFromLeft(80));
    presetComboBox->setBounds(presetArea);
    
    // Operator panels area
    auto operatorArea = bounds.reduced(10);
    int panelWidth = operatorArea.getWidth() / 2;
    int panelHeight = operatorArea.getHeight() / 2;
    
    // Arrange operators in 2x2 grid with proper bounds
    operatorPanels[0]->setBounds(operatorArea.getX(), operatorArea.getY(), panelWidth, panelHeight);
    operatorPanels[1]->setBounds(operatorArea.getX() + panelWidth, operatorArea.getY(), panelWidth, panelHeight);
    operatorPanels[2]->setBounds(operatorArea.getX(), operatorArea.getY() + panelHeight, panelWidth, panelHeight);
    operatorPanels[3]->setBounds(operatorArea.getX() + panelWidth, operatorArea.getY() + panelHeight, panelWidth, panelHeight);
}

void MainComponent::setupGlobalControls()
{
    // Algorithm slider
    algorithmSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    algorithmSlider->setRange(0, 7, 1);
    algorithmSlider->setValue(0);
    addAndMakeVisible(*algorithmSlider);
    
    algorithmLabel = std::make_unique<juce::Label>("", "Algorithm");
    algorithmLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    algorithmLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*algorithmLabel);
    
    // Attach to parameters
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "algorithm", *algorithmSlider);
}

void MainComponent::setupOperatorPanels()
{
    for (int i = 0; i < 4; ++i)
    {
        operatorPanels[i] = std::make_unique<OperatorPanel>(audioProcessor, i + 1);
        addAndMakeVisible(*operatorPanels[i]);
    }
}

void MainComponent::setupPresetSelector()
{
    presetComboBox = std::make_unique<juce::ComboBox>();
    presetComboBox->addItem("Electric Piano", 1);
    presetComboBox->addItem("Synth Bass", 2);
    presetComboBox->addItem("Brass Section", 3);
    presetComboBox->addItem("String Pad", 4);
    presetComboBox->addItem("Lead Synth", 5);
    presetComboBox->addItem("Organ", 6);
    presetComboBox->addItem("Bells", 7);
    presetComboBox->addItem("Init", 8);
    presetComboBox->setSelectedId(8); // Default to Init
    presetComboBox->onChange = [this]()
    {
        int selectedIndex = presetComboBox->getSelectedId() - 1;
        if (selectedIndex >= 0 && selectedIndex < 8)
        {
            audioProcessor.setCurrentProgram(selectedIndex);
        }
    };
    addAndMakeVisible(*presetComboBox);
    
    presetLabel = std::make_unique<juce::Label>("", "Preset");
    presetLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*presetLabel);
}