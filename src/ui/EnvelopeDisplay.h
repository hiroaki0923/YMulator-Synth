#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

/**
 * Draws an operator's envelope the way the YM2151 runs it: attenuation in TL units
 * (0 loud, 127 silent), the peak at TL, decay 1 down to TL + 4*D1L, decay 2 sloping
 * on from there while the key is held, and the release from wherever the level was.
 * Rates map to times as on the chip, doubling every four steps.
 */
class EnvelopeDisplay : public juce::Component
{
public:
    EnvelopeDisplay();
    ~EnvelopeDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void setYM2151Parameters(int totalLevel, int attackRate, int decay1Rate, int decay1Level, int decay2Rate, int releaseRate);
    void setLineColour(juce::Colour colour);
    
    /** One corner of the envelope: time in chip-ish units from the key-on, attenuation in TL units (0..127). */
    struct Point { float time; float attenuation; };
    struct Shape {
        std::vector<Point> points;   // key-on at time 0; the last point is silence after the release
        float keyOffTime = 0.0f;
        bool silent = false;         // AR 0 or TL 127: the operator never sounds
    };
    static constexpr float kHoldUnits = 12.0f;     // how long the key is shown held
    
    /** Time a full-range sweep takes at `rate` (1..31); rate 0 never moves. Doubles every four steps like the chip. */
    static float timeForRate(int rate);
    static Shape computeShape(int totalLevel, int attackRate, int decay1Rate, int decay1Level, int decay2Rate, int releaseRate);

private:
    int totalLevel = 0, attackRate = 31, decay1Rate = 0, decay1Level = 0, decay2Rate = 0, releaseRate = 7;
    juce::Path envelopePath;
    juce::Colour lineColour { 0xff52e3a1 };
    void updateEnvelopePath();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EnvelopeDisplay)
};
