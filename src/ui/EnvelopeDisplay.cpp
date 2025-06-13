#include "EnvelopeDisplay.h"
#include "../utils/Debug.h"

EnvelopeDisplay::EnvelopeDisplay()
{
    updateEnvelopePath();
}

EnvelopeDisplay::~EnvelopeDisplay() = default;

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Background
    g.setColour(juce::Colour(0xff1a202c));
    g.fillRoundedRectangle(bounds, 4.0f);
    
    // Border
    g.setColour(juce::Colour(0xff4a5568));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
    
    // Reduce bounds for content with more vertical space for labels
    auto contentBounds = bounds.reduced(8.0f, 20.0f);
    
    // Draw grid
    g.setColour(juce::Colour(0xff374151));
    
    // Horizontal grid lines
    for (int i = 1; i <= 3; ++i) {
        float y = contentBounds.getY() + (contentBounds.getHeight() * i / 4.0f);
        g.drawHorizontalLine(static_cast<int>(y), contentBounds.getX(), contentBounds.getRight());
    }
    
    // Vertical grid lines (phases)
    float totalWidth = contentBounds.getWidth();
    float attackWidth = totalWidth * 0.2f;
    float decay1Width = totalWidth * 0.25f;
    float decay2Width = totalWidth * 0.35f;
    // Release width = remaining
    
    float x = contentBounds.getX();
    x += attackWidth;
    g.drawVerticalLine(static_cast<int>(x), contentBounds.getY(), contentBounds.getBottom());
    x += decay1Width;
    g.drawVerticalLine(static_cast<int>(x), contentBounds.getY(), contentBounds.getBottom());
    x += decay2Width;
    g.drawVerticalLine(static_cast<int>(x), contentBounds.getY(), contentBounds.getBottom());
    
    // Draw envelope path
    if (!envelopePath.isEmpty()) {
        g.setColour(juce::Colour(0xff4ade80)); // Green
        g.strokePath(envelopePath, juce::PathStrokeType(2.0f));
        
        // Add glow effect
        g.setColour(juce::Colour(0xff4ade80).withAlpha(0.3f));
        g.strokePath(envelopePath, juce::PathStrokeType(4.0f));
    }
    
    // Draw phase labels
    g.setColour(juce::Colour(0xffa0aec0));
    g.setFont(juce::Font(9.0f));
    
    x = contentBounds.getX();
    g.drawText("A", juce::Rectangle<float>(x, contentBounds.getBottom() + 2, attackWidth, 12), juce::Justification::centred);
    x += attackWidth;
    g.drawText("D1", juce::Rectangle<float>(x, contentBounds.getBottom() + 2, decay1Width, 12), juce::Justification::centred);
    x += decay1Width;
    g.drawText("D2", juce::Rectangle<float>(x, contentBounds.getBottom() + 2, decay2Width, 12), juce::Justification::centred);
    x += decay2Width;
    float releaseWidth = contentBounds.getRight() - x;
    g.drawText("R", juce::Rectangle<float>(x, contentBounds.getBottom() + 2, releaseWidth, 12), juce::Justification::centred);
    
    // Draw key-off indicator
    float keyOffX = contentBounds.getX() + attackWidth + decay1Width + decay2Width;
    g.setColour(juce::Colour(0xfff56565)); // Red
    g.drawVerticalLine(static_cast<int>(keyOffX), contentBounds.getY(), contentBounds.getBottom());
    
    // Key-off label
    g.setColour(juce::Colour(0xfff56565));
    g.setFont(juce::Font(8.0f, juce::Font::bold));
    g.drawText("KEY OFF", juce::Rectangle<float>(keyOffX - 20, bounds.getY() + 5, 40, 12), juce::Justification::centred);
}

void EnvelopeDisplay::resized()
{
    updateEnvelopePath();
}

void EnvelopeDisplay::setEnvelopeParameters(float attack, float decay1, float decay1Level, float decay2, float release)
{
    attackRate = juce::jlimit(0.0f, 1.0f, attack);
    decay1Rate = juce::jlimit(0.0f, 1.0f, decay1);
    decay1Level = juce::jlimit(0.0f, 1.0f, decay1Level);
    decay2Rate = juce::jlimit(0.0f, 1.0f, decay2);
    releaseRate = juce::jlimit(0.0f, 1.0f, release);
    
    updateEnvelopePath();
    repaint();
}

void EnvelopeDisplay::setYM2151Parameters(int attackRate, int decay1Rate, int decay1Level, int decay2Rate, int releaseRate)
{
    // Convert YM2151 parameter ranges to normalized values
    float normalizedAttack = convertRateToNormalized(attackRate, 31);      // AR: 0-31
    float normalizedDecay1 = convertRateToNormalized(decay1Rate, 31);      // D1R: 0-31
    float normalizedDecay1Level = static_cast<float>(decay1Level) / 15.0f; // D1L: 0-15
    float normalizedDecay2 = convertRateToNormalized(decay2Rate, 31);      // D2R: 0-31
    float normalizedRelease = convertRateToNormalized(releaseRate, 15);    // RR: 0-15
    
    setEnvelopeParameters(normalizedAttack, normalizedDecay1, normalizedDecay1Level, 
                         normalizedDecay2, normalizedRelease);
}

void EnvelopeDisplay::updateEnvelopePath()
{
    auto bounds = getLocalBounds().reduced(8.0f, 20.0f).toFloat();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;
    
    envelopePath.clear();
    
    // Calculate time proportions for each phase
    float totalWidth = bounds.getWidth();
    float attackWidth = totalWidth * 0.2f;
    float decay1Width = totalWidth * 0.25f;
    float decay2Width = totalWidth * 0.35f;
    float releaseWidth = totalWidth * 0.2f;
    
    // Start at bottom left (silent)
    float x = bounds.getX();
    float y = bounds.getBottom();
    envelopePath.startNewSubPath(x, y);
    
    // Attack phase: rise to peak based on attack rate
    // Faster attack rate = steeper curve
    float attackTime = attackWidth * (1.0f - attackRate * 0.8f + 0.2f); // Range: 0.2-1.0 of attack width
    x += attackTime;
    y = bounds.getY(); // Peak level
    envelopePath.lineTo(x, y);
    
    // If attack didn't use full width, continue at peak level
    if (attackTime < attackWidth) {
        x = bounds.getX() + attackWidth;
        envelopePath.lineTo(x, y);
    }
    
    // Decay 1 phase: fall to sustain level
    x += decay1Width;
    y = bounds.getY() + (1.0f - decay1Level) * bounds.getHeight();
    
    // Create curved decay based on decay1 rate
    if (decay1Rate > 0.1f) {
        // Exponential-style decay
        float controlX = bounds.getX() + attackWidth + decay1Width * 0.3f;
        float controlY = bounds.getY() + (1.0f - decay1Level) * bounds.getHeight() * 0.3f;
        envelopePath.quadraticTo(controlX, controlY, x, y);
    } else {
        envelopePath.lineTo(x, y);
    }
    
    // Decay 2 phase: slowly decay to zero
    float decay2StartX = x;
    float decay2StartY = y;
    x += decay2Width;
    float decay2EndLevel = decay1Level * (1.0f - decay2Rate * 0.7f); // Gradual decay
    y = bounds.getY() + (1.0f - decay2EndLevel) * bounds.getHeight();
    
    if (decay2Rate > 0.1f) {
        // Slow exponential decay
        float controlX = decay2StartX + decay2Width * 0.7f;
        float controlY = decay2StartY + (y - decay2StartY) * 0.3f;
        envelopePath.quadraticTo(controlX, controlY, x, y);
    } else {
        envelopePath.lineTo(x, y);
    }
    
    // Release phase: fast decay to zero
    float releaseStartY = y;
    x += releaseWidth;
    y = bounds.getBottom();
    
    if (releaseRate > 0.1f) {
        // Fast exponential release
        float controlX = x - releaseWidth * 0.7f;
        float controlY = releaseStartY + (y - releaseStartY) * 0.2f;
        envelopePath.quadraticTo(controlX, controlY, x, y);
    } else {
        envelopePath.lineTo(x, y);
    }
}

float EnvelopeDisplay::convertRateToNormalized(int rate, int maxRate) const
{
    if (maxRate <= 0) return 0.0f;
    return static_cast<float>(rate) / static_cast<float>(maxRate);
}