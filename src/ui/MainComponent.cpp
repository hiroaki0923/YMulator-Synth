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
    setupPresetSelector();
    setupDisplayComponents();
    
    // Listen for parameter state changes
    audioProcessor.getParameters().state.addListener(this);
    
    // Ensure preset selectors are up to date when UI is opened
    // Note: setupPresetSelector() already calls these, so defer to avoid double initialization
    juce::MessageManager::callAsync([this]() {
        updateBankComboBox();
        updatePresetComboBox();
    });
    
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
    
    // Right side: Bank/Preset selectors - use remaining space (after left side is allocated)
    auto presetArea = topArea.reduced(5);
    
    // Save button on the right first
    auto saveButtonArea = presetArea.removeFromRight(50);
    if (savePresetButton) {
        auto centeredButtonArea = saveButtonArea.withHeight(25).withCentre(saveButtonArea.getCentre());
        savePresetButton->setBounds(centeredButtonArea);
    }
    
    // Bank label and ComboBox
    auto bankLabelArea = presetArea.removeFromLeft(40);
    if (bankLabel) {
        bankLabel->setBounds(bankLabelArea);
    }
    
    auto bankComboArea = presetArea.removeFromLeft(100);
    if (bankComboBox) {
        auto centeredBankArea = bankComboArea.withHeight(30).withCentre(bankComboArea.getCentre());
        bankComboBox->setBounds(centeredBankArea);
    }
    
    // Preset label and ComboBox
    auto presetLabelArea = presetArea.removeFromLeft(50);
    if (presetLabel) {
        presetLabel->setBounds(presetLabelArea);
    }
    
    // Preset ComboBox takes remaining space
    if (presetComboBox) {
        auto centeredPresetArea = presetArea.withHeight(30).withCentre(presetArea.getCentre()).reduced(5, 0);
        presetComboBox->setBounds(centeredPresetArea);
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

void MainComponent::setupPresetSelector()
{
    // Bank selector
    bankComboBox = std::make_unique<juce::ComboBox>();
    bankComboBox->addItem("Factory", 1);
    // Don't set initial selection here - let updateBankComboBox handle it
    bankComboBox->onChange = [this]() { onBankChanged(); };
    addAndMakeVisible(*bankComboBox);
    
    bankLabel = std::make_unique<juce::Label>("", "Bank");
    bankLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    bankLabel->setJustificationType(juce::Justification::centredRight);
    bankLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*bankLabel);
    
    // Preset selector
    presetComboBox = std::make_unique<juce::ComboBox>();
    presetComboBox->onChange = [this]() { onPresetChanged(); };
    addAndMakeVisible(*presetComboBox);
    
    presetLabel = std::make_unique<juce::Label>("", "Preset");
    presetLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    presetLabel->setJustificationType(juce::Justification::centredRight);
    presetLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*presetLabel);
    
    savePresetButton = std::make_unique<juce::TextButton>("Save");
    savePresetButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a5568));
    savePresetButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    savePresetButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    savePresetButton->setTooltip("Save current settings as new preset");
    savePresetButton->onClick = [this]() { savePresetDialog(); };
    // Initially disabled - will be enabled when in custom mode
    savePresetButton->setEnabled(false);
    addAndMakeVisible(*savePresetButton);
    
    // Initialize will be done later to avoid blocking UI
}

void MainComponent::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                           const juce::Identifier& property)
{
    juce::ignoreUnused(treeWhosePropertyHasChanged);
    
    const auto propertyName = property.toString();
    
    // Special handling for preset index changes from DAW
    if (propertyName == "presetIndexChanged") {
        // Add debug output to see if this is being called
        // Only update the preset selectors, not the entire list
        juce::MessageManager::callAsync([this]() {
            updateBankComboBox();
            updatePresetComboBox();
        });
        return;
    }
    
    
    // Special handling for bank list changes (after DAW project load)
    if (propertyName == "bankListUpdated") {
        CS_DBG("Received bankListUpdated notification, refreshing UI");
        juce::MessageManager::callAsync([this]() {
            updateBankComboBox();
            updatePresetComboBox();
        });
        return;
    }
    
    // Define preset-relevant properties that should trigger UI updates
    static const std::set<std::string> presetRelevantProperties = {
        "presetIndex",
        "isCustomMode",
        "currentBankIndex",
        "currentPresetInBank",
        "presetListUpdated",
        "bankListUpdated",
        // Add any other preset management related properties here
        // Note: We deliberately exclude operator parameters, global synthesis params, and channel params
    };
    
    // Filter out properties that don't affect preset display
    if (presetRelevantProperties.find(propertyName.toStdString()) == presetRelevantProperties.end()) {
        // Check if it's an operator parameter (op1_*, op2_*, op3_*, op4_*)
        if (propertyName.startsWith("op") && propertyName.length() >= 3) {
            char opChar = propertyName[2];
            if (opChar >= '1' && opChar <= '4') {
                return;
            }
        }
        
        // Check if it's a global synthesis parameter
        if (propertyName == "algorithm" || propertyName == "feedback" || 
            propertyName == "pitch_bend_range" || propertyName == "master_pan") {
            return;
        }
        
        // Check if it's a channel parameter (ch*_*)
        if (propertyName.startsWith("ch") && propertyName.contains("_")) {
            return;
        }
        
        // If we reach here, it's an unknown parameter - log it but don't update UI
        return;
    }
    
    // This is a preset-relevant property change - update the preset button
    
    // Use MessageManager to ensure UI updates happen on the main thread
    juce::MessageManager::callAsync([this]() {
        updateBankComboBox();
        updatePresetComboBox();
    });
}

void MainComponent::updateBankComboBox()
{
    if (!bankComboBox) return;
    
    // Get current bank names
    auto bankNames = audioProcessor.getBankNames();
    
    // Check if we need to update (including the Import item)
    int expectedItems = bankNames.size() + 1; // +1 for "Import OPM File..."
    bool needsUpdate = (bankComboBox->getNumItems() != expectedItems);
    if (!needsUpdate) {
        for (int i = 0; i < bankNames.size(); ++i) {
            if (bankComboBox->getItemText(i) != bankNames[i]) {
                needsUpdate = true;
                break;
            }
        }
        // Check if last item is still the import option
        if (bankComboBox->getItemText(bankComboBox->getNumItems() - 1) != "Import OPM File...") {
            needsUpdate = true;
        }
    }
    
    if (!needsUpdate) {
        return; // Already up to date
    }
    
    bankComboBox->clear();
    
    // Add all banks
    for (int i = 0; i < bankNames.size(); ++i) {
        bankComboBox->addItem(bankNames[i], i + 1);
    }
    
    // Add separator and import option
    bankComboBox->addSeparator();
    bankComboBox->addItem("Import OPM File...", 9999); // Use high ID to distinguish
    
    // Select bank from ValueTreeState (for DAW persistence)
    int savedBankIndex = 0; // Default to Factory bank
    
    // Try state property first
    auto& state = audioProcessor.getParameters().state;
    if (state.hasProperty(ParamID::Global::CurrentBankIndex)) {
        savedBankIndex = state.getProperty(ParamID::Global::CurrentBankIndex, 0);
        CS_FILE_DBG("Restored bank index from state property: " + juce::String(savedBankIndex));
    } else {
        // Fallback to parameter approach
        auto bankParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentBankIndex);
        if (bankParam) {
            savedBankIndex = static_cast<int>(bankParam->getValue() * (bankParam->getNumSteps() - 1));
            CS_DBG("Restored bank index from parameter: " + juce::String(savedBankIndex) + 
                   " (raw value: " + juce::String(bankParam->getValue()) + ")");
        } else {
            CS_DBG("Bank parameter not found in ValueTreeState");
        }
    }
    
    // Debug: Show all available banks
    CS_DBG("Available banks (" + juce::String(bankNames.size()) + "):");
    for (int i = 0; i < bankNames.size(); ++i) {
        CS_DBG("  [" + juce::String(i) + "] " + bankNames[i]);
    }
    
    // Ensure the bank index is valid
    isUpdatingFromState = true;
    if (savedBankIndex >= 0 && savedBankIndex < bankNames.size()) {
        CS_DBG("Setting bank combo to ID: " + juce::String(savedBankIndex + 1) + " (bank: " + bankNames[savedBankIndex] + ")");
        CS_FILE_DBG("Setting bank combo to ID: " + juce::String(savedBankIndex + 1) + " (bank: " + bankNames[savedBankIndex] + ")");
        bankComboBox->setSelectedId(savedBankIndex + 1, juce::dontSendNotification);
    } else {
        CS_DBG("Bank index invalid (" + juce::String(savedBankIndex) + "), defaulting to Factory (ID: 1)");
        CS_FILE_DBG("Bank index invalid (" + juce::String(savedBankIndex) + "), defaulting to Factory (ID: 1)");
        bankComboBox->setSelectedId(1, juce::dontSendNotification); // Default to Factory
    }
    isUpdatingFromState = false;
}

void MainComponent::updatePresetComboBox()
{
    if (!presetComboBox) return;
    
    // Get presets for currently selected bank
    int selectedBankId = bankComboBox ? bankComboBox->getSelectedId() : 1;
    int bankIndex = selectedBankId - 1; // Convert to 0-based index
    
    // Get preset names for the selected bank
    auto presetNames = audioProcessor.getPresetsForBank(bankIndex);
    
    // Check if we need to update
    bool needsUpdate = (presetComboBox->getNumItems() != presetNames.size());
    if (!needsUpdate) {
        for (int i = 0; i < presetNames.size(); ++i) {
            if (presetComboBox->getItemText(i) != presetNames[i]) {
                needsUpdate = true;
                break;
            }
        }
    }
    
    if (!needsUpdate && !audioProcessor.isInCustomMode()) {
        return; // Already up to date
    }
    
    // Rebuild preset list
    presetComboBox->clear();
    
    for (int i = 0; i < presetNames.size(); ++i)
    {
        presetComboBox->addItem(presetNames[i], i + 1);
    }
    
    // Set current selection (if not in custom mode)
    if (!audioProcessor.isInCustomMode())
    {
        // Get saved preset index from ValueTreeState (for DAW persistence)
        int savedPresetIndex = 7; // Default to Init preset
        
        // Try state property first
        auto& state = audioProcessor.getParameters().state;
        if (state.hasProperty(ParamID::Global::CurrentPresetInBank)) {
            savedPresetIndex = state.getProperty(ParamID::Global::CurrentPresetInBank, 7);
            CS_FILE_DBG("Restored preset index from state property: " + juce::String(savedPresetIndex));
        } else {
            // Fallback to parameter approach
            auto presetParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentPresetInBank);
            if (presetParam) {
                savedPresetIndex = static_cast<int>(presetParam->getValue() * (presetParam->getNumSteps() - 1));
                CS_DBG("Restored preset index from parameter: " + juce::String(savedPresetIndex) + 
                       " (raw value: " + juce::String(presetParam->getValue()) + ")");
            } else {
                CS_DBG("Preset parameter not found in ValueTreeState");
            }
        }
        
        // Ensure the preset index is valid for this bank
        isUpdatingFromState = true;
        if (savedPresetIndex >= 0 && savedPresetIndex < presetNames.size()) {
            CS_DBG("Setting preset combo to ID: " + juce::String(savedPresetIndex + 1));
            CS_FILE_DBG("Setting preset combo to ID: " + juce::String(savedPresetIndex + 1));
            presetComboBox->setSelectedId(savedPresetIndex + 1, juce::dontSendNotification);
        } else {
            CS_DBG("Preset index invalid, using fallback search");
            CS_DBG("savedPresetIndex: " + juce::String(savedPresetIndex) + ", presetNames.size(): " + juce::String(presetNames.size()));
            // Fallback: Find which preset in the current bank matches the global current preset
            int currentGlobalIndex = audioProcessor.getCurrentProgram();
            
            // Find the preset index within the current bank
            for (int i = 0; i < presetNames.size(); ++i) {
                int globalIndex = audioProcessor.getPresetManager().getGlobalPresetIndex(bankIndex, i);
                if (globalIndex == currentGlobalIndex) {
                    presetComboBox->setSelectedId(i + 1, juce::dontSendNotification);
                    break;
                }
            }
        }
        isUpdatingFromState = false;
    }
    
    // Enable/disable Save button based on custom mode
    if (savePresetButton) {
        bool hasChanges = audioProcessor.isInCustomMode();
        savePresetButton->setEnabled(hasChanges);
        
        // Update visual appearance based on state
        if (hasChanges) {
            savePresetButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4a5568));
            savePresetButton->setTooltip("Save modified settings as new preset");
        } else {
            savePresetButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d3748));
            savePresetButton->setTooltip("Save as new preset (modify parameters to enable)");
        }
    }
}

void MainComponent::onBankChanged()
{
    CS_FILE_DBG("onBankChanged called, isUpdatingFromState=" + juce::String(isUpdatingFromState ? "true" : "false"));
    if (!bankComboBox || isUpdatingFromState) return;
    
    int selectedId = bankComboBox->getSelectedId();
    CS_FILE_DBG("onBankChanged: selectedId = " + juce::String(selectedId));
    
    // Check if "Import OPM File..." was selected
    if (selectedId == 9999) {
        CS_DBG("Import OPM File option selected, opening file dialog");
        
        // Reset to previous bank selection (Factory by default)
        bankComboBox->setSelectedId(1, juce::dontSendNotification);
        
        // Open import dialog
        loadOpmFileDialog();
        return;
    }
    
    // Save bank selection to ValueTreeState for DAW persistence
    int bankIndex = selectedId - 1; // Convert to 0-based index
    
    // Save to state property (more reliable for persistence)
    auto& state = audioProcessor.getParameters().state;
    state.setProperty(ParamID::Global::CurrentBankIndex, bankIndex, nullptr);
    CS_DBG("Saved bank index to state property: " + juce::String(bankIndex));
    
    // Also save to parameter for host automation
    auto bankParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentBankIndex);
    if (bankParam) {
        float normalizedValue = bankParam->convertTo0to1(static_cast<float>(bankIndex));
        bankParam->setValueNotifyingHost(normalizedValue);
        CS_DBG("Saved bank index to parameter: " + juce::String(bankIndex) + 
               " (normalized: " + juce::String(normalizedValue) + ")");
    } else {
        CS_DBG("Failed to find bank parameter for saving");
    }
    
    // Normal bank selection - defer update to avoid blocking the dropdown
    juce::MessageManager::callAsync([this]() {
        updatePresetComboBox();
    });
}

void MainComponent::onPresetChanged()
{
    CS_FILE_DBG("onPresetChanged called, isUpdatingFromState=" + juce::String(isUpdatingFromState ? "true" : "false"));
    if (!presetComboBox || !bankComboBox || isUpdatingFromState) return;
    
    int selectedPresetId = presetComboBox->getSelectedId();
    int selectedBankId = bankComboBox->getSelectedId();
    CS_FILE_DBG("onPresetChanged: bankId=" + juce::String(selectedBankId) + ", presetId=" + juce::String(selectedPresetId));
    
    if (selectedPresetId > 0 && selectedBankId > 0)
    {
        // Convert to 0-based indices
        int bankIndex = selectedBankId - 1;
        int presetIndex = selectedPresetId - 1;
        
        // Save preset selection to ValueTreeState for DAW persistence
        auto& state = audioProcessor.getParameters().state;
        state.setProperty(ParamID::Global::CurrentPresetInBank, presetIndex, nullptr);
        CS_DBG("Saved preset index to state property: " + juce::String(presetIndex));
        
        // Also save to parameter for host automation
        auto presetParam = audioProcessor.getParameters().getParameter(ParamID::Global::CurrentPresetInBank);
        if (presetParam) {
            float normalizedValue = presetParam->convertTo0to1(static_cast<float>(presetIndex));
            presetParam->setValueNotifyingHost(normalizedValue);
            CS_DBG("Saved preset index to parameter: " + juce::String(presetIndex) + 
                   " (normalized: " + juce::String(normalizedValue) + ")");
        } else {
            CS_DBG("Failed to find preset parameter for saving");
        }
        
        // Defer the actual change to avoid blocking the dropdown
        juce::MessageManager::callAsync([this, bankIndex, presetIndex]() {
            audioProcessor.setCurrentPresetInBank(bankIndex, presetIndex);
        });
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

void MainComponent::loadOpmFileDialog()
{
    CS_DBG("loadOpmFileDialog() called - opening file chooser");
    
    auto fileChooser = std::make_shared<juce::FileChooser>(
        "Select a VOPM preset file",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.opm"
    );
    
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    
    fileChooser->launchAsync(chooserFlags, [this, fileChooser](const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        
        if (file.existsAsFile())
        {
            
            // Load the OPM file through the audio processor
            int numLoaded = audioProcessor.loadOpmFile(file);
            
            if (numLoaded > 0)
            {
                
                // Update the bank list to include the new bank
                updateBankComboBox();
                
                // Select the newly created bank (it will be the last bank before the Import option)
                auto bankNames = audioProcessor.getBankNames();
                int newBankIndex = bankNames.size(); // Last bank
                if (newBankIndex > 0) {
                    bankComboBox->setSelectedId(newBankIndex, juce::dontSendNotification);
                }
                
                // Update presets for the newly selected bank
                updatePresetComboBox();
                
                // Notify that preset list has been updated
                audioProcessor.getParameters().state.setProperty("presetListUpdated", juce::var(juce::Random::getSystemRandom().nextInt()), nullptr);
                
                // Show success message
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "Load Successful",
                    "Loaded " + juce::String(numLoaded) + " preset(s) from " + file.getFileName()
                );
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::WarningIcon,
                    "Load Error",
                    "Failed to load any presets from: " + file.getFileName()
                );
            }
        }
    });
}


void MainComponent::savePresetDialog()
{
    
    // Get default preset name
    juce::String defaultName = "My Preset";
    if (audioProcessor.isInCustomMode()) {
        defaultName = audioProcessor.getCustomPresetName();
    }
    
    // Create dialog
    auto* dialog = new juce::AlertWindow("Save Preset", 
                                        "Enter a name for the new preset:", 
                                        juce::MessageBoxIconType::QuestionIcon);
    
    dialog->addTextEditor("presetName", defaultName, "Preset Name:");
    dialog->addButton("Save", 1);
    dialog->addButton("Cancel", 0);
    
    // Use a lambda that captures the dialog pointer
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog](int result)
    {
        if (result == 1)
        {
            // Get the preset name from the text editor
            auto* textEditor = dialog->getTextEditor("presetName");
            if (textEditor)
            {
                juce::String presetName = textEditor->getText().trim();
                
                if (presetName.isNotEmpty())
                {
                    
                    // Save directly to User bank (dummy file parameter - not used anymore)
                    savePresetToFile(juce::File{}, presetName);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon,
                        "Invalid Name",
                        "Please enter a valid preset name."
                    );
                }
            }
        }
        
        delete dialog;
    }));
}

void MainComponent::savePresetToFile(const juce::File& file, const juce::String& presetName)
{
    
    // Save the preset to the User bank instead of a file
    if (audioProcessor.saveCurrentPresetToUserBank(presetName))
    {
        
        // Update UI to show the new preset
        updateBankComboBox();
        updatePresetComboBox();
        
        // Select User bank
        auto bankNames = audioProcessor.getBankNames();
        for (int i = 0; i < bankNames.size(); ++i) {
            if (bankNames[i] == "User") {
                bankComboBox->setSelectedId(i + 1, juce::dontSendNotification);
                updatePresetComboBox();
                break;
            }
        }
        
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Save Successful",
            "Preset '" + presetName + "' has been saved to the User bank.\n\n" +
            "Your preset will persist across application restarts."
        );
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync(
            juce::MessageBoxIconType::WarningIcon,
            "Save Error",
            "Failed to save preset '" + presetName + "' to User bank."
        );
    }
}
