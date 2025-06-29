#include "MainComponent.h"
#include "../PluginProcessor.h"
#include "../utils/Debug.h"
#include "../utils/ParameterIDs.h"
#include <set>

MainComponent::MainComponent(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupGlobalControls();
    setupLfoControls();
    setupOperatorPanels();
    setupDisplayComponents();
    
    // Setup preset UI manager
    presetUIManager = std::make_unique<PresetUIManager>(processor);
    addAndMakeVisible(*presetUIManager);
    
    // Listen for parameter state changes
    audioProcessor.getParameters().state.addListener(this);
    
    // PresetUIManager handles its own initialization
    
    setSize(1000, 635);  // Original height + menu bar
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
    
    // Section dividers
    g.setColour(juce::Colour(0xff4a5568));
    g.drawHorizontalLine(60, 0.0f, static_cast<float>(getWidth()));  // After global controls  
    g.drawHorizontalLine(135, 0.0f, static_cast<float>(getWidth())); // After LFO/Noise section
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    // Top area for global controls (compact layout without menu space)
    auto topArea = bounds.removeFromTop(60);
    
    // Left side: Algorithm ComboBox, Feedback Knob, and Global Pan
    auto controlsArea = topArea.removeFromLeft(380);
    auto algorithmArea = controlsArea.removeFromLeft(175).reduced(5);
    auto feedbackArea = controlsArea.removeFromLeft(105);
    auto globalPanArea = controlsArea.removeFromLeft(100);
    
    // Algorithm ComboBox (label on left, combo on right) - standardized height
    if (algorithmComboBox && algorithmLabel) {
        auto algLabelArea = algorithmArea.removeFromLeft(30);
        algorithmLabel->setBounds(algLabelArea);
        // Center combo box vertically with standardized height
        auto centeredComboArea = algorithmArea.withHeight(30).withCentre(algorithmArea.getCentre());
        algorithmComboBox->setBounds(centeredComboArea);
    }
    
    // Feedback Knob
    if (feedbackKnob) {
        feedbackKnob->setBounds(feedbackArea);
    }
    
    // Global Pan ComboBox (label on left, combo on right)
    if (globalPanComboBox && globalPanLabel) {
        auto panLabelArea = globalPanArea.removeFromLeft(30);
        globalPanLabel->setBounds(panLabelArea);
        // Center combo box vertically with standardized height
        auto centeredPanArea = globalPanArea.withHeight(30).withCentre(globalPanArea.getCentre());
        globalPanComboBox->setBounds(centeredPanArea);
    }
    
    // Algorithm display
    // Algorithm display - temporarily disabled
    // if (algorithmDisplay) {
    //     algorithmDisplay->setBounds(algorithmDisplayArea);
    // }
    
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
    
    // Global Pan ComboBox
    globalPanComboBox = std::make_unique<juce::ComboBox>();
    globalPanComboBox->addItemList({"Left", "Center", "Right", "Random"}, 1);
    globalPanComboBox->setSelectedId(2, juce::dontSendNotification); // Default: Center
    addAndMakeVisible(*globalPanComboBox);
    
    globalPanLabel = std::make_unique<juce::Label>("", "PAN");
    globalPanLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    globalPanLabel->setJustificationType(juce::Justification::centredRight);
    globalPanLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*globalPanLabel);
    
    // Attach global pan to parameters
    globalPanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), "global_pan", *globalPanComboBox);
}

void MainComponent::setupLfoControls()
{
    // LFO section label
    lfoSectionLabel = std::make_unique<juce::Label>("", "LFO");
    lfoSectionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    lfoSectionLabel->setJustificationType(juce::Justification::centred); // Center both horizontally and vertically
    lfoSectionLabel->setFont(juce::Font(16.0f, juce::Font::bold));
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
    lfoRateLabel->setFont(juce::Font(12.0f));
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
    lfoAmdLabel->setFont(juce::Font(12.0f));
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
    lfoPmdLabel->setFont(juce::Font(12.0f));
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
    lfoWaveformLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*lfoWaveformLabel);
    
    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoWaveform, *lfoWaveformComboBox);
    
    // Noise section label
    noiseSectionLabel = std::make_unique<juce::Label>("", "Noise");
    noiseSectionLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    noiseSectionLabel->setJustificationType(juce::Justification::centred); // Center both horizontally and vertically
    noiseSectionLabel->setFont(juce::Font(16.0f, juce::Font::bold));
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
    noiseEnableLabel->setFont(juce::Font(12.0f));
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
    noiseFreqLabel->setFont(juce::Font(12.0f));
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


void MainComponent::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                           const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged, property);
    
    // MainComponent now delegates preset-related property changes to PresetUIManager
    // PresetUIManager handles its own ValueTree listening for preset updates
    // MainComponent only needs to handle non-preset related property changes
    
    // Currently no MainComponent-specific properties to handle
    // Algorithm display updates happen through parameter attachments
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

