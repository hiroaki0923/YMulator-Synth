#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class ChipSynthAudioProcessor;

class OperatorPanel : public juce::Component
{
public:
    OperatorPanel(ChipSynthAudioProcessor& processor, int operatorNumber);
    ~OperatorPanel() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    ChipSynthAudioProcessor& audioProcessor;
    int operatorNum;
    juce::String operatorId;
    
    // Operator controls
    std::unique_ptr<juce::Slider> totalLevelSlider;
    std::unique_ptr<juce::Label> totalLevelLabel;
    
    std::unique_ptr<juce::Slider> attackRateSlider;
    std::unique_ptr<juce::Label> attackRateLabel;
    
    std::unique_ptr<juce::Slider> decay1RateSlider;
    std::unique_ptr<juce::Label> decay1RateLabel;
    
    std::unique_ptr<juce::Slider> decay2RateSlider;
    std::unique_ptr<juce::Label> decay2RateLabel;
    
    std::unique_ptr<juce::Slider> releaseRateSlider;
    std::unique_ptr<juce::Label> releaseRateLabel;
    
    std::unique_ptr<juce::Slider> sustainLevelSlider;
    std::unique_ptr<juce::Label> sustainLevelLabel;
    
    std::unique_ptr<juce::Slider> multipleSlider;
    std::unique_ptr<juce::Label> multipleLabel;
    
    std::unique_ptr<juce::Slider> detune1Slider;
    std::unique_ptr<juce::Label> detune1Label;
    
    std::unique_ptr<juce::Slider> keyScaleSlider;
    std::unique_ptr<juce::Label> keyScaleLabel;
    
    // Parameter attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    
    void setupControls();
    juce::Slider* createSlider(const juce::String& paramId, const juce::String& labelText, 
                               int minVal, int maxVal, int defaultVal);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OperatorPanel)
};