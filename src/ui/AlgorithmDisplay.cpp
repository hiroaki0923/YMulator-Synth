#include "AlgorithmDisplay.h"
#include "../dsp/AlgorithmInfo.h"
#include "../utils/Debug.h"
#include "UiTheme.h"

const juce::Colour AlgorithmDisplay::carrierColour   = UiTheme::carrier;
const juce::Colour AlgorithmDisplay::modulatorColour = UiTheme::modulator;

namespace {

// Normalised operator centres per algorithm (x, y in 0..1), in voice order 1-4
using Layout = std::array<juce::Point<float>, 4>;
const std::array<Layout, 8> kLayouts = {{
    {{ {0.50f, 0.12f}, {0.50f, 0.37f}, {0.50f, 0.62f}, {0.50f, 0.87f} }},   // 1>2>3>4
    {{ {0.30f, 0.15f}, {0.70f, 0.15f}, {0.50f, 0.50f}, {0.50f, 0.85f} }},   // (1+2)>3>4
    {{ {0.28f, 0.30f}, {0.72f, 0.12f}, {0.72f, 0.50f}, {0.50f, 0.85f} }},   // (1+(2>3))>4
    {{ {0.28f, 0.12f}, {0.28f, 0.50f}, {0.72f, 0.30f}, {0.50f, 0.85f} }},   // ((1>2)+3)>4
    {{ {0.30f, 0.25f}, {0.30f, 0.75f}, {0.70f, 0.25f}, {0.70f, 0.75f} }},   // (1>2)+(3>4)
    {{ {0.50f, 0.20f}, {0.20f, 0.75f}, {0.50f, 0.75f}, {0.80f, 0.75f} }},   // 1>(2+3+4)
    {{ {0.20f, 0.25f}, {0.20f, 0.75f}, {0.50f, 0.75f}, {0.80f, 0.75f} }},   // (1>2)+3+4
    {{ {0.125f, 0.5f}, {0.375f, 0.5f}, {0.625f, 0.5f}, {0.875f, 0.5f} }},   // 1+2+3+4
}};


} // namespace

AlgorithmDisplay::AlgorithmDisplay() = default;

float AlgorithmDisplay::boxWidth() const
{
    return juce::jlimit(14.0f, 30.0f, static_cast<float>(getWidth()) / 5.0f);
}

float AlgorithmDisplay::boxHeight() const
{
    // Four stacked rows must fit even in the compact TONE-row diagram
    return juce::jmin(boxWidth() * 0.66f, static_cast<float>(getHeight()) / 4.6f);
}

void AlgorithmDisplay::setAlgorithm(int algorithmNumber)
{
    CS_ASSERT_ALGORITHM(algorithmNumber);
    const int clamped = juce::jlimit(0, 7, algorithmNumber);
    if (clamped != currentAlgorithm) {
        currentAlgorithm = clamped;
        repaint();
    }
}

void AlgorithmDisplay::setFeedbackLevel(int feedbackLevel)
{
    CS_ASSERT_FEEDBACK(feedbackLevel);
    const int clamped = juce::jlimit(0, 7, feedbackLevel);
    if (clamped != currentFeedback) {
        currentFeedback = clamped;
        repaint();
    }
}

juce::Rectangle<float> AlgorithmDisplay::operatorBox(int op, const juce::Rectangle<float>& bounds) const
{
    const auto& p = kLayouts[static_cast<size_t>(currentAlgorithm)][static_cast<size_t>(op)];
    const juce::Point<float> centre(bounds.getX() + p.x * bounds.getWidth(),
                                    bounds.getY() + p.y * bounds.getHeight());
    return juce::Rectangle<float>(boxWidth(), boxHeight()).withCentre(centre);
}

void AlgorithmDisplay::drawArrow(juce::Graphics& g, juce::Point<float> from, juce::Point<float> to) const
{
    juce::Line<float> line(from, to);
    g.drawLine(line, 1.5f);
    juce::Path head;
    head.addArrow(line, 1.5f, 7.0f, 6.0f);
    g.fillPath(head);
}

void AlgorithmDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(boxWidth() * 0.5f + 2.0f, boxHeight() * 0.5f + 2.0f);
    const float kBoxHeight = boxHeight();
    const auto& info = ymulatorsynth::algorithmInfo(currentAlgorithm);
    
    // Modulation edges, drawn between box edges rather than centres
    g.setColour(juce::Colour(0xff9aa4b2));
    for (int i = 0; i < info.edgeCount; ++i) {
        const auto& e = info.edges[static_cast<size_t>(i)];
        const auto fromBox = operatorBox(e.from, bounds);
        const auto toBox = operatorBox(e.to, bounds);
        juce::Line<float> centres(fromBox.getCentre(), toBox.getCentre());
        const auto start = fromBox.getConstrainedPoint(centres.getPointAlongLine(kBoxHeight));
        const auto end = toBox.getConstrainedPoint(centres.getPointAlongLine(centres.getLength() - kBoxHeight));
        drawArrow(g, start, end);
    }
    
    // Feedback loop on operator 1
    if (currentFeedback > 0) {
        const auto box = operatorBox(0, bounds);
        juce::Path loop;
        const float r = 8.0f;
        loop.addCentredArc(box.getRight() + 2.0f, box.getY() - 2.0f, r, r, 0.0f,
                           juce::MathConstants<float>::pi * 1.0f, juce::MathConstants<float>::pi * 2.6f, true);
        g.setColour(juce::Colour(0xfff6ad55));
        g.strokePath(loop, juce::PathStrokeType(1.5f));
    }
    
    // Operators
    for (int op = 0; op < 4; ++op) {
        const auto box = operatorBox(op, bounds);
        g.setColour(info.isCarrier(op) ? carrierColour : modulatorColour);
        g.fillRoundedRectangle(box, 3.0f);
        g.setColour(juce::Colours::white);
        g.setFont(UiTheme::mono(juce::jlimit(7.0f, 11.0f, boxHeight() * 0.7f), true));
        g.drawText(juce::String(op + 1), box, juce::Justification::centred);
    }
}
