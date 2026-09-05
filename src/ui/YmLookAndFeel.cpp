#include "YmLookAndFeel.h"
#include "UiTheme.h"

YmLookAndFeel::YmLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, UiTheme::panel);
    setColour(juce::ComboBox::outlineColourId, UiTheme::border);
    setColour(juce::ComboBox::textColourId, UiTheme::text);
    setColour(juce::ComboBox::arrowColourId, UiTheme::muted);
    setColour(juce::ComboBox::focusedOutlineColourId, UiTheme::green);
    setColour(juce::PopupMenu::backgroundColourId, UiTheme::panel);
    setColour(juce::PopupMenu::textColourId, UiTheme::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, UiTheme::border);
    setColour(juce::PopupMenu::highlightedTextColourId, UiTheme::text);
    setColour(juce::TextButton::buttonColourId, UiTheme::panel);
    setColour(juce::TextButton::buttonOnColourId, UiTheme::borderSoft);
    setColour(juce::TextButton::textColourOffId, UiTheme::text);
    setColour(juce::TextButton::textColourOnId, UiTheme::text);
    setColour(juce::TextButton::textColourOffId, UiTheme::text);
    setColour(juce::Label::textColourId, UiTheme::text);
    setColour(juce::TextEditor::backgroundColourId, UiTheme::dark);
    setColour(juce::TextEditor::textColourId, UiTheme::text);
    setColour(juce::TextEditor::outlineColourId, UiTheme::border);
    setColour(juce::AlertWindow::backgroundColourId, UiTheme::panel);
    setColour(juce::AlertWindow::textColourId, UiTheme::text);
    setColour(juce::AlertWindow::outlineColourId, UiTheme::border);
    setColour(juce::TooltipWindow::backgroundColourId, UiTheme::panel);
    setColour(juce::TooltipWindow::textColourId, UiTheme::text);
}

void YmLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                     bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsDown);
    const bool on = button.getToggleState();
    const float pillW = 30.0f, pillH = 16.0f;
    auto bounds = button.getLocalBounds().toFloat();
    auto pill = juce::Rectangle<float>(pillW, pillH).withCentre({ bounds.getX() + pillW * 0.5f, bounds.getCentreY() });
    
    auto pillColour = on ? UiTheme::green : UiTheme::border;
    if (shouldDrawButtonAsHighlighted) pillColour = pillColour.brighter(0.15f);
    g.setColour(pillColour);
    g.fillRoundedRectangle(pill, pillH * 0.5f);
    
    const float dot = pillH - 4.0f;
    const auto dotBounds = juce::Rectangle<float>(dot, dot)
        .withCentre({ on ? pill.getRight() - 2.0f - dot * 0.5f : pill.getX() + 2.0f + dot * 0.5f, pill.getCentreY() });
    g.setColour(on ? UiTheme::dark : UiTheme::muted);
    g.fillEllipse(dotBounds);
    
    if (button.getButtonText().isNotEmpty()) {
        g.setColour(button.isEnabled() ? UiTheme::muted : UiTheme::dim);
        g.setFont(UiTheme::mono(10.0f));
        g.drawText(button.getButtonText(), bounds.withTrimmedLeft(pillW + 6.0f), juce::Justification::centredLeft);
    }
}

void YmLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                 int, int, int, int, juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown);
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds.reduced(0.5f), 4.0f);
    g.setColour(box.findColour(box.hasKeyboardFocus(true) ? juce::ComboBox::focusedOutlineColourId
                                                          : juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    
    juce::Path arrow;
    const float cx = bounds.getRight() - 12.0f, cy = bounds.getCentreY();
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(box.findColour(juce::ComboBox::arrowColourId).withAlpha(box.isEnabled() ? 1.0f : 0.4f));
    g.fillPath(arrow);
}

void YmLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(8, 1, box.getWidth() - 28, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
}

juce::Font YmLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return UiTheme::sans(box.getHeight() <= 24 ? 11.0f : 13.0f);
}

void YmLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    if (button.getProperties()["chip"]) {
        // Pill-shaped category chip: green when selected
        const bool on = button.getToggleState();
        g.setColour(on ? UiTheme::green : UiTheme::panel);
        g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);
        g.setColour(on ? UiTheme::green : UiTheme::border);
        g.drawRoundedRectangle(bounds, bounds.getHeight() * 0.5f, 1.0f);
        return;
    }
    if (button.getProperties()["accent"]) {
        g.setColour(shouldDrawButtonAsDown ? UiTheme::green.darker(0.2f) : (shouldDrawButtonAsHighlighted ? UiTheme::green.brighter(0.1f) : UiTheme::green));
        g.fillRoundedRectangle(bounds, 4.0f);
        return;
    }
    auto colour = backgroundColour;
    if (shouldDrawButtonAsDown) colour = colour.brighter(0.2f);
    else if (shouldDrawButtonAsHighlighted) colour = colour.brighter(0.08f);
    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(button.getToggleState() ? UiTheme::green : UiTheme::border);
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

juce::Font YmLookAndFeel::getTextButtonFont(juce::TextButton& button, int)
{
    return UiTheme::sans(12.0f, !button.getProperties()["chip"]);
}

void YmLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                     float, float, juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal) {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0.0f, 0.0f, style, slider);
        return;
    }
    const float centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
    auto track = juce::Rectangle<float>(static_cast<float>(x), centreY - 3.0f, static_cast<float>(width), 6.0f);
    g.setColour(UiTheme::borderSoft);
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(UiTheme::border);
    g.drawRoundedRectangle(track, 3.0f, 1.0f);
    g.setColour(slider.isEnabled() ? UiTheme::green : UiTheme::dim);
    g.fillRoundedRectangle(track.withRight(sliderPos), 3.0f);
    const auto thumb = juce::Rectangle<float>(16.0f, 16.0f).withCentre({ sliderPos, centreY });
    g.setColour(UiTheme::background);
    g.fillEllipse(thumb.expanded(2.0f));
    g.setColour(UiTheme::text);
    g.fillEllipse(thumb);
}
