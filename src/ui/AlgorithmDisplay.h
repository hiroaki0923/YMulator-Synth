#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

/**
 * Shows the current YM2151 algorithm as a diagram: carriers in blue,
 * modulators in magenta, arrows for modulation and the output bus. The
 * drawings are the SVGs in resources/algorithms (see tools/gen_algorithm_svg.py),
 * embedded as binary data and scaled to fit.
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

private:
    int currentAlgorithm = 0;  // 0-7
    int currentFeedback = 0;   // 0-7
    std::array<std::unique_ptr<juce::Drawable>, 8> diagrams;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AlgorithmDisplay)
};
