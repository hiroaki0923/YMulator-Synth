#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
 * Plays a rendered note back visually: three periods of the waveform at a
 * playhead that runs through the render in real time and loops, over a strip
 * showing the level envelope with the key-off marked. The trace keeps the
 * render's own scale so the envelope is visible in the waveform too.
 */
class OutputScope : public juce::Component,
                    private juce::Timer
{
public:
    OutputScope();
    ~OutputScope() override;
    
    void paint(juce::Graphics& g) override;
    void visibilityChanged() override;
    void parentHierarchyChanged() override;
    
    static constexpr int kPeriods = 3;
    static constexpr int kEnvelopeBins = 160;
    static constexpr double kLoopPauseSeconds = 0.35;
    
    /** Takes a mono render, the period of its note and the render's sample rate; `noteOffSample` marks the key-off. */
    void setWaveform(const std::vector<float>& samples, double periodInSamples, double sampleRate, size_t noteOffSample);
    
    /** Shown in place of the trace while the render is silent, e.g. "no carrier is on". */
    void setSilenceHint(const juce::String& hint) { silenceHint = hint; repaint(); }
    
    /** Moves the playhead to `seconds` into the render (used by tests and snapshots). */
    void setPlayhead(double seconds);
    double playheadSeconds() const { return playhead; }
    
    const std::vector<float>& shownFrame() const { return frame; }
    const std::vector<float>& envelope() const { return envelopeBins; }
    float peakLevel() const { return peak; }
    
private:
    void timerCallback() override;
    void updateTimer();
    void selectFrame();
    
    std::vector<float> samples, frame, envelopeBins;
    double period = 1.0, rate = 48000.0, playhead = 0.0, loopStartMs = 0.0;
    size_t noteOff = 0;
    float peak = 0.0f;
    juce::String silenceHint;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputScope)
};
