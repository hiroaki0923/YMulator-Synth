#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

/**
 * Draws the current YM2151 algorithm as a graph: carriers in blue, modulators
 * in magenta, arrows for modulation and a loop on operator 1 when feedback is
 * active. Roles and connections come from AlgorithmInfo; only the operator
 * positions are defined here.
 */
class AlgorithmDisplay : public juce::Component
{
public:
    AlgorithmDisplay();
    ~AlgorithmDisplay() override = default;

    void paint(juce::Graphics& g) override;

    void setAlgorithm(int algorithmNumber);
    void setFeedbackLevel(int feedbackLevel);
    int getAlgorithm() const { return currentAlgorithm; }

    static const juce::Colour carrierColour;
    static const juce::Colour modulatorColour;

private:
    int currentAlgorithm = 0;  // 0-7
    int currentFeedback = 0;   // 0-7

    float boxWidth() const;
    float boxHeight() const;
    juce::Rectangle<float> operatorBox(int op, const juce::Rectangle<float>& bounds) const;
    void drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmDisplay)
};
