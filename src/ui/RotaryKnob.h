#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

/**
 * Rotary control drawn in three sizes. The value shown can be reformatted for
 * people (level 0-100, ratio x2, detune +3) while the control keeps operating
 * on the raw register range; a sub-label carries the raw value.
 */
class RotaryKnob : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    enum class Style { Large, Primary, Small, Tiny };   // 84 / 48 / 36 / 30 px dial
    enum class LabelPosition { Below, Right, None };
    
    explicit RotaryKnob(const juce::String& labelText = "", Style style = Style::Small);
    ~RotaryKnob() override;
    
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    
    void setValue(double newValue, juce::NotificationType notification = juce::sendNotificationAsync);
    double getValue() const { return value; }
    void setRange(double minValue, double maxValue, double stepSize = 1.0);
    
    void setLabel(const juce::String& labelText);
    void setSubLabel(const juce::String& text);
    void setStyle(Style newStyle);
    void setLabelPosition(LabelPosition position);
    void setAccentColour(const juce::Colour& colour);
    void setValueFormatter(std::function<juce::String(double)> formatter);
    /** Arc grows as the raw value falls (TL: 0 is loudest). */
    void setInverted(bool shouldInvert);
    void setHighlighted(bool shouldHighlight);
    bool isHighlighted() const { return highlighted; }
    
    /** Double-click: a box over the dial takes the raw value typed in; Return applies it, Escape drops it. */
    void beginTextEntry();
    bool isTextEntryOpen() const { return valueEditor != nullptr; }
    /** What the text box would apply: the raw value, snapped to the range. Empty or non-numeric text changes nothing. */
    void applyTypedValue(const juce::String& text);
    
    static int dialDiameter(Style style);
    /** Size that fits the dial, label and optional sub-label. */
    static juce::Rectangle<int> preferredSize(Style style, LabelPosition position, bool hasSubLabel, int labelWidth = 0);
    
    std::function<void(double)> onValueChange;
    std::function<void()> onGestureStart;
    std::function<void()> onGestureEnd;
    std::function<void(bool)> onHoverChanged;
    
private:
    double value = 0.0, minValue = 0.0, maxValue = 1.0, stepSize = 1.0;
    juce::String label, subLabel;
    Style style = Style::Small;
    LabelPosition labelPosition = LabelPosition::Below;
    juce::Colour accentColour { 0xff52e3a1 };
    std::function<juce::String(double)> valueFormatter;
    bool inverted = false;
    bool highlighted = false;
    bool isDragging = false;
    juce::Point<int> lastMousePos;
    std::unique_ptr<juce::TextEditor> valueEditor;
    void closeTextEntry();
    
    static constexpr double rotationRange = juce::MathConstants<double>::pi * 1.5;
    static constexpr double startAngle = juce::MathConstants<double>::pi * 1.25;
    
    juce::Rectangle<float> dialBounds() const;
    double normalizedValue() const;
    double displayNormalized() const;
    void setDisplayNormalized(double displayValue, juce::NotificationType notification);
    double constrainValue(double val) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryKnob)
};
