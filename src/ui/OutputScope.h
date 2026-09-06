#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
 * Shows a rendered waveform: three periods taken where the sound is loudest,
 * triggered on a rising zero crossing, with the peak level as a bar.
 */
class OutputScope : public juce::Component
{
public:
    OutputScope();
    ~OutputScope() override = default;
    
    void paint(juce::Graphics& g) override;
    
    static constexpr int kPeriods = 3;
    
    /** Takes a mono render and the period of its note in samples. */
    void setWaveform(const std::vector<float>& samples, double periodInSamples);
    const std::vector<float>& shownFrame() const { return frame; }
    float peakLevel() const { return peak; }
    
private:
    std::vector<float> frame;
    float peak = 0.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputScope)
};
