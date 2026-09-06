#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>
#include "../dsp/ScopeBuffer.h"

/** Oscilloscope of the plugin output: three periods of the lowest sounding note, triggered on a rising zero crossing, with a level bar. */
class OutputScope : public juce::Component,
                    private juce::Timer
{
public:
    /** periodInSamples returns the period of the sounding note, or 0 when nothing is known. */
    OutputScope(const ymulatorsynth::ScopeBuffer& buffer, std::function<double()> periodInSamples);
    ~OutputScope() override;
    
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;
    
    static constexpr int kPeriods = 3;        // periods shown across the width
    static constexpr int kMaxWindow = 4096;   // samples shown when no period can be found (3 periods down to about 35 Hz)
    static constexpr int kCapture = 8192;     // samples pulled from the ring each frame
    
private:
    const ymulatorsynth::ScopeBuffer& scope;
    std::function<double()> periodInSamples;
    std::vector<float> frame;
    std::vector<float> latest;
    float peak = 0.0f;
    size_t lastSeen = 0;
    
    void timerCallback() override;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputScope)
};
