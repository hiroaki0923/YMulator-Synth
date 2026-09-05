#include "RotaryKnob.h"
#include "UiTheme.h"

namespace {
constexpr float kRingPadding = 3.0f;      // room for the highlight ring
constexpr int kLabelHeight = 13;
constexpr int kSubLabelHeight = 11;
}

RotaryKnob::RotaryKnob(const juce::String& labelText, Style knobStyle)
    : label(labelText), style(knobStyle)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

RotaryKnob::~RotaryKnob() = default;

int RotaryKnob::dialDiameter(Style s)
{
    switch (s) {
        case Style::Primary: return 48;
        case Style::Small:   return 36;
        default:             return 30;
    }
}

juce::Rectangle<int> RotaryKnob::preferredSize(Style s, LabelPosition position, bool hasSubLabel, int labelWidth)
{
    const int d = dialDiameter(s) + static_cast<int>(kRingPadding * 2.0f);
    switch (position) {
        case LabelPosition::Below:
            return { 0, 0, juce::jmax(d, labelWidth), d + kLabelHeight + (hasSubLabel ? kSubLabelHeight : 0) };
        case LabelPosition::Right:
            return { 0, 0, d + 6 + labelWidth, d };
        default:
            return { 0, 0, d, d };
    }
}

juce::Rectangle<float> RotaryKnob::dialBounds() const
{
    const float d = static_cast<float>(dialDiameter(style));
    auto bounds = getLocalBounds().toFloat();
    switch (labelPosition) {
        case LabelPosition::Below:
            return juce::Rectangle<float>(d, d).withCentre({ bounds.getCentreX(), kRingPadding + d * 0.5f });
        case LabelPosition::Right:
            return juce::Rectangle<float>(d, d).withCentre({ kRingPadding + d * 0.5f, bounds.getCentreY() });
        default:
            return juce::Rectangle<float>(d, d).withCentre(bounds.getCentre());
    }
}

void RotaryKnob::paint(juce::Graphics& g)
{
    const auto dial = dialBounds();
    const float d = dial.getWidth();
    const auto centre = dial.getCentre();
    const float ringRadius = d * 0.5f - 2.0f;
    const double displayNorm = displayNormalized();
    const double endAngle = startAngle + displayNorm * rotationRange;
    
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                        static_cast<float>(startAngle), static_cast<float>(startAngle + rotationRange), true);
    g.setColour(UiTheme::border);
    g.strokePath(track, juce::PathStrokeType(3.0f));
    
    if (displayNorm > 0.0) {
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, ringRadius, ringRadius, 0.0f,
                          static_cast<float>(startAngle), static_cast<float>(endAngle), true);
        g.setColour(isEnabled() ? accentColour : UiTheme::dim);
        g.strokePath(arc, juce::PathStrokeType(3.0f));
    }
    
    const auto disc = dial.reduced(d * 0.14f);
    g.setColour(UiTheme::panel);
    g.fillEllipse(disc);
    g.setColour(UiTheme::border);
    g.drawEllipse(disc, 1.0f);
    
    if (highlighted) {
        g.setColour(UiTheme::amber);
        g.drawEllipse(dial.expanded(1.5f), 2.0f);
    }
    
    const float valueFontSize = style == Style::Primary ? 12.0f : (style == Style::Small ? 11.0f : 9.0f);
    g.setColour(UiTheme::text);
    g.setFont(UiTheme::mono(valueFontSize, true));
    const juce::String shown = valueFormatter ? valueFormatter(value) : juce::String(juce::roundToInt(value));
    g.drawFittedText(shown, disc.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    
    if (label.isEmpty() || labelPosition == LabelPosition::None) return;
    
    g.setFont(UiTheme::mono(10.0f));
    g.setColour(highlighted ? UiTheme::amber : UiTheme::muted);
    if (labelPosition == LabelPosition::Below) {
        auto labelArea = juce::Rectangle<float>(0.0f, dial.getBottom() + kRingPadding, static_cast<float>(getWidth()), static_cast<float>(kLabelHeight));
        g.drawText(label, labelArea, juce::Justification::centred);
        if (subLabel.isNotEmpty()) {
            g.setColour(UiTheme::dim);
            g.setFont(UiTheme::mono(9.0f));
            g.drawText(subLabel, labelArea.translated(0.0f, static_cast<float>(kLabelHeight) - 1.0f).withHeight(static_cast<float>(kSubLabelHeight)),
                       juce::Justification::centred);
        }
    } else {
        auto labelArea = getLocalBounds().toFloat().withTrimmedLeft(dial.getRight() + 6.0f);
        g.drawText(label, labelArea, juce::Justification::centredLeft);
    }
}

void RotaryKnob::mouseDown(const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown()) return;
    isDragging = true;
    lastMousePos = event.getPosition();
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    if (onGestureStart) onGestureStart();
}

void RotaryKnob::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDragging) return;
    const auto currentPos = event.getPosition();
    const int deltaY = lastMousePos.y - currentPos.y;
    double sensitivity = 0.01;
    if (event.mods.isShiftDown()) sensitivity *= 0.1;
    setDisplayNormalized(juce::jlimit(0.0, 1.0, displayNormalized() + deltaY * sensitivity), juce::sendNotificationAsync);
    lastMousePos = currentPos;
}

void RotaryKnob::mouseUp(const juce::MouseEvent&)
{
    if (!isDragging) return;
    isDragging = false;
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    if (onGestureEnd) onGestureEnd();
}

void RotaryKnob::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    double sensitivity = 0.1;
    if (event.mods.isShiftDown()) sensitivity *= 0.1;
    setDisplayNormalized(juce::jlimit(0.0, 1.0, displayNormalized() + wheel.deltaY * sensitivity), juce::sendNotificationAsync);
}

void RotaryKnob::mouseEnter(const juce::MouseEvent&)
{
    if (onHoverChanged) onHoverChanged(true);
}

void RotaryKnob::mouseExit(const juce::MouseEvent&)
{
    if (onHoverChanged) onHoverChanged(false);
}

void RotaryKnob::setValue(double newValue, juce::NotificationType notification)
{
    const double constrained = constrainValue(newValue);
    if (value == constrained) return;
    value = constrained;
    repaint();
    if (notification != juce::dontSendNotification && onValueChange) onValueChange(value);
}

void RotaryKnob::setRange(double newMin, double newMax, double newStep)
{
    minValue = newMin;
    maxValue = newMax;
    stepSize = newStep;
    setValue(value, juce::dontSendNotification);
}

void RotaryKnob::setLabel(const juce::String& labelText) { label = labelText; repaint(); }
void RotaryKnob::setSubLabel(const juce::String& text) { subLabel = text; repaint(); }
void RotaryKnob::setStyle(Style newStyle) { style = newStyle; repaint(); }
void RotaryKnob::setLabelPosition(LabelPosition position) { labelPosition = position; repaint(); }
void RotaryKnob::setAccentColour(const juce::Colour& colour) { accentColour = colour; repaint(); }
void RotaryKnob::setValueFormatter(std::function<juce::String(double)> formatter) { valueFormatter = std::move(formatter); repaint(); }
void RotaryKnob::setInverted(bool shouldInvert) { inverted = shouldInvert; repaint(); }

void RotaryKnob::setHighlighted(bool shouldHighlight)
{
    if (highlighted == shouldHighlight) return;
    highlighted = shouldHighlight;
    repaint();
}

double RotaryKnob::normalizedValue() const
{
    if (maxValue <= minValue) return 0.0;
    return (value - minValue) / (maxValue - minValue);
}

double RotaryKnob::displayNormalized() const
{
    return inverted ? 1.0 - normalizedValue() : normalizedValue();
}

void RotaryKnob::setDisplayNormalized(double displayValue, juce::NotificationType notification)
{
    const double raw = inverted ? 1.0 - displayValue : displayValue;
    setValue(minValue + raw * (maxValue - minValue), notification);
}

double RotaryKnob::constrainValue(double val) const
{
    if (stepSize > 0.0) val = std::round((val - minValue) / stepSize) * stepSize + minValue;
    return juce::jlimit(minValue, maxValue, val);
}
