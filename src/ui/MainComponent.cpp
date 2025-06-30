#include "MainComponent.h"
#include "../PluginProcessor.h"
#include "../utils/Debug.h"
#include "../utils/ParameterIDs.h"
#include <set>

MainComponent::MainComponent(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    CS_FILE_DBG("MainComponent constructor started");
    
    setupLfoControls();
    CS_FILE_DBG("MainComponent: LFO controls setup complete");
    
    setupOperatorPanels();
    CS_FILE_DBG("MainComponent: Operator panels setup complete");
    
    setupDisplayComponents();
    CS_FILE_DBG("MainComponent: Display components setup complete");
    
    // Setup global controls panel
    globalControlsPanel = std::make_unique<GlobalControlsPanel>(processor);
    addAndMakeVisible(*globalControlsPanel);
    CS_FILE_DBG("MainComponent: Global controls panel setup complete");
    
    // Setup preset UI manager
    presetUIManager = std::make_unique<PresetUIManager>(processor);
    addAndMakeVisible(*presetUIManager);
    CS_FILE_DBG("MainComponent: Preset UI manager setup complete");
    
    // PresetUIManager handles its own ValueTree listening
    
    setSize(1000, 635);  // Original height + menu bar
    
    CS_FILE_DBG("MainComponent constructor completed successfully");
}

MainComponent::~MainComponent()
{
    CS_FILE_DBG("MainComponent destructor started");
    
    // Explicitly reset child components to ensure proper cleanup order
    CS_FILE_DBG("MainComponent: Resetting preset UI manager...");
    presetUIManager.reset();
    
    CS_FILE_DBG("MainComponent: Resetting global controls panel...");
    globalControlsPanel.reset();
    
    CS_FILE_DBG("MainComponent destructor completed");
}

void MainComponent::paint(juce::Graphics& g)
{
    CS_FILE_DBG("MainComponent::paint called - bounds: " + getLocalBounds().toString() + 
                ", isVisible: " + juce::String(isVisible() ? "true" : "false") + 
                ", isShowing: " + juce::String(isShowing() ? "true" : "false"));
    
    // Debug: Check if key child components are visible
    int visibleChildren = 0;
    int totalChildren = getNumChildComponents();
    for (int i = 0; i < totalChildren; ++i) {
        if (getChildComponent(i) && getChildComponent(i)->isVisible()) {
            visibleChildren++;
        }
    }
    CS_FILE_DBG("MainComponent child components - total: " + juce::String(totalChildren) + ", visible: " + juce::String(visibleChildren));
    
    // Debug: Check specific key components
    if (presetUIManager) {
        CS_FILE_DBG("PresetUIManager - visible: " + juce::String(presetUIManager->isVisible() ? "true" : "false") + 
                    ", bounds: " + presetUIManager->getBounds().toString());
    }
    if (globalControlsPanel) {
        CS_FILE_DBG("GlobalControlsPanel - visible: " + juce::String(globalControlsPanel->isVisible() ? "true" : "false") + 
                    ", bounds: " + globalControlsPanel->getBounds().toString());
    }
    
    // Dark blue-gray background similar to VOPM
    g.fillAll(juce::Colour(0xff2d3748));
    
    // Section dividers
    g.setColour(juce::Colour(0xff4a5568));
    g.drawHorizontalLine(60, 0.0f, static_cast<float>(getWidth()));  // After global controls  
    g.drawHorizontalLine(135, 0.0f, static_cast<float>(getWidth())); // After LFO/Noise section
    
    CS_FILE_DBG("MainComponent::paint completed");
}

void MainComponent::resized()
{
    CS_FILE_DBG("MainComponent::resized called");
    auto bounds = getLocalBounds();
    CS_FILE_DBG("MainComponent::resized bounds: " + bounds.toString());
    // Top area for global controls (compact layout without menu space)
    auto topArea = bounds.removeFromTop(60);
    
    // Left side: Global Controls Panel
    auto controlsArea = topArea.removeFromLeft(380);
    if (globalControlsPanel) {
        globalControlsPanel->setBounds(controlsArea);
    }
    
    // Right side: Preset UI Manager - use remaining space (after left side is allocated)
    auto presetArea = topArea.reduced(5);
    if (presetUIManager) {
        presetUIManager->setBounds(presetArea);
    }
    
    // LFO and Noise controls area
    auto lfoArea = bounds.removeFromTop(75);
    
    // Calculate proper Y offset based on current bounds position
    int baseY = lfoArea.getY();
    
    // Define control positions manually for better alignment
    int startX = 50;  // Start after LFO label
    int knobY = baseY + 7;    // Top position for knobs (2px lower)
    int labelY = baseY + 54;  // Label position (2px lower)
    int knobSize = 45; // Slightly smaller to match operator knobs exactly
    int spacing = 80;  // Spacing between controls
    
    // LFO section label
    if (lfoSectionLabel) {
        lfoSectionLabel->setBounds(5, baseY + 15, 40, 50); // Keep original position
    }
    
    // LFO Rate knob and label - positioned separately
    if (lfoRateKnob) {
        lfoRateKnob->setBounds(startX, knobY, knobSize, knobSize);
    }
    if (lfoRateLabel) {
        lfoRateLabel->setBounds(startX, labelY, knobSize, 16);
    }
    
    // LFO AMD knob and label - positioned separately
    if (lfoAmdKnob) {
        lfoAmdKnob->setBounds(startX + spacing, knobY, knobSize, knobSize);
    }
    if (lfoAmdLabel) {
        lfoAmdLabel->setBounds(startX + spacing, labelY, knobSize, 16);
    }
    
    // LFO PMD knob and label - positioned separately
    if (lfoPmdKnob) {
        lfoPmdKnob->setBounds(startX + spacing * 2, knobY, knobSize, knobSize);
    }
    if (lfoPmdLabel) {
        lfoPmdLabel->setBounds(startX + spacing * 2, labelY, knobSize, 16);
    }
    
    // LFO Waveform dropdown and label - positioned separately
    int waveX = startX + spacing * 3;
    lfoWaveformComboBox->setBounds(waveX, knobY + 15, 100, 25); // Center vertically with knobs
    lfoWaveformLabel->setBounds(waveX, labelY, 100, 16);
    
    // Noise section - with gap
    int noiseStartX = waveX + 150; // Gap between LFO and Noise
    
    // Noise section label
    if (noiseSectionLabel) {
        noiseSectionLabel->setBounds(noiseStartX, baseY + 15, 50, 50); // Keep original position
    }
    
    // Noise Enable checkbox and label - positioned separately
    int noiseControlX = noiseStartX + 60;
    noiseEnableButton->setBounds(noiseControlX + 10, knobY + 15, 30, 20); // Wider area to prevent cutoff
    if (noiseEnableLabel) {
        noiseEnableLabel->setBounds(noiseControlX, labelY, 50, 16);
    }
    
    // Noise Frequency knob and label - positioned separately
    if (noiseFrequencyKnob) {
        noiseFrequencyKnob->setBounds(noiseControlX + spacing, knobY, knobSize, knobSize);
    }
    if (noiseFreqLabel) {
        noiseFreqLabel->setBounds(noiseControlX + spacing, labelY, knobSize, 16);
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
    
    CS_FILE_DBG("MainComponent::resized completed");
}



void MainComponent::setupLfoControls()
{
    // LFO section label
    lfoSectionLabel = std::make_unique<juce::Label>("", "LFO");
    lfoSectionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoSectionLabel->setJustificationType(juce::Justification::centred); // Center both horizontally and vertically
    lfoSectionLabel->setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle(juce::Font::bold)));
    addAndMakeVisible(*lfoSectionLabel);
    
    // LFO Rate knob (without label - will be added separately)
    lfoRateKnob = std::make_unique<RotaryKnob>("");
    lfoRateKnob->setRange(0, 255, 1);
    lfoRateKnob->setValue(0);
    lfoRateKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*lfoRateKnob);
    
    // LFO Rate label (below knob)
    lfoRateLabel = std::make_unique<juce::Label>("", "Rate");
    lfoRateLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoRateLabel->setJustificationType(juce::Justification::centred);
    lfoRateLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(*lfoRateLabel);
    
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
    
    // LFO AMD knob (without label - will be added separately)
    lfoAmdKnob = std::make_unique<RotaryKnob>("");
    lfoAmdKnob->setRange(0, 127, 1);
    lfoAmdKnob->setValue(0);
    lfoAmdKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*lfoAmdKnob);
    
    // LFO AMD label (below knob)
    lfoAmdLabel = std::make_unique<juce::Label>("", "AMD");
    lfoAmdLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoAmdLabel->setJustificationType(juce::Justification::centred);
    lfoAmdLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(*lfoAmdLabel);
    
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
    
    // LFO PMD knob (without label - will be added separately)
    lfoPmdKnob = std::make_unique<RotaryKnob>("");
    lfoPmdKnob->setRange(0, 127, 1);
    lfoPmdKnob->setValue(0);
    lfoPmdKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*lfoPmdKnob);
    
    // LFO PMD label (below knob)
    lfoPmdLabel = std::make_unique<juce::Label>("", "PMD");
    lfoPmdLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoPmdLabel->setJustificationType(juce::Justification::centred);
    lfoPmdLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(*lfoPmdLabel);
    
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
    
    lfoWaveformLabel = std::make_unique<juce::Label>("", "Wave");
    lfoWaveformLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoWaveformLabel->setJustificationType(juce::Justification::centred);
    lfoWaveformLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(*lfoWaveformLabel);
    
    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoWaveform, *lfoWaveformComboBox);
    
    // Noise section label
    noiseSectionLabel = std::make_unique<juce::Label>("", "Noise");
    noiseSectionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseSectionLabel->setJustificationType(juce::Justification::centred); // Center both horizontally and vertically
    noiseSectionLabel->setFont(juce::Font(juce::FontOptions().withHeight(16.0f).withStyle(juce::Font::bold)));
    addAndMakeVisible(*noiseSectionLabel);
    
    // Noise Enable toggle button (without text - will be labeled separately)
    noiseEnableButton = std::make_unique<juce::ToggleButton>();
    noiseEnableButton->setButtonText("");
    noiseEnableButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(*noiseEnableButton);
    
    // Noise Enable label (below button)
    noiseEnableLabel = std::make_unique<juce::Label>("", "Enable");
    noiseEnableLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseEnableLabel->setJustificationType(juce::Justification::centred);
    noiseEnableLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(*noiseEnableLabel);
    
    noiseEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Global::NoiseEnable, *noiseEnableButton);
    
    // Noise Frequency knob (without label - will be added separately)
    noiseFrequencyKnob = std::make_unique<RotaryKnob>("");
    noiseFrequencyKnob->setRange(0, 31, 1);
    noiseFrequencyKnob->setValue(16);
    addAndMakeVisible(*noiseFrequencyKnob);
    
    // Noise Frequency label (below knob)
    noiseFreqLabel = std::make_unique<juce::Label>("", "Freq");
    noiseFreqLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseFreqLabel->setJustificationType(juce::Justification::centred);
    noiseFreqLabel->setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
    addAndMakeVisible(*noiseFreqLabel);
    
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
    // Algorithm display is temporarily disabled
    // GlobalControlsPanel now handles algorithm and feedback controls
    // This method is kept for future algorithm display implementation
}

