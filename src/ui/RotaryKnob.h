#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class RotaryKnob : public juce::Component
{
public:
    RotaryKnob(const juce::String& labelText = "");
    ~RotaryKnob() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    void setValue(double newValue, juce::NotificationType notification = juce::sendNotificationAsync);
    double getValue() const;
    void setRange(double minValue, double maxValue, double stepSize = 1.0);
    void setLabel(const juce::String& labelText);
    
    std::function<void(double)> onValueChange;
    std::function<void()> onGestureStart;
    std::function<void()> onGestureEnd;

private:
    double value = 0.5;
    double minValue = 0.0;
    double maxValue = 1.0;
    double stepSize = 1.0;
    juce::String label;
    
    juce::Point<int> lastMousePos;
    bool isDragging = false;
    
    static constexpr double rotationRange = juce::MathConstants<double>::pi * 1.5; // 270 degrees
    static constexpr double startAngle = juce::MathConstants<double>::pi * 1.25;   // Start at 225 degrees (left bottom)
    
    double normalizedValue() const;
    void setNormalizedValue(double normalizedVal, juce::NotificationType notification);
    double constrainValue(double val) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryKnob)
};