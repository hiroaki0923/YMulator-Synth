#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Placeholder for main UI component
class MainComponent : public juce::Component
{
public:
    MainComponent() = default;
    ~MainComponent() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
};