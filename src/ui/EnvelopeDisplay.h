#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class EnvelopeDisplay : public juce::Component
{
public:
    EnvelopeDisplay();
    ~EnvelopeDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // Set envelope parameters (normalized 0.0-1.0)
    void setEnvelopeParameters(float attack, float decay1, float decay1Level, float decay2, float release);
    
    // Set parameters with YM2151 ranges
    void setYM2151Parameters(int totalLevel, int attackRate, int decay1Rate, int decay1Level, int decay2Rate, int releaseRate);

private:
    // Normalized envelope parameters (0.0-1.0)
    float totalLevel = 1.0f;    // Peak level (inverted from TL)
    float attackRate = 0.9f;    // Fast attack by default
    float decay1Rate = 0.3f;
    float decay1Level = 0.7f;
    float decay2Rate = 0.5f;
    float releaseRate = 0.4f;
    
    // Fixed phase widths based on ADSR timing
    static constexpr float ATTACK_WIDTH = 0.15f;   // Short attack phase
    static constexpr float DECAY1_WIDTH = 0.25f;   // Decay to sustain level
    static constexpr float DECAY2_WIDTH = 0.45f;   // Sustain decay (longest phase)
    static constexpr float RELEASE_WIDTH = 0.15f;  // Short release phase
    
    juce::Path envelopePath;
    void updateEnvelopePath();
    
    // Convert YM2151 rates to normalized display values
    float convertRateToNormalized(int rate, int maxRate) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeDisplay)
};