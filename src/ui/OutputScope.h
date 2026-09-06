#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include "../dsp/ScopeBuffer.h"

/** Oscilloscope of the plugin output: triggered on a rising zero crossing, with a level bar. */
class OutputScope : public juce::Component,
                    private juce::Timer
{
public:
    explicit OutputScope(const ymulatorsynth::ScopeBuffer& buffer);
    ~OutputScope() override;
    
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;
    
    /** Samples shown across the width. */
    static constexpr int kWindow = 1024;
    
private:
    const ymulatorsynth::ScopeBuffer& scope;
    std::vector<float> frame;
    std::vector<float> latest;
    float peak = 0.0f;
    size_t lastSeen = 0;
    
    void timerCallback() override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputScope)
};
