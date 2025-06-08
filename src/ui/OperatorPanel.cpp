#include "OperatorPanel.h"
#include "../PluginProcessor.h"

OperatorPanel::OperatorPanel(ChipSynthAudioProcessor& processor, int operatorNumber)
    : audioProcessor(processor), operatorNum(operatorNumber)
{
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
    
    // First column (left)
    auto leftColumn = bounds.removeFromLeft(bounds.getWidth() / 2 - spacing);
    
    // TL (Total Level)
    auto tlArea = leftColumn.removeFromTop(rowHeight);
    totalLevelLabel->setBounds(tlArea.removeFromLeft(labelWidth));
    totalLevelSlider->setBounds(tlArea.reduced(2));
    
    // AR (Attack Rate)
    auto arArea = leftColumn.removeFromTop(rowHeight);
    attackRateLabel->setBounds(arArea.removeFromLeft(labelWidth));
    attackRateSlider->setBounds(arArea.reduced(2));
    
    // D1R (Decay 1 Rate)
    auto d1rArea = leftColumn.removeFromTop(rowHeight);
    decay1RateLabel->setBounds(d1rArea.removeFromLeft(labelWidth));
    decay1RateSlider->setBounds(d1rArea.reduced(2));
    
    // D2R (Decay 2 Rate)
    auto d2rArea = leftColumn.removeFromTop(rowHeight);
    decay2RateLabel->setBounds(d2rArea.removeFromLeft(labelWidth));
    decay2RateSlider->setBounds(d2rArea.reduced(2));
    
    // RR (Release Rate)
    auto rrArea = leftColumn.removeFromTop(rowHeight);
    releaseRateLabel->setBounds(rrArea.removeFromLeft(labelWidth));
    releaseRateSlider->setBounds(rrArea.reduced(2));
    
    // Second column (right)
    auto rightColumn = bounds.removeFromLeft(bounds.getWidth());
    
    // D1L (Sustain Level)
    auto d1lArea = rightColumn.removeFromTop(rowHeight);
    sustainLevelLabel->setBounds(d1lArea.removeFromLeft(labelWidth));
    sustainLevelSlider->setBounds(d1lArea.reduced(2));
    
    // MUL (Multiple)
    auto mulArea = rightColumn.removeFromTop(rowHeight);
    multipleLabel->setBounds(mulArea.removeFromLeft(labelWidth));
    multipleSlider->setBounds(mulArea.reduced(2));
    
    // DT1 (Detune 1)
    auto dt1Area = rightColumn.removeFromTop(rowHeight);
    detune1Label->setBounds(dt1Area.removeFromLeft(labelWidth));
    detune1Slider->setBounds(dt1Area.reduced(2));
    
    // DT2 (Detune 2)
    auto dt2Area = rightColumn.removeFromTop(rowHeight);
    detune2Label->setBounds(dt2Area.removeFromLeft(labelWidth));
    detune2Slider->setBounds(dt2Area.reduced(2));
    
    // KS (Key Scale) - now available for all operators
    auto ksArea = rightColumn.removeFromTop(rowHeight);
    keyScaleLabel->setBounds(ksArea.removeFromLeft(labelWidth));
    keyScaleSlider->setBounds(ksArea.reduced(2));
}

void OperatorPanel::setupControls()
{
    // Total Level (TL) - inverted for UI (higher value = louder)
    createSlider(operatorId + "_tl", "TL", 0, 127, operatorNum == 1 ? 80 : 127);
    
    // Attack Rate (AR)
    createSlider(operatorId + "_ar", "AR", 0, 31, 31);
    
    // Decay 1 Rate (D1R)
    createSlider(operatorId + "_d1r", "D1R", 0, 31, 0);
    
    // Decay 2 Rate (D2R)
    createSlider(operatorId + "_d2r", "D2R", 0, 31, 0);
    
    // Release Rate (RR)
    createSlider(operatorId + "_rr", "RR", 0, 15, 7);
    
    // Sustain Level (D1L)
    createSlider(operatorId + "_d1l", "D1L", 0, 15, 0);
    
    // Multiple (MUL)
    createSlider(operatorId + "_mul", "MUL", 0, 15, 1);
    
    // Detune 1 (DT1)
    createSlider(operatorId + "_dt1", "DT1", 0, 7, 3);
    
    // Detune 2 (DT2)
    createSlider(operatorId + "_dt2", "DT2", 0, 3, 0);
    
    // Key Scale (KS) for all operators
    createSlider(operatorId + "_ks", "KS", 0, 3, 0);
}

juce::Slider* OperatorPanel::createSlider(const juce::String& paramId, const juce::String& labelText,
                                         int minVal, int maxVal, int defaultVal)
{
    auto slider = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
    slider->setRange(minVal, maxVal, 1);
    slider->setValue(defaultVal);
    slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
    addAndMakeVisible(*slider);
    
    auto label = std::make_unique<juce::Label>("", labelText);
    label->setColour(juce::Label::textColourId, juce::Colours::white);
    label->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*label);
    
    // Create parameter attachment
    auto attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), paramId, *slider);
    attachments.push_back(std::move(attachment));
    
    // Store label and slider pointers using proper assignment
    juce::Slider* sliderPtr = slider.get();
    
    if (labelText == "TL") {
        totalLevelLabel = std::move(label);
        totalLevelSlider = std::move(slider);
    }
    else if (labelText == "AR") {
        attackRateLabel = std::move(label);
        attackRateSlider = std::move(slider);
    }
    else if (labelText == "D1R") {
        decay1RateLabel = std::move(label);
        decay1RateSlider = std::move(slider);
    }
    else if (labelText == "D2R") {
        decay2RateLabel = std::move(label);
        decay2RateSlider = std::move(slider);
    }
    else if (labelText == "RR") {
        releaseRateLabel = std::move(label);
        releaseRateSlider = std::move(slider);
    }
    else if (labelText == "D1L") {
        sustainLevelLabel = std::move(label);
        sustainLevelSlider = std::move(slider);
    }
    else if (labelText == "MUL") {
        multipleLabel = std::move(label);
        multipleSlider = std::move(slider);
    }
    else if (labelText == "DT1") {
        detune1Label = std::move(label);
        detune1Slider = std::move(slider);
    }
    else if (labelText == "DT2") {
        detune2Label = std::move(label);
        detune2Slider = std::move(slider);
    }
    else if (labelText == "KS") {
        keyScaleLabel = std::move(label);
        keyScaleSlider = std::move(slider);
    }
    else if (labelText == "FB") {
        keyScaleLabel = std::move(label);  // Reuse KS UI for FB in operator 1
        keyScaleSlider = std::move(slider);
    }
    
    return sliderPtr;
}