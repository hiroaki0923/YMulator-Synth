#include "RotaryKnob.h"
#include "../utils/Debug.h"

RotaryKnob::RotaryKnob(const juce::String& labelText)
    : label(labelText)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

RotaryKnob::~RotaryKnob() = default;

void RotaryKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto drawingBounds = bounds;
    
    // If this is an LFO, Noise, or FB label, reserve space on the left
    if (!label.isEmpty() && (label.contains("LFO") || label.contains("Noise") || label == "FB")) {
        drawingBounds.removeFromLeft(35.0f);
    }
    
    auto center = drawingBounds.getCentre();
    
    // Calculate knob area (square, taking the smaller dimension)
    float knobSize = juce::jmin(drawingBounds.getWidth(), drawingBounds.getHeight());
    if (!label.isEmpty() && !label.contains("LFO") && !label.contains("Noise") && label != "FB") {
        knobSize = juce::jmin(knobSize, drawingBounds.getHeight() - 20.0f); // Leave space for bottom label
    }
    // Limit maximum knob size - smaller for top-level controls
    if (label.contains("LFO") || label.contains("Noise") || label == "FB") {
        knobSize = juce::jmin(knobSize, 45.0f); // Smaller for top controls
    } else {
        knobSize = juce::jmin(knobSize, 55.0f); // Regular size for operator knobs
    }
    
    auto knobBounds = juce::Rectangle<float>(knobSize, knobSize).withCentre(center);
    if (!label.isEmpty() && !label.contains("LFO") && !label.contains("Noise") && label != "FB") {
        knobBounds = knobBounds.withY(drawingBounds.getY() + 2.0f); // Move up to leave space for label
    }
    
    float radius = knobSize * 0.35f;
    center = knobBounds.getCentre();
    
    // Draw background circle
    g.setColour(juce::Colour(0xff2d3748));
    g.fillEllipse(knobBounds.reduced(2.0f));
    
    // Draw border
    g.setColour(juce::Colour(0xff4a5568));
    g.drawEllipse(knobBounds.reduced(2.0f), 1.5f);
    
    // Calculate current angle based on value
    double normalizedVal = normalizedValue();
    double currentAngle = startAngle + normalizedVal * rotationRange;
    
    // Draw value arc
    if (normalizedVal > 0.0) {
        g.setColour(accentColour); // Use configurable accent colour
        juce::Path arc;
        arc.addCentredArc(center.x, center.y, radius, radius, 0.0f,
                         static_cast<float>(startAngle),
                         static_cast<float>(currentAngle), true);
        g.strokePath(arc, juce::PathStrokeType(3.0f));
    }
    
    // Draw background arc (remaining portion)
    g.setColour(juce::Colour(0xff374151));
    juce::Path backgroundArc;
    backgroundArc.addCentredArc(center.x, center.y, radius, radius, 0.0f,
                               static_cast<float>(currentAngle),
                               static_cast<float>(startAngle + rotationRange), true);
    g.strokePath(backgroundArc, juce::PathStrokeType(2.0f));
    
    // Pointer removed for cleaner look
    
    // Draw center dot
    g.setColour(juce::Colour(0xff1a202c));
    g.fillEllipse(center.x - 3.0f, center.y - 3.0f, 6.0f, 6.0f);
    
    // Draw label (check if label contains "LFO" or "Noise" or is "FB" for special formatting)
    if (!label.isEmpty()) {
        if (label.contains("LFO") || label.contains("Noise") || label == "FB") {
            // For LFO/Noise/FB labels, draw on the left side
            auto labelArea = bounds.removeFromLeft(35.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
            
            if (label == "FB") {
                // FB label - single line, centered vertically with knob
                auto centerY = knobBounds.getCentreY();
                auto textHeight = g.getCurrentFont().getHeight();
                auto textArea = labelArea.withHeight(textHeight).withCentre({labelArea.getCentreX(), centerY});
                g.drawText(label, textArea, juce::Justification::centredRight);
            } else {
                // Split label at space for LFO/Noise
                auto parts = juce::StringArray::fromTokens(label, " ", "");
                if (parts.size() >= 2) {
                    // Center vertically around the knob center
                    auto textHeight = g.getCurrentFont().getHeight() * 2.2f; // Height for 2 lines
                    auto centerY = knobBounds.getCentreY();
                    auto textArea = labelArea.withHeight(textHeight).withCentre({labelArea.getCentreX(), centerY});
                    auto topArea = textArea.removeFromTop(textArea.getHeight() / 2);
                    g.drawText(parts[0], topArea, juce::Justification::centredRight);
                    g.drawText(parts[1], textArea, juce::Justification::centredRight);
                } else {
                    g.drawText(label, labelArea, juce::Justification::centredRight);
                }
            }
        } else {
            // Regular label at bottom
            auto labelArea = bounds.removeFromBottom(16.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
            g.drawText(label, labelArea, juce::Justification::centred);
        }
    }
    
    // Draw value text
    juce::String valueText = juce::String(static_cast<int>(value));
    auto textBounds = knobBounds.reduced(knobSize * 0.3f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle(juce::Font::bold)));
    g.drawText(valueText, textBounds, juce::Justification::centred);
}

void RotaryKnob::resized()
{
    // Nothing specific needed for resize
}

void RotaryKnob::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isLeftButtonDown()) {
        isDragging = true;
        lastMousePos = event.getPosition();
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        
        if (onGestureStart) {
            onGestureStart();
        }
    }
}

void RotaryKnob::mouseDrag(const juce::MouseEvent& event)
{
    if (isDragging) {
        auto currentPos = event.getPosition();
        int deltaY = lastMousePos.y - currentPos.y; // Inverted for natural feel
        
        double sensitivity = 0.01;
        if (event.mods.isShiftDown()) {
            sensitivity *= 0.1; // Fine control with Shift
        }
        
        double normalizedChange = deltaY * sensitivity;
        double newNormalizedValue = juce::jlimit(0.0, 1.0, normalizedValue() + normalizedChange);
        
        setNormalizedValue(newNormalizedValue, juce::sendNotificationAsync);
        lastMousePos = currentPos;
    }
}

void RotaryKnob::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    isDragging = false;
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    
    if (onGestureEnd) {
        onGestureEnd();
    }
}

void RotaryKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused(event);
    
    double sensitivity = 0.1;
    if (event.mods.isShiftDown()) {
        sensitivity *= 0.1; // Fine control with Shift
    }
    
    double normalizedChange = wheel.deltaY * sensitivity;
    double newNormalizedValue = juce::jlimit(0.0, 1.0, normalizedValue() + normalizedChange);
    
    setNormalizedValue(newNormalizedValue, juce::sendNotificationAsync);
}

void RotaryKnob::setValue(double newValue, juce::NotificationType notification)
{
    double constrainedValue = constrainValue(newValue);
    if (value != constrainedValue) {
        value = constrainedValue;
        repaint();
        
        if (notification != juce::dontSendNotification && onValueChange) {
            onValueChange(value);
        }
    }
}

double RotaryKnob::getValue() const
{
    return value;
}

void RotaryKnob::setRange(double newMinValue, double newMaxValue, double newStepSize)
{
    minValue = newMinValue;
    maxValue = newMaxValue;
    stepSize = newStepSize;
    setValue(value, juce::dontSendNotification); // Re-constrain current value
}

void RotaryKnob::setLabel(const juce::String& labelText)
{
    label = labelText;
    repaint();
}

void RotaryKnob::setAccentColour(const juce::Colour& colour)
{
    accentColour = colour;
    repaint();
}

double RotaryKnob::normalizedValue() const
{
    if (maxValue <= minValue) return 0.0;
    return (value - minValue) / (maxValue - minValue);
}

void RotaryKnob::setNormalizedValue(double normalizedVal, juce::NotificationType notification)
{
    double newValue = minValue + normalizedVal * (maxValue - minValue);
    setValue(newValue, notification);
}

double RotaryKnob::constrainValue(double val) const
{
    // Snap to step size
    if (stepSize > 0.0) {
        val = std::round((val - minValue) / stepSize) * stepSize + minValue;
    }
    
    return juce::jlimit(minValue, maxValue, val);
}