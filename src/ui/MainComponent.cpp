#include "MainComponent.h"
#include "../PluginProcessor.h"

MainComponent::MainComponent(ChipSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupGlobalControls();
    setupOperatorPanels();
    setupPresetSelector();
    
    // Listen for parameter state changes
    audioProcessor.getParameters().state.addListener(this);
    
    // Ensure preset list is up to date when UI is opened
    updatePresetComboBox();
    
    setSize(800, 600);
}

MainComponent::~MainComponent()
{
    // Remove listener to avoid dangling pointer
    audioProcessor.getParameters().state.removeListener(this);
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
    auto globalLeft = globalArea.removeFromLeft(getWidth() / 3).reduced(10);
    auto globalCenter = globalArea.removeFromLeft(getWidth() / 3).reduced(10);
    auto globalRight = globalArea.reduced(10);
    
    // Algorithm on the left
    auto algorithmArea = globalLeft.removeFromTop(35);
    algorithmLabel->setBounds(algorithmArea.removeFromLeft(80));
    algorithmSlider->setBounds(algorithmArea);
    
    // Feedback in the center
    auto feedbackArea = globalCenter.removeFromTop(35);
    feedbackLabel->setBounds(feedbackArea.removeFromLeft(80));
    feedbackSlider->setBounds(feedbackArea);
    
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
    
    // Feedback slider
    feedbackSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    feedbackSlider->setRange(0, 7, 1);
    feedbackSlider->setValue(0);
    addAndMakeVisible(*feedbackSlider);
    
    feedbackLabel = std::make_unique<juce::Label>("", "Feedback");
    feedbackLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    feedbackLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*feedbackLabel);
    
    // Attach to parameters
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "feedback", *feedbackSlider);
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
    
    // Populate combo box with preset names from PresetManager
    auto presetNames = audioProcessor.getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetComboBox->addItem(presetNames[i], i + 1);
    }
    
    // Set initial selection based on processor's current program
    presetComboBox->setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
    
    presetComboBox->onChange = [this]()
    {
        int selectedIndex = presetComboBox->getSelectedId() - 1;
        if (selectedIndex >= 0 && selectedIndex < audioProcessor.getNumPrograms())
        {
            audioProcessor.setCurrentProgram(selectedIndex);
            // Update host display to sync with DAW
            audioProcessor.updateHostDisplay();
        }
    };
    addAndMakeVisible(*presetComboBox);
    
    presetLabel = std::make_unique<juce::Label>("", "Preset");
    presetLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*presetLabel);
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                           const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged, property);
    
    // Update preset combo box when parameters change
    // Use MessageManager to ensure UI updates happen on the main thread
    juce::MessageManager::callAsync([this]() {
        updatePresetComboBox();
    });
}

void MainComponent::updatePresetComboBox()
{
    // Refresh preset list in case new presets were loaded
    presetComboBox->clear();
    auto presetNames = audioProcessor.getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetComboBox->addItem(presetNames[i], i + 1);
    }
    
    // Add custom preset if active
    if (audioProcessor.isInCustomMode()) {
        presetComboBox->addItem(audioProcessor.getCustomPresetName(), presetNames.size() + 1);
    }
    
    // Update combo box selection to match current program
    // Use dontSendNotification to avoid triggering onChange callback
    presetComboBox->setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
}