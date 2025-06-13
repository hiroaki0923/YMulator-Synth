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
    
    // Use full area for envelope display
    auto contentBounds = bounds.reduced(8.0f, 8.0f);
    
    
    // Draw envelope path
    if (!envelopePath.isEmpty()) {
        g.setColour(juce::Colour(0xff4ade80)); // Green
        g.strokePath(envelopePath, juce::PathStrokeType(2.0f));
        
        // Add glow effect
        g.setColour(juce::Colour(0xff4ade80).withAlpha(0.3f));
        g.strokePath(envelopePath, juce::PathStrokeType(4.0f));
    }
    
    
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

void EnvelopeDisplay::setYM2151Parameters(int totalLevel, int attackRate, int decay1Rate, int decay1Level, int decay2Rate, int releaseRate)
{
    // Store YM2151 parameter values directly for correct envelope behavior
    this->totalLevel = 1.0f - (static_cast<float>(totalLevel) / 127.0f);   // TL: 0-127 (inverted: 0=loudest, 127=quietest)
    
    // YM2151 rates: higher values = faster rates (31 = instant, 0 = slowest)
    this->attackRate = static_cast<float>(attackRate) / 31.0f;              // AR: 0-31
    this->decay1Rate = static_cast<float>(decay1Rate) / 31.0f;              // D1R: 0-31  
    this->decay1Level = static_cast<float>(decay1Level) / 15.0f;            // D1L: 0-15 (sustain level)
    this->decay2Rate = static_cast<float>(decay2Rate) / 31.0f;              // D2R: 0-31
    this->releaseRate = static_cast<float>(releaseRate) / 15.0f;            // RR: 0-15
    
    updateEnvelopePath();
    repaint();
}

void EnvelopeDisplay::updateEnvelopePath()
{
    // Use full width of the available space
    auto bounds = getLocalBounds().reduced(8.0f, 8.0f).toFloat();
    
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;
    
    envelopePath.clear();
    
    // Convert normalized parameters back to YM2151 ranges
    int TL = static_cast<int>((1.0f - totalLevel) * 127.0f);      // 0-127
    int AR = static_cast<int>(attackRate * 31.0f);                // 0-31
    int D1R = static_cast<int>(decay1Rate * 31.0f);               // 0-31  
    int D2R = static_cast<int>(decay2Rate * 31.0f);               // 0-31
    int D1L = static_cast<int>(decay1Level * 15.0f);              // 0-15
    int RR = static_cast<int>(releaseRate * 15.0f);               // 0-15
    
    // Special case: AR==0 or TL==127 (silent) - matches real hardware behavior
    if (AR == 0 || TL == 127) {
        // Draw flat line at bottom (no sound)
        envelopePath.startNewSubPath(bounds.getX(), bounds.getBottom());
        envelopePath.lineTo(bounds.getRight(), bounds.getBottom());
        return;
    }
    
    // Calculate Y positions (volume levels)
    float silentY = bounds.getBottom();
    float tlY = bounds.getY() + (static_cast<float>(TL) / 127.0f) * bounds.getHeight();
    float d1lY = bounds.getY() + ((D1L * 4 > TL) ? (static_cast<float>(D1L * 4) / 127.0f) : (static_cast<float>(TL) / 127.0f)) * bounds.getHeight();
    
    // Define time scale - pixels per time unit
    float timeScale = bounds.getWidth() / 40.0f; // Adjust this to control overall envelope width
    
    // Calculate X positions based on rate-to-time conversion
    // Rate controls speed (slope), not relative width
    float currentX = 0.0f;
    
    // Attack phase: time = level_change / mapped_rate
    // Map AR (1-31) to effective rate (1-3) for compressed slope range
    float attackLevelChange = 127.0f - static_cast<float>(TL);
    float mappedAR = 1.0f + (static_cast<float>(AR - 1) / 30.0f) * 2.0f; // Maps 1-31 to 1-3
    float attackTime = attackLevelChange / (mappedAR * 10.0f); // Adjusted scaling factor
    float attackX = currentX + attackTime * timeScale;
    
    // Decay 1 phase (if needed)
    float decay1X = attackX;
    if ((D1L * 4) > TL && D1R > 0) {
        float decay1LevelChange = static_cast<float>(D1L * 4 - TL);
        float mappedD1R = 1.0f + (static_cast<float>(D1R - 1) / 30.0f) * 2.0f; // Maps 1-31 to 1-3
        float decay1Time = decay1LevelChange / (mappedD1R * 10.0f); // Adjusted scaling factor
        decay1X = attackX + decay1Time * timeScale;
    }
    
    // Decay 2 phase - fixed duration
    float decay2Time = 16.0f; // Doubled sustain duration
    float decay2X = decay1X + decay2Time * timeScale;
    
    // Calculate D2 end level
    float decay2EndY = d1lY;
    if (D2R > 0) {
        // D2R affects the slope during sustain
        float mappedD2R = 1.0f + (static_cast<float>(D2R - 1) / 30.0f) * 2.0f; // Maps 1-31 to 1-3
        float d2Drop = (silentY - d1lY) * (mappedD2R / 3.0f) * 0.3f; // Gradual drop
        decay2EndY = d1lY + d2Drop;
    }
    
    // Release phase
    float releaseLevelChange = 127.0f - (decay2EndY / bounds.getHeight() * 127.0f);
    // Note: RR range is 1-15, not 1-31 like AR/D1R/D2R, so we need to adjust mapping
    float mappedRR = (RR > 0) ? (1.0f + (static_cast<float>(RR - 1) / 14.0f) * 2.0f) : 1.0f; // Maps 1-15 to 1-3
    float releaseTime = releaseLevelChange / (mappedRR * 10.0f); // Same scaling as AR/D1R
    float releaseX = decay2X + releaseTime * timeScale;
    
    // Draw envelope path (clip at bounds, don't scale)
    envelopePath.startNewSubPath(bounds.getX(), silentY); // Start at silence
    
    // Attack phase - always draw with proper slope
    float drawAttackX = juce::jmin(attackX, bounds.getWidth());
    envelopePath.lineTo(bounds.getX() + drawAttackX, tlY);
    
    // Decay 1 phase (if different from attack end and within bounds)
    if (decay1X > attackX && attackX < bounds.getWidth()) {
        float drawDecay1X = juce::jmin(decay1X, bounds.getWidth());
        envelopePath.lineTo(bounds.getX() + drawDecay1X, d1lY);
    }
    
    // Decay 2 phase (sustain) - only if previous phases fit
    if (decay1X < bounds.getWidth()) {
        float drawDecay2X = juce::jmin(decay2X, bounds.getWidth());
        envelopePath.lineTo(bounds.getX() + drawDecay2X, decay2EndY);
    }
    
    // Release phase - only if previous phases fit
    if (decay2X < bounds.getWidth()) {
        float drawReleaseX = juce::jmin(releaseX, bounds.getWidth());
        envelopePath.lineTo(bounds.getX() + drawReleaseX, silentY);
    }
}

float EnvelopeDisplay::convertRateToNormalized(int rate, int maxRate) const
{
    if (maxRate <= 0) return 0.0f;
    return static_cast<float>(rate) / static_cast<float>(maxRate);
}