#include "AlgorithmDisplay.h"
#include "../utils/Debug.h"
#include <cmath>

AlgorithmDisplay::AlgorithmDisplay()
{
    updateAlgorithmLayout();
}

AlgorithmDisplay::~AlgorithmDisplay() = default;

void AlgorithmDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(juce::Colour(0xff1a202c));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Border
    g.setColour(juce::Colour(0xff4a5568));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    auto contentBounds = bounds.reduced(10.0f);
    
    // Draw title
    auto titleArea = contentBounds.removeFromTop(16.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle(juce::Font::bold)));
    g.drawText("Algorithm " + juce::String(currentAlgorithm), titleArea, juce::Justification::centred);
    
    contentBounds.removeFromTop(4.0f); // Small gap
    
    // Draw connections first (so they appear behind operators)
    for (const auto& connection : connections) {
        drawConnection(g, connection, contentBounds);
    }
    
    // Draw feedback loop if present
    if (currentFeedback > 0) {
        drawFeedbackLoop(g, 0, contentBounds); // Feedback typically on operator 1 (index 0)
    }
    
    // Draw operators
    for (const auto& op : operators) {
        drawOperator(g, op, contentBounds);
    }
    
    // Draw feedback level indicator
    if (currentFeedback > 0) {
        auto feedbackArea = bounds.removeFromBottom(16.0f).reduced(10.0f, 0.0f);
        g.setColour(juce::Colour(0xfff59e0b)); // Amber
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.drawText("FB: " + juce::String(currentFeedback), feedbackArea, juce::Justification::centredRight);
    }
}

void AlgorithmDisplay::resized()
{
    updateAlgorithmLayout();
}

void AlgorithmDisplay::setAlgorithm(int algorithmNumber)
{
    if (algorithmNumber >= 0 && algorithmNumber <= 7 && algorithmNumber != currentAlgorithm) {
        currentAlgorithm = algorithmNumber;
        updateAlgorithmLayout();
        repaint();
    }
}

void AlgorithmDisplay::setFeedbackLevel(int feedbackLevel)
{
    if (feedbackLevel >= 0 && feedbackLevel <= 7 && feedbackLevel != currentFeedback) {
        currentFeedback = feedbackLevel;
        repaint();
    }
}

void AlgorithmDisplay::updateAlgorithmLayout()
{
    connections.clear();
    
    // Initialize operator names
    operators[0] = {{0.25f, 0.2f}, false, "M1"};  // Modulator 1
    operators[1] = {{0.75f, 0.2f}, false, "M2"};  // Modulator 2
    operators[2] = {{0.25f, 0.8f}, true, "C1"};   // Carrier 1
    operators[3] = {{0.75f, 0.8f}, true, "C2"};   // Carrier 2
    
    // Setup algorithm-specific layouts
    switch (currentAlgorithm) {
        case 0: setupAlgorithm0(); break;
        case 1: setupAlgorithm1(); break;
        case 2: setupAlgorithm2(); break;
        case 3: setupAlgorithm3(); break;
        case 4: setupAlgorithm4(); break;
        case 5: setupAlgorithm5(); break;
        case 6: setupAlgorithm6(); break;
        case 7: setupAlgorithm7(); break;
        default: setupAlgorithm0(); break;
    }
}

void AlgorithmDisplay::drawOperator(juce::Graphics& g, const OperatorInfo& op, const juce::Rectangle<float>& bounds)
{
    float x = bounds.getX() + op.position.x * bounds.getWidth();
    float y = bounds.getY() + op.position.y * bounds.getHeight();
    float size = 24.0f;
    
    auto opBounds = juce::Rectangle<float>(size, size).withCentre({x, y});
    
    // Operator circle
    if (op.isCarrier) {
        g.setColour(juce::Colour(0xff4ade80)); // Green for carriers
    } else {
        g.setColour(juce::Colour(0xff60a5fa)); // Blue for modulators
    }
    g.fillEllipse(opBounds);
    
    // Border
    g.setColour(juce::Colours::white);
    g.drawEllipse(opBounds, 1.5f);
    
    // Operator label
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle(juce::Font::bold)));
    g.drawText(op.name, opBounds, juce::Justification::centred);
}

void AlgorithmDisplay::drawConnection(juce::Graphics& g, const Connection& conn, const juce::Rectangle<float>& bounds)
{
    if (conn.fromOp < 0 || conn.fromOp >= 4 || conn.toOp < 0 || conn.toOp >= 4) return;
    
    const auto& fromOp = operators[conn.fromOp];
    const auto& toOp = operators[conn.toOp];
    
    float fromX = bounds.getX() + fromOp.position.x * bounds.getWidth();
    float fromY = bounds.getY() + fromOp.position.y * bounds.getHeight();
    float toX = bounds.getX() + toOp.position.x * bounds.getWidth();
    float toY = bounds.getY() + toOp.position.y * bounds.getHeight();
    
    // Adjust points to edge of circles
    float radius = 12.0f;
    juce::Point<float> from(fromX, fromY);
    juce::Point<float> to(toX, toY);
    
    auto diff = to - from;
    auto length = std::sqrt(diff.x * diff.x + diff.y * diff.y);
    auto direction = length > 0.0f ? juce::Point<float>(diff.x / length, diff.y / length) : juce::Point<float>(0.0f, 0.0f);
    from += direction * radius;
    to -= direction * radius;
    
    // Draw connection line
    g.setColour(juce::Colour(0xff9ca3af)); // Gray
    g.drawLine(from.x, from.y, to.x, to.y, 2.0f);
    
    // Draw arrow head
    float arrowSize = 8.0f;
    auto arrowTip = to;
    auto arrowBase = arrowTip - direction * arrowSize;
    auto perpendicular = juce::Point<float>(-direction.y, direction.x) * (arrowSize * 0.5f);
    
    juce::Path arrow;
    arrow.startNewSubPath(arrowTip);
    arrow.lineTo(arrowBase + perpendicular);
    arrow.lineTo(arrowBase - perpendicular);
    arrow.closeSubPath();
    
    g.setColour(juce::Colour(0xff9ca3af));
    g.fillPath(arrow);
}

void AlgorithmDisplay::drawFeedbackLoop(juce::Graphics& g, int operatorIndex, const juce::Rectangle<float>& bounds)
{
    if (operatorIndex < 0 || operatorIndex >= 4) return;
    
    const auto& op = operators[operatorIndex];
    float centerX = bounds.getX() + op.position.x * bounds.getWidth();
    float centerY = bounds.getY() + op.position.y * bounds.getHeight();
    
    // Draw feedback arc
    float radius = 20.0f;
    float startAngle = juce::MathConstants<float>::pi * 0.75f;
    float endAngle = juce::MathConstants<float>::pi * 2.25f;
    
    juce::Path feedbackArc;
    feedbackArc.addCentredArc(centerX + radius * 0.7f, centerY - radius * 0.7f, 
                             radius, radius, 0.0f, startAngle, endAngle, true);
    
    g.setColour(juce::Colour(0xfff59e0b)); // Amber for feedback
    g.strokePath(feedbackArc, juce::PathStrokeType(2.0f));
    
    // Draw arrow at end of arc
    float arrowX = centerX + radius * 0.7f + radius * std::cos(endAngle);
    float arrowY = centerY - radius * 0.7f + radius * std::sin(endAngle);
    
    juce::Path arrow;
    arrow.startNewSubPath(arrowX, arrowY);
    arrow.lineTo(arrowX - 4, arrowY - 6);
    arrow.lineTo(arrowX + 4, arrowY - 6);
    arrow.closeSubPath();
    
    g.fillPath(arrow);
}

// Algorithm 0: M1→M2→C1→C2 (complete series)
void AlgorithmDisplay::setupAlgorithm0()
{
    operators[0].position = {0.2f, 0.2f};  // M1
    operators[1].position = {0.2f, 0.4f};  // M2
    operators[2].position = {0.2f, 0.6f};  // C1
    operators[3].position = {0.2f, 0.8f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = false;
    operators[2].isCarrier = false;
    operators[3].isCarrier = true; // Only final output
    
    connections = {{0, 1}, {1, 2}, {2, 3}};
}

// Algorithm 1: M1→C1, M2→C2 (two parallel chains)
void AlgorithmDisplay::setupAlgorithm1()
{
    operators[0].position = {0.3f, 0.3f};  // M1
    operators[1].position = {0.7f, 0.3f};  // M2
    operators[2].position = {0.3f, 0.7f};  // C1
    operators[3].position = {0.7f, 0.7f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = false;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections = {{0, 2}, {1, 3}};
}

// Algorithm 2: M1→(C1+C2), M2→C2 (branch + parallel)
void AlgorithmDisplay::setupAlgorithm2()
{
    operators[0].position = {0.2f, 0.2f};  // M1
    operators[1].position = {0.8f, 0.3f};  // M2
    operators[2].position = {0.3f, 0.7f};  // C1
    operators[3].position = {0.7f, 0.7f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = false;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections = {{0, 2}, {0, 3}, {1, 3}};
}

// Algorithm 3: M1→C1, M2→C1, C2 (2 input 1 output + parallel)
void AlgorithmDisplay::setupAlgorithm3()
{
    operators[0].position = {0.2f, 0.2f};  // M1
    operators[1].position = {0.5f, 0.2f};  // M2
    operators[2].position = {0.35f, 0.6f}; // C1
    operators[3].position = {0.8f, 0.6f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = false;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections = {{0, 2}, {1, 2}};
}

// Algorithm 4: M1→C1, M2, C2 (1 chain + 2 parallel)
void AlgorithmDisplay::setupAlgorithm4()
{
    operators[0].position = {0.2f, 0.2f};  // M1
    operators[1].position = {0.5f, 0.5f};  // M2
    operators[2].position = {0.2f, 0.7f};  // C1
    operators[3].position = {0.8f, 0.5f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = true;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections = {{0, 2}};
}

// Algorithm 5: M1→(C1+C2+M2) (1 input 3 output)
void AlgorithmDisplay::setupAlgorithm5()
{
    operators[0].position = {0.2f, 0.2f};  // M1
    operators[1].position = {0.5f, 0.7f};  // M2
    operators[2].position = {0.2f, 0.7f};  // C1
    operators[3].position = {0.8f, 0.7f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = true;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections = {{0, 1}, {0, 2}, {0, 3}};
}

// Algorithm 6: M1→(C1+M2), C2 (1 input 2 output + parallel)
void AlgorithmDisplay::setupAlgorithm6()
{
    operators[0].position = {0.2f, 0.2f};  // M1
    operators[1].position = {0.5f, 0.5f};  // M2
    operators[2].position = {0.2f, 0.7f};  // C1
    operators[3].position = {0.8f, 0.5f};  // C2
    
    operators[0].isCarrier = false;
    operators[1].isCarrier = true;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections = {{0, 1}, {0, 2}};
}

// Algorithm 7: M1, M2, C1, C2 (4 parallel outputs)
void AlgorithmDisplay::setupAlgorithm7()
{
    operators[0].position = {0.2f, 0.4f};  // M1
    operators[1].position = {0.4f, 0.4f};  // M2
    operators[2].position = {0.6f, 0.4f};  // C1
    operators[3].position = {0.8f, 0.4f};  // C2
    
    operators[0].isCarrier = true;
    operators[1].isCarrier = true;
    operators[2].isCarrier = true;
    operators[3].isCarrier = true;
    
    connections.clear(); // No connections, all parallel
}