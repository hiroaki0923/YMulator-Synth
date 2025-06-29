#include "GlobalControlsPanel.h"
#include "../PluginProcessor.h"
#include "../utils/Debug.h"
#include "../utils/ParameterIDs.h"

GlobalControlsPanel::GlobalControlsPanel(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setupComponents();
    setupParameterAttachments();
    
    CS_DBG("GlobalControlsPanel created");
}

GlobalControlsPanel::~GlobalControlsPanel()
{
    CS_DBG("GlobalControlsPanel destroyed");
}

void GlobalControlsPanel::paint(juce::Graphics& g)
{
    // No specific painting needed - child components handle their own painting
    juce::ignoreUnused(g);
}

void GlobalControlsPanel::resized()
{
    auto bounds = getLocalBounds();
    
    // Algorithm ComboBox area (label on left, combo on right)
    auto algorithmArea = bounds.removeFromLeft(175).reduced(5);
    if (algorithmComboBox && algorithmLabel) {
        auto algLabelArea = algorithmArea.removeFromLeft(30);
        algorithmLabel->setBounds(algLabelArea);
        // Center combo box vertically with standardized height
        auto centeredComboArea = algorithmArea.withHeight(30).withCentre(algorithmArea.getCentre());
        algorithmComboBox->setBounds(centeredComboArea);
    }
    
    // Feedback Knob area
    auto feedbackArea = bounds.removeFromLeft(105);
    if (feedbackKnob) {
        feedbackKnob->setBounds(feedbackArea);
    }
    
    // Global Pan ComboBox area (label on left, combo on right)
    auto globalPanArea = bounds.removeFromLeft(100);
    if (globalPanComboBox && globalPanLabel) {
        auto panLabelArea = globalPanArea.removeFromLeft(30);
        globalPanLabel->setBounds(panLabelArea);
        // Center combo box vertically with standardized height
        auto centeredPanArea = globalPanArea.withHeight(30).withCentre(globalPanArea.getCentre());
        globalPanComboBox->setBounds(centeredPanArea);
    }
}

void GlobalControlsPanel::setupComponents()
{
    // Algorithm selector
    algorithmComboBox = std::make_unique<juce::ComboBox>();
    for (int i = 0; i <= 7; ++i) {
        algorithmComboBox->addItem("Algorithm " + juce::String(i), i + 1);
    }
    algorithmComboBox->setSelectedId(1, juce::dontSendNotification);
    addAndMakeVisible(*algorithmComboBox);
    
    algorithmLabel = std::make_unique<juce::Label>("", "AL");
    algorithmLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    algorithmLabel->setJustificationType(juce::Justification::centredRight);
    algorithmLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*algorithmLabel);
    
    // Feedback knob
    feedbackKnob = std::make_unique<RotaryKnob>("FB");
    feedbackKnob->setRange(0, 7, 1);
    feedbackKnob->setValue(0);
    feedbackKnob->setAccentColour(juce::Colour(0xff00bfff)); // Fluorescent blue
    addAndMakeVisible(*feedbackKnob);
    
    // Global Pan selector
    globalPanComboBox = std::make_unique<juce::ComboBox>();
    globalPanComboBox->addItemList({"Left", "Center", "Right", "Random"}, 1);
    globalPanComboBox->setSelectedId(2, juce::dontSendNotification); // Default: Center
    addAndMakeVisible(*globalPanComboBox);
    
    globalPanLabel = std::make_unique<juce::Label>("", "PAN");
    globalPanLabel->setColour(juce::Label::textColourId, juce::Colours::white);
    globalPanLabel->setJustificationType(juce::Justification::centredRight);
    globalPanLabel->setFont(juce::Font(12.0f));
    addAndMakeVisible(*globalPanLabel);
}

void GlobalControlsPanel::setupParameterAttachments()
{
    // Algorithm attachment
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::Algorithm, *algorithmComboBox);
    
    // Feedback knob with hidden slider for parameter attachment
    feedbackHiddenSlider = std::make_unique<juce::Slider>();
    feedbackHiddenSlider->setRange(0, 7, 1);
    feedbackHiddenSlider->setValue(0, juce::dontSendNotification);
    feedbackHiddenSlider->setVisible(false);
    addAndMakeVisible(*feedbackHiddenSlider);
    
    // Connect feedback knob and hidden slider
    feedbackHiddenSlider->onValueChange = [this]() {
        if (feedbackKnob) {
            feedbackKnob->setValue(feedbackHiddenSlider->getValue(), juce::dontSendNotification);
        }
    };
    
    feedbackKnob->onValueChange = [this](double value) {
        if (feedbackHiddenSlider) {
            feedbackHiddenSlider->setValue(value, juce::sendNotificationSync);
        }
    };
    
    // Add gesture support for feedback
    feedbackKnob->onGestureStart = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::Feedback)) {
            param->beginChangeGesture();
        }
    };
    
    feedbackKnob->onGestureEnd = [this]() {
        if (auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::Feedback)) {
            param->endChangeGesture();
        }
    };
    
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), ParamID::Global::Feedback, *feedbackHiddenSlider);
    
    // Global Pan attachment
    globalPanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::GlobalPan, *globalPanComboBox);
}