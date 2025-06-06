#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Placeholder for operator panel UI
class OperatorPanel : public juce::Component
{
public:
    OperatorPanel() = default;
    ~OperatorPanel() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
};