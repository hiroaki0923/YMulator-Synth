#include "MainComponent.h"
#include "../PluginProcessor.h"
#include "../utils/Debug.h"
#include "../utils/ParameterIDs.h"
#include <set>

MainComponent::MainComponent(ChipSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupGlobalControls();
    setupLfoControls();
    setupOperatorPanels();
    setupPresetSelector();
    
    // Listen for parameter state changes
    audioProcessor.getParameters().state.addListener(this);
    
    // Ensure preset list is up to date when UI is opened
    updatePresetComboBox();
    
    setSize(800, 800);  // Increased height for better operator panel spacing
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
    g.drawHorizontalLine(220, 0.0f, static_cast<float>(getWidth()));  // LFO section divider (adjusted)
    g.drawVerticalLine(getWidth() / 2, 230.0f, static_cast<float>(getHeight()));  // Vertical divider (adjusted)
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
    
    // LFO and Noise controls area (optimized to 100 pixels for 2 rows)
    auto lfoArea = bounds.removeFromTop(100);
    auto lfoLeft = lfoArea.removeFromLeft(getWidth() / 4).reduced(10);
    auto lfoMidLeft = lfoArea.removeFromLeft(getWidth() / 4).reduced(10);
    auto lfoMidRight = lfoArea.removeFromLeft(getWidth() / 4).reduced(10);
    auto lfoRight = lfoArea.reduced(10);
    
    // First row - LFO controls
    // LFO Rate
    auto lfoRateArea = lfoLeft.removeFromTop(35);
    lfoRateLabel->setBounds(lfoRateArea.removeFromLeft(60));
    lfoRateSlider->setBounds(lfoRateArea);
    
    // LFO AMD
    auto lfoAmdArea = lfoMidLeft.removeFromTop(35);
    lfoAmdLabel->setBounds(lfoAmdArea.removeFromLeft(60));
    lfoAmdSlider->setBounds(lfoAmdArea);
    
    // LFO PMD
    auto lfoPmdArea = lfoMidRight.removeFromTop(35);
    lfoPmdLabel->setBounds(lfoPmdArea.removeFromLeft(60));
    lfoPmdSlider->setBounds(lfoPmdArea);
    
    // LFO Waveform
    auto lfoWaveformArea = lfoRight.removeFromTop(35);
    lfoWaveformLabel->setBounds(lfoWaveformArea.removeFromLeft(80));
    lfoWaveformComboBox->setBounds(lfoWaveformArea);
    
    // Second row - Noise controls
    lfoLeft.removeFromTop(10); // Small gap
    lfoMidLeft.removeFromTop(10);
    
    // Noise Enable
    auto noiseEnableArea = lfoLeft.removeFromTop(35);
    noiseEnableLabel->setBounds(noiseEnableArea.removeFromLeft(60));
    noiseEnableButton->setBounds(noiseEnableArea);
    
    // Noise Frequency
    auto noiseFreqArea = lfoMidLeft.removeFromTop(35);
    noiseFrequencyLabel->setBounds(noiseFreqArea.removeFromLeft(80));
    noiseFrequencySlider->setBounds(noiseFreqArea);
    
    // Operator panels area with better vertical spacing
    bounds.removeFromTop(10);  // Add some space above operators
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

void MainComponent::setupLfoControls()
{
    // LFO Rate slider
    lfoRateSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    lfoRateSlider->setRange(0, 255, 1);
    lfoRateSlider->setValue(0);
    addAndMakeVisible(*lfoRateSlider);
    
    lfoRateLabel = std::make_unique<juce::Label>("", "LFO Rate");
    lfoRateLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoRateLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*lfoRateLabel);
    
    lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoRate, *lfoRateSlider);
    
    // LFO AMD slider
    lfoAmdSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    lfoAmdSlider->setRange(0, 127, 1);
    lfoAmdSlider->setValue(0);
    addAndMakeVisible(*lfoAmdSlider);
    
    lfoAmdLabel = std::make_unique<juce::Label>("", "LFO AMD");
    lfoAmdLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoAmdLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*lfoAmdLabel);
    
    lfoAmdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoAmd, *lfoAmdSlider);
    
    // LFO PMD slider
    lfoPmdSlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    lfoPmdSlider->setRange(0, 127, 1);
    lfoPmdSlider->setValue(0);
    addAndMakeVisible(*lfoPmdSlider);
    
    lfoPmdLabel = std::make_unique<juce::Label>("", "LFO PMD");
    lfoPmdLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoPmdLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*lfoPmdLabel);
    
    lfoPmdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoPmd, *lfoPmdSlider);
    
    // LFO Waveform combo box
    lfoWaveformComboBox = std::make_unique<juce::ComboBox>();
    lfoWaveformComboBox->addItem("Saw", 1);
    lfoWaveformComboBox->addItem("Square", 2);
    lfoWaveformComboBox->addItem("Triangle", 3);
    lfoWaveformComboBox->addItem("Noise", 4);
    lfoWaveformComboBox->setSelectedId(1);
    addAndMakeVisible(*lfoWaveformComboBox);
    
    lfoWaveformLabel = std::make_unique<juce::Label>("", "LFO Wave");
    lfoWaveformLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoWaveformLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*lfoWaveformLabel);
    
    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoWaveform, *lfoWaveformComboBox);
    
    // Noise Enable toggle button
    noiseEnableButton = std::make_unique<juce::ToggleButton>();
    noiseEnableButton->setButtonText("Noise Enable");
    noiseEnableButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(*noiseEnableButton);
    
    noiseEnableLabel = std::make_unique<juce::Label>("", "Noise");
    noiseEnableLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseEnableLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*noiseEnableLabel);
    
    noiseEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Global::NoiseEnable, *noiseEnableButton);
    
    // Noise Frequency slider
    noiseFrequencySlider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    noiseFrequencySlider->setRange(0, 31, 1);
    noiseFrequencySlider->setValue(16);
    addAndMakeVisible(*noiseFrequencySlider);
    
    noiseFrequencyLabel = std::make_unique<juce::Label>("", "Noise Freq");
    noiseFrequencyLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseFrequencyLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*noiseFrequencyLabel);
    
    noiseFrequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::NoiseFrequency, *noiseFrequencySlider);
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
    juce::ignoreUnused(treeWhosePropertyHasChanged);
    
    const auto propertyName = property.toString();
    
    // Special handling for preset index changes from DAW
    if (propertyName == "presetIndexChanged") {
        CS_DBG("MainComponent: Handling presetIndexChanged - updating combo box selection only");
        // Only update the combo box selection, not the entire list
        juce::MessageManager::callAsync([this]() {
            presetComboBox->setSelectedId(audioProcessor.getCurrentProgram() + 1, juce::dontSendNotification);
        });
        return;
    }
    
    // Define preset-relevant properties that should trigger UI updates
    static const std::set<std::string> presetRelevantProperties = {
        "presetIndex",
        "isCustomMode",
        // Add any other preset management related properties here
        // Note: We deliberately exclude operator parameters, global synthesis params, and channel params
    };
    
    // Filter out properties that don't affect preset display
    if (presetRelevantProperties.find(propertyName.toStdString()) == presetRelevantProperties.end()) {
        // Check if it's an operator parameter (op1_*, op2_*, op3_*, op4_*)
        if (propertyName.startsWith("op") && propertyName.length() >= 3) {
            char opChar = propertyName[2];
            if (opChar >= '1' && opChar <= '4') {
                CS_DBG("MainComponent: Filtered out operator parameter: " + propertyName);
                return;
            }
        }
        
        // Check if it's a global synthesis parameter
        if (propertyName == "algorithm" || propertyName == "feedback" || 
            propertyName == "pitch_bend_range" || propertyName == "master_pan") {
            CS_DBG("MainComponent: Filtered out global synthesis parameter: " + propertyName);
            return;
        }
        
        // Check if it's a channel parameter (ch*_*)
        if (propertyName.startsWith("ch") && propertyName.contains("_")) {
            CS_DBG("MainComponent: Filtered out channel parameter: " + propertyName);
            return;
        }
        
        // If we reach here, it's an unknown parameter - log it but don't update UI
        CS_DBG("MainComponent: Filtered out unknown parameter: " + propertyName);
        return;
    }
    
    // This is a preset-relevant property change - update the preset combo box
    CS_DBG("MainComponent: Preset-relevant property changed: " + propertyName + " - updating preset combo box");
    
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