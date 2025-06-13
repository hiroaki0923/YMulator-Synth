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
    setupDisplayComponents();
    
    // Listen for parameter state changes
    audioProcessor.getParameters().state.addListener(this);
    
    // Ensure preset list is up to date when UI is opened
    updatePresetComboBox();
    
    setSize(1000, 650);  // Increased width for larger knobs
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
    g.drawHorizontalLine(100, 0.0f, static_cast<float>(getWidth()));  // Adjusted for reduced top area
    g.drawHorizontalLine(160, 0.0f, static_cast<float>(getWidth()));  // LFO section divider (adjusted)
    g.drawVerticalLine(getWidth() / 2, 170.0f, static_cast<float>(getHeight()));  // Vertical divider (adjusted)
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Title area
    bounds.removeFromTop(40);
    
    // Top area for global controls (reduced height)
    auto topArea = bounds.removeFromTop(60);
    
    // Left side: Algorithm ComboBox and Feedback Knob
    auto controlsArea = topArea.removeFromLeft(280);
    auto algorithmArea = controlsArea.removeFromLeft(175).reduced(5);
    auto feedbackArea = controlsArea.removeFromLeft(105);
    
    // Right side: Preset selector
    auto presetArea = topArea.removeFromRight(350).reduced(10);
    
    // Algorithm ComboBox (label on left, combo on right)
    if (algorithmComboBox && algorithmLabel) {
        auto algLabelArea = algorithmArea.removeFromLeft(30);
        algorithmLabel->setBounds(algLabelArea);
        algorithmComboBox->setBounds(algorithmArea);
    }
    
    // Feedback Knob
    if (feedbackKnob) {
        feedbackKnob->setBounds(feedbackArea);
    }
    
    // Algorithm display
    // Algorithm display - temporarily disabled
    // if (algorithmDisplay) {
    //     algorithmDisplay->setBounds(algorithmDisplayArea);
    // }
    
    // Preset selector
    auto presetLabelArea = presetArea.removeFromLeft(60);
    presetLabel->setBounds(presetLabelArea);
    presetComboBox->setBounds(presetArea);
    
    // LFO and Noise controls area
    auto lfoArea = bounds.removeFromTop(60);
    
    // LFO controls - 3 knobs + waveform selector
    auto lfoRateArea = lfoArea.removeFromLeft(100);
    if (lfoRateKnob) {
        lfoRateKnob->setBounds(lfoRateArea);
    }
    
    auto lfoAmdArea = lfoArea.removeFromLeft(100);
    if (lfoAmdKnob) {
        lfoAmdKnob->setBounds(lfoAmdArea);
    }
    
    auto lfoPmdArea = lfoArea.removeFromLeft(100);
    if (lfoPmdKnob) {
        lfoPmdKnob->setBounds(lfoPmdArea);
    }
    
    // LFO Waveform
    auto lfoWaveformArea = lfoArea.removeFromLeft(170);
    auto waveformLabelArea = lfoWaveformArea.removeFromLeft(45);
    auto comboArea = lfoWaveformArea.reduced(5);
    lfoWaveformLabel->setBounds(waveformLabelArea);
    lfoWaveformComboBox->setBounds(comboArea);
    
    // Noise controls
    auto noiseArea = lfoArea.removeFromLeft(200);
    
    // Noise Enable
    auto noiseEnableArea = noiseArea.removeFromLeft(110);
    auto noiseEnableLabelArea = noiseEnableArea.removeFromLeft(40);
    noiseEnableLabel->setBounds(noiseEnableLabelArea);
    noiseEnableButton->setBounds(noiseEnableArea.reduced(2));
    
    // Noise Frequency
    auto noiseFreqArea = noiseArea;
    if (noiseFrequencyKnob) {
        noiseFrequencyKnob->setBounds(noiseFreqArea);
    }
    
    // Operator panels area - use all remaining space
    auto operatorArea = bounds.reduced(10);
    int panelHeight = operatorArea.getHeight() / 4;
    int panelWidth = operatorArea.getWidth();
    
    // Arrange operators in 1x4 vertical layout
    for (int i = 0; i < 4; ++i) {
        operatorPanels[i]->setBounds(operatorArea.getX(), 
                                    operatorArea.getY() + i * panelHeight, 
                                    panelWidth, 
                                    panelHeight);
    }
}

void MainComponent::setupGlobalControls()
{
    // Algorithm ComboBox
    algorithmComboBox = std::make_unique<juce::ComboBox>();
    for (int i = 0; i <= 7; ++i) {
        algorithmComboBox->addItem("Algorithm " + juce::String(i), i + 1);
    }
    algorithmComboBox->setSelectedId(1, juce::dontSendNotification);
    algorithmComboBox->onChange = [this]() {
        updateAlgorithmDisplay();
    };
    addAndMakeVisible(*algorithmComboBox);
    
    algorithmLabel = std::make_unique<juce::Label>("", "AL");
    algorithmLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    algorithmLabel->setJustificationType(juce::Justification::centredRight);
    algorithmLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*algorithmLabel);
    
    // Attach to parameters
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "algorithm", *algorithmComboBox);
    
    // Feedback knob
    feedbackKnob = std::make_unique<RotaryKnob>("FB");
    feedbackKnob->setRange(0, 7, 1);
    feedbackKnob->setValue(0);
    feedbackKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    feedbackKnob->onValueChange = [this](double /*value*/) {
        updateAlgorithmDisplay();
    };
    addAndMakeVisible(*feedbackKnob);
    
    // Create hidden slider for feedback parameter
    feedbackHiddenSlider = std::make_unique<juce::Slider>();
    feedbackHiddenSlider->setRange(0, 7, 1);
    feedbackHiddenSlider->setValue(0, juce::dontSendNotification);
    feedbackHiddenSlider->setVisible(false);
    addAndMakeVisible(*feedbackHiddenSlider);
    
    // Connect knob and slider bidirectionally
    feedbackHiddenSlider->onValueChange = [this]() {
        if (feedbackKnob) {
            feedbackKnob->setValue(feedbackHiddenSlider->getValue(), juce::dontSendNotification);
            updateAlgorithmDisplay();
        }
    };
    
    feedbackKnob->onValueChange = [this](double value) {
        if (feedbackHiddenSlider) {
            feedbackHiddenSlider->setValue(value, juce::sendNotificationSync);
        }
        updateAlgorithmDisplay();
    };
    
    // Add gesture support for custom preset detection
    feedbackKnob->onGestureStart = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter("feedback")) {
            param->beginChangeGesture();
        }
    };
    
    feedbackKnob->onGestureEnd = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter("feedback")) {
            param->endChangeGesture();
        }
    };
    
    // Attach to parameters
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "feedback", *feedbackHiddenSlider);
}

void MainComponent::setupLfoControls()
{
    // LFO Rate knob
    lfoRateKnob = std::make_unique<RotaryKnob>("LFO Rate");
    lfoRateKnob->setRange(0, 255, 1);
    lfoRateKnob->setValue(0);
    lfoRateKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*lfoRateKnob);
    
    // Create hidden slider for LFO Rate parameter
    lfoRateHiddenSlider = std::make_unique<juce::Slider>();
    lfoRateHiddenSlider->setRange(0, 255, 1);
    lfoRateHiddenSlider->setValue(0, juce::dontSendNotification);
    lfoRateHiddenSlider->setVisible(false);
    addAndMakeVisible(*lfoRateHiddenSlider);
    
    // Connect knob and slider
    lfoRateHiddenSlider->onValueChange = [this]() {
        if (lfoRateKnob) {
            lfoRateKnob->setValue(lfoRateHiddenSlider->getValue(), juce::dontSendNotification);
        }
    };
    
    lfoRateKnob->onValueChange = [this](double value) {
        if (lfoRateHiddenSlider) {
            lfoRateHiddenSlider->setValue(value, juce::sendNotificationSync);
        }
    };
    
    // Add gesture support
    lfoRateKnob->onGestureStart = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::LfoRate)) {
            param->beginChangeGesture();
        }
    };
    
    lfoRateKnob->onGestureEnd = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::LfoRate)) {
            param->endChangeGesture();
        }
    };
    
    lfoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoRate, *lfoRateHiddenSlider);
    
    // LFO AMD knob
    lfoAmdKnob = std::make_unique<RotaryKnob>("LFO AMD");
    lfoAmdKnob->setRange(0, 127, 1);
    lfoAmdKnob->setValue(0);
    lfoAmdKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*lfoAmdKnob);
    
    // Create hidden slider for LFO AMD parameter
    lfoAmdHiddenSlider = std::make_unique<juce::Slider>();
    lfoAmdHiddenSlider->setRange(0, 127, 1);
    lfoAmdHiddenSlider->setValue(0, juce::dontSendNotification);
    lfoAmdHiddenSlider->setVisible(false);
    addAndMakeVisible(*lfoAmdHiddenSlider);
    
    // Connect knob and slider
    lfoAmdHiddenSlider->onValueChange = [this]() {
        if (lfoAmdKnob) {
            lfoAmdKnob->setValue(lfoAmdHiddenSlider->getValue(), juce::dontSendNotification);
        }
    };
    
    lfoAmdKnob->onValueChange = [this](double value) {
        if (lfoAmdHiddenSlider) {
            lfoAmdHiddenSlider->setValue(value, juce::sendNotificationSync);
        }
    };
    
    // Add gesture support
    lfoAmdKnob->onGestureStart = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::LfoAmd)) {
            param->beginChangeGesture();
        }
    };
    
    lfoAmdKnob->onGestureEnd = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::LfoAmd)) {
            param->endChangeGesture();
        }
    };
    
    lfoAmdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoAmd, *lfoAmdHiddenSlider);
    
    // LFO PMD knob
    lfoPmdKnob = std::make_unique<RotaryKnob>("LFO PMD");
    lfoPmdKnob->setRange(0, 127, 1);
    lfoPmdKnob->setValue(0);
    lfoPmdKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*lfoPmdKnob);
    
    // Create hidden slider for LFO PMD parameter
    lfoPmdHiddenSlider = std::make_unique<juce::Slider>();
    lfoPmdHiddenSlider->setRange(0, 127, 1);
    lfoPmdHiddenSlider->setValue(0, juce::dontSendNotification);
    lfoPmdHiddenSlider->setVisible(false);
    addAndMakeVisible(*lfoPmdHiddenSlider);
    
    // Connect knob and slider
    lfoPmdHiddenSlider->onValueChange = [this]() {
        if (lfoPmdKnob) {
            lfoPmdKnob->setValue(lfoPmdHiddenSlider->getValue(), juce::dontSendNotification);
        }
    };
    
    lfoPmdKnob->onValueChange = [this](double value) {
        if (lfoPmdHiddenSlider) {
            lfoPmdHiddenSlider->setValue(value, juce::sendNotificationSync);
        }
    };
    
    // Add gesture support
    lfoPmdKnob->onGestureStart = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::LfoPmd)) {
            param->beginChangeGesture();
        }
    };
    
    lfoPmdKnob->onGestureEnd = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::LfoPmd)) {
            param->endChangeGesture();
        }
    };
    
    lfoPmdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoPmd, *lfoPmdHiddenSlider);
    
    // LFO Waveform combo box
    lfoWaveformComboBox = std::make_unique<juce::ComboBox>();
    lfoWaveformComboBox->addItem("Saw", 1);
    lfoWaveformComboBox->addItem("Square", 2);
    lfoWaveformComboBox->addItem("Triangle", 3);
    lfoWaveformComboBox->addItem("Noise", 4);
    lfoWaveformComboBox->setSelectedId(1);
    addAndMakeVisible(*lfoWaveformComboBox);
    
    lfoWaveformLabel = std::make_unique<juce::Label>("", "LFO\nWave");
    lfoWaveformLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoWaveformLabel->setJustificationType(juce::Justification::centredRight);
    lfoWaveformLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*lfoWaveformLabel);
    
    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoWaveform, *lfoWaveformComboBox);
    
    // Noise Enable toggle button
    noiseEnableButton = std::make_unique<juce::ToggleButton>();
    noiseEnableButton->setButtonText("Enable");
    noiseEnableButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(*noiseEnableButton);
    
    noiseEnableLabel = std::make_unique<juce::Label>("", "Noise");
    noiseEnableLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseEnableLabel->setJustificationType(juce::Justification::centredRight);
    noiseEnableLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*noiseEnableLabel);
    
    noiseEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Global::NoiseEnable, *noiseEnableButton);
    
    // Noise Frequency knob
    noiseFrequencyKnob = std::make_unique<RotaryKnob>("Noise Freq");
    noiseFrequencyKnob->setRange(0, 31, 1);
    noiseFrequencyKnob->setValue(16);
    addAndMakeVisible(*noiseFrequencyKnob);
    
    // Create hidden slider for Noise Frequency parameter
    noiseFrequencyHiddenSlider = std::make_unique<juce::Slider>();
    noiseFrequencyHiddenSlider->setRange(0, 31, 1);
    noiseFrequencyHiddenSlider->setValue(16, juce::dontSendNotification);
    noiseFrequencyHiddenSlider->setVisible(false);
    addAndMakeVisible(*noiseFrequencyHiddenSlider);
    
    // Connect knob and slider
    noiseFrequencyHiddenSlider->onValueChange = [this]() {
        if (noiseFrequencyKnob) {
            noiseFrequencyKnob->setValue(noiseFrequencyHiddenSlider->getValue(), juce::dontSendNotification);
        }
    };
    
    noiseFrequencyKnob->onValueChange = [this](double value) {
        if (noiseFrequencyHiddenSlider) {
            noiseFrequencyHiddenSlider->setValue(value, juce::sendNotificationSync);
        }
    };
    
    // Add gesture support
    noiseFrequencyKnob->onGestureStart = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::NoiseFrequency)) {
            param->beginChangeGesture();
        }
    };
    
    noiseFrequencyKnob->onGestureEnd = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::NoiseFrequency)) {
            param->endChangeGesture();
        }
    };
    
    
    noiseFrequencyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::NoiseFrequency, *noiseFrequencyHiddenSlider);
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
    presetLabel->setFont(juce::Font(12.0f));
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

void MainComponent::setupDisplayComponents()
{
    // Create algorithm display
    // Algorithm display - temporarily disabled
    // algorithmDisplay = std::make_unique<AlgorithmDisplay>();
    // addAndMakeVisible(*algorithmDisplay);
    
    // Set initial algorithm and feedback values
    updateAlgorithmDisplay();
}

void MainComponent::updateAlgorithmDisplay()
{
    if (!algorithmDisplay) return;
    
    // Get current values from ComboBox and Knob (if they exist)
    int algorithm = 0;
    int feedback = 0;
    
    if (algorithmComboBox) {
        algorithm = algorithmComboBox->getSelectedId() - 1;
    }
    if (feedbackKnob) {
        feedback = static_cast<int>(feedbackKnob->getValue());
    }
    
    // algorithmDisplay->setAlgorithm(algorithm);
    // algorithmDisplay->setFeedbackLevel(feedback);
}
