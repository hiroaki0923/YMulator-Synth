#include "OperatorPanel.h"
#include "../PluginProcessor.h"

// Static control specifications - defines all operator controls
const std::vector<ControlSpec> OperatorPanel::controlSpecs = {
    // ADSR-related parameters (left side, closer to graph)
    {"_tl",  "TL",  0, 127, 0,   0, 0},  // Total Level
    {"_ar",  "AR",  0, 31,  31,  0, 1},  // Attack Rate
    {"_d1r", "D1R", 0, 31,  0,   0, 2},  // Decay 1 Rate  
    {"_d1l", "D1L", 0, 15,  0,   0, 3},  // Sustain Level (Decay 1 Level)
    {"_d2r", "D2R", 0, 31,  0,   0, 4},  // Decay 2 Rate
    {"_rr",  "RR",  0, 15,  7,   0, 5},  // Release Rate
    
    // Other parameters (right side)
    {"_mul", "MUL", 0, 15,  1,   0, 6},  // Multiple
    {"_dt1", "DT1", 0, 7,   3,   0, 7},  // Detune 1
    {"_dt2", "DT2", 0, 3,   0,   0, 8},  // Detune 2
    {"_ks",  "KS",  0, 3,   0,   0, 9}   // Key Scale
};

OperatorPanel::OperatorPanel(ChipSynthAudioProcessor& processor, int operatorNumber)
    : audioProcessor(processor), operatorNum(operatorNumber)
{
    CS_ASSERT_OPERATOR(operatorNumber - 1); // operatorNumber is 1-based, assert 0-3
    operatorId = "op" + juce::String(operatorNumber);
    setupControls();
}

void OperatorPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Panel background with border
    g.setColour(juce::Colour(0xff374151));
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);
    
    g.setColour(juce::Colour(0xff6b7280));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1), 5.0f, 1.0f);
    
    // Operator title
    auto titleArea = bounds.removeFromTop(25);
    g.setColour(juce::Colour(0xff1f2937));
    g.fillRoundedRectangle(titleArea.toFloat().reduced(2, 2), 3.0f);
    
    // Draw title text (left side of title area, after SLOT checkbox)
    auto textArea = titleArea.reduced(5, 0);
    textArea.removeFromLeft(30); // Space for SLOT checkbox
    textArea.removeFromRight(60); // Space for AMS Enable
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("Operator " + juce::String(operatorNum), textArea, juce::Justification::centredLeft);
}

void OperatorPanel::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    auto titleArea = bounds.removeFromTop(25); // Title area
    
    // Position SLOT checkbox in title bar (left side)
    if (slotEnableButton != nullptr) {
        auto checkboxArea = titleArea.removeFromLeft(25).reduced(2);
        checkboxArea = checkboxArea.withY(checkboxArea.getY() - 2); // Move up by 2 pixels
        slotEnableButton->setBounds(checkboxArea);
    }
    
    // Position AMS enable button in title bar (right side)
    if (amsEnableButton != nullptr) {
        auto amsCheckboxArea = titleArea.removeFromRight(60).reduced(2);
        amsEnableButton->setBounds(amsCheckboxArea);
    }
    
    // Split the panel: left for envelope display, right for knobs
    auto envelopeArea = bounds.removeFromLeft(bounds.getWidth() * 0.25); // 25% for envelope
    bounds.removeFromLeft(10); // Gap between envelope and knobs
    auto knobArea = bounds; // Remaining 75% for knobs
    
    const int knobSize = 65;
    const int spacing = 5;
    
    // Calculate grid layout for knobs (10 columns x 1 row)
    int cols = 10;
    int rows = 1;
    int colWidth = (knobArea.getWidth() - (spacing * (cols - 1))) / cols;
    int rowHeight = knobArea.getHeight(); // Full height since AMS moved to title
    
    // Layout knobs in grid
    for (int i = 0; i < controls.size(); ++i) {
        auto& control = controls[i];
        int col = i % cols; // Which column (0-4)
        int row = i / cols; // Which row (0-1)
        
        int x = knobArea.getX() + col * (colWidth + spacing);
        int y = knobArea.getY() + row * (rowHeight + spacing);
        
        control.knob->setBounds(x, y, colWidth, juce::jmin(knobSize, rowHeight));
    }
    
    // Position envelope display
    if (envelopeDisplay != nullptr) {
        envelopeDisplay->setBounds(envelopeArea.reduced(2)); // Small margin for border
    }
}

void OperatorPanel::setupControls()
{
    // Create SLOT enable button (in title bar)
    slotEnableButton = std::make_unique<juce::ToggleButton>();
    slotEnableButton->setButtonText(""); // No text to avoid "..." display
    slotEnableButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    slotEnableButton->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff4ade80)); // Green when enabled
    slotEnableButton->setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff6b7280)); // Gray when disabled
    slotEnableButton->setToggleState(true, juce::dontSendNotification); // Default enabled
    addAndMakeVisible(*slotEnableButton);
    
    // Attach SLOT parameter
    slotEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Op::slot_en(operatorNum), *slotEnableButton);
    
    // Create controls from specifications
    controls.reserve(controlSpecs.size());
    
    for (const auto& spec : controlSpecs) {
        createControlFromSpec(spec);
    }
    
    // Create AMS enable button
    amsEnableButton = std::make_unique<juce::ToggleButton>("AMS");
    amsEnableButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(*amsEnableButton);
    
    // Attach to parameter
    amsEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Op::ams_en(operatorNum), *amsEnableButton);
    
    // Create envelope display
    envelopeDisplay = std::make_unique<EnvelopeDisplay>();
    addAndMakeVisible(*envelopeDisplay);
    
    // Update envelope display with current parameter values
    updateEnvelopeDisplay();
    
    CS_DBG("OperatorPanel: Created " + juce::String(controls.size()) + " controls for operator " + juce::String(operatorNum));
}

void OperatorPanel::createControlFromSpec(const ControlSpec& spec)
{
    CS_ASSERT_PARAMETER_RANGE(spec.minValue, 0, 255);
    CS_ASSERT_PARAMETER_RANGE(spec.maxValue, spec.minValue, 255);
    CS_ASSERT_PARAMETER_RANGE(spec.defaultValue, spec.minValue, spec.maxValue);
    
    // Create the control pair
    ControlPair controlPair;
    controlPair.spec = spec;
    
    // Create rotary knob
    controlPair.knob = std::make_unique<RotaryKnob>(spec.labelText);
    controlPair.knob->setRange(spec.minValue, spec.maxValue, 1.0);
    controlPair.knob->setValue(spec.defaultValue);
    
    addAndMakeVisible(*controlPair.knob);
    
    // Create hidden slider for parameter attachment
    controlPair.hiddenSlider = std::make_unique<juce::Slider>();
    controlPair.hiddenSlider->setRange(spec.minValue, spec.maxValue, 1);
    controlPair.hiddenSlider->setValue(spec.defaultValue, juce::dontSendNotification);
    controlPair.hiddenSlider->setVisible(false);
    addAndMakeVisible(*controlPair.hiddenSlider);
    
    // Connect knob and slider bidirectionally
    auto* sliderPtr = controlPair.hiddenSlider.get();
    auto* knobPtr = controlPair.knob.get();
    
    controlPair.hiddenSlider->onValueChange = [knobPtr, sliderPtr, this]() {
        knobPtr->setValue(sliderPtr->getValue(), juce::dontSendNotification);
        updateEnvelopeDisplay();
    };
    
    controlPair.knob->onValueChange = [sliderPtr, this](double value) {
        sliderPtr->setValue(value, juce::sendNotificationSync);
        updateEnvelopeDisplay();
    };
    
    // Add gesture support for custom preset detection  
    controlPair.knob->onGestureStart = [this, paramIdSuffix = spec.paramIdSuffix]() {
        juce::String paramId = operatorId + paramIdSuffix;
        if (auto* param = audioProcessor.getParameters().getParameter(paramId)) {
            param->beginChangeGesture();
        }
    };
    
    controlPair.knob->onGestureEnd = [this, paramIdSuffix = spec.paramIdSuffix]() {
        juce::String paramId = operatorId + paramIdSuffix;
        if (auto* param = audioProcessor.getParameters().getParameter(paramId)) {
            param->endChangeGesture();
        }
    };
    
    // Create parameter attachment
    juce::String paramId = operatorId + spec.paramIdSuffix;
    controlPair.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), paramId, *controlPair.hiddenSlider);
    
    // Add to controls vector
    controls.push_back(std::move(controlPair));
    
    CS_DBG("Created control: " + paramId + " (" + spec.labelText + ")");
}

void OperatorPanel::updateEnvelopeDisplay()
{
    if (!envelopeDisplay) return;
    
    // Find envelope-related controls and get their values
    int ar = 31, d1r = 0, d1l = 0, d2r = 0, rr = 7; // Default values
    
    for (const auto& control : controls) {
        if (control.spec.paramIdSuffix == "_ar") {
            ar = static_cast<int>(control.knob->getValue());
        } else if (control.spec.paramIdSuffix == "_d1r") {
            d1r = static_cast<int>(control.knob->getValue());
        } else if (control.spec.paramIdSuffix == "_d1l") {
            d1l = static_cast<int>(control.knob->getValue());
        } else if (control.spec.paramIdSuffix == "_d2r") {
            d2r = static_cast<int>(control.knob->getValue());
        } else if (control.spec.paramIdSuffix == "_rr") {
            rr = static_cast<int>(control.knob->getValue());
        }
    }
    
    // Update envelope display with YM2151 parameter values
    envelopeDisplay->setYM2151Parameters(ar, d1r, d1l, d2r, rr);
}