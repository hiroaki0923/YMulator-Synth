#include "OperatorPanel.h"
#include "../PluginProcessor.h"

// Static control specifications - defines all operator controls
const std::vector<ControlSpec> OperatorPanel::controlSpecs = {
    // Left column (column 0)
    {"_tl",  "TL",  0, 127, 0,   0, 0},  // Total Level
    {"_ar",  "AR",  0, 31,  31,  0, 1},  // Attack Rate
    {"_d1r", "D1R", 0, 31,  0,   0, 2},  // Decay 1 Rate  
    {"_d2r", "D2R", 0, 31,  0,   0, 3},  // Decay 2 Rate
    {"_rr",  "RR",  0, 15,  7,   0, 4},  // Release Rate
    
    // Right column (column 1)
    {"_d1l", "D1L", 0, 15,  0,   1, 0},  // Sustain Level (Decay 1 Level)
    {"_mul", "MUL", 0, 15,  1,   1, 1},  // Multiple
    {"_dt1", "DT1", 0, 7,   3,   1, 2},  // Detune 1
    {"_dt2", "DT2", 0, 3,   0,   1, 3},  // Detune 2
    {"_ks",  "KS",  0, 3,   0,   1, 4}   // Key Scale
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
    
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("Operator " + juce::String(operatorNum), titleArea, juce::Justification::centred);
}

void OperatorPanel::resized()
{
    auto bounds = getLocalBounds().reduced(5);
    bounds.removeFromTop(25); // Title area
    
    const int rowHeight = 28;
    const int labelWidth = 50;
    const int spacing = 5;
    
    // Split into two columns
    auto leftColumn = bounds.removeFromLeft(bounds.getWidth() / 2 - spacing);
    auto rightColumn = bounds;
    
    // Layout controls based on their column and row specifications
    for (auto& control : controls) {
        juce::Rectangle<int> columnArea = (control.spec.column == 0) ? leftColumn : rightColumn;
        
        // Calculate row position from the top of the column
        auto controlArea = columnArea.withHeight(rowHeight).withY(columnArea.getY() + control.spec.row * rowHeight);
        
        // Layout label and slider
        control.label->setBounds(controlArea.removeFromLeft(labelWidth));
        control.slider->setBounds(controlArea.reduced(2));
    }
    
    // Position AMS enable button at the bottom
    if (amsEnableButton != nullptr) {
        auto amsArea = bounds.removeFromBottom(25).reduced(10, 2);
        amsEnableButton->setBounds(amsArea);
    }
}

void OperatorPanel::setupControls()
{
    // Create controls from specifications
    controls.reserve(controlSpecs.size());
    
    for (const auto& spec : controlSpecs) {
        createControlFromSpec(spec);
    }
    
    // Create AMS enable button
    amsEnableButton = std::make_unique<juce::ToggleButton>("AMS Enable");
    amsEnableButton->setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    addAndMakeVisible(*amsEnableButton);
    
    // Attach to parameter
    amsEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Op::ams_en(operatorNum), *amsEnableButton);
    
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
    
    // Create slider
    controlPair.slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    controlPair.slider->setRange(spec.minValue, spec.maxValue, 1);
    controlPair.slider->setValue(spec.defaultValue);
    controlPair.slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addAndMakeVisible(*controlPair.slider);
    
    // Create label
    controlPair.label = std::make_unique<juce::Label>("", spec.labelText);
    controlPair.label->setColour(juce::Label::textColourId, juce::Colours::white);
    controlPair.label->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*controlPair.label);
    
    // Create parameter attachment
    juce::String paramId = operatorId + spec.paramIdSuffix;
    controlPair.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), paramId, *controlPair.slider);
    
    // Add to controls vector
    controls.push_back(std::move(controlPair));
    
    CS_DBG("Created control: " + paramId + " (" + spec.labelText + ")");
}