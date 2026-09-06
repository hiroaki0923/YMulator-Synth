#include "EnvelopeDisplay.h"
#include "../utils/Debug.h"
#include "UiTheme.h"
#include <cmath>

namespace {
constexpr float kSilence = 127.0f;
constexpr float kLogUnitsAcross = 11.0f;   // log2 time units that span the box; slower envelopes are clipped
}

EnvelopeDisplay::EnvelopeDisplay()
{
    updateEnvelopePath();
}

EnvelopeDisplay::~EnvelopeDisplay() = default;

float EnvelopeDisplay::timeForRate(int rate)
{
    if (rate <= 0) return std::numeric_limits<float>::infinity();
    return std::pow(2.0f, (31.0f - static_cast<float>(juce::jmin(31, rate))) / 4.0f);
}

EnvelopeDisplay::Shape EnvelopeDisplay::computeShape(int totalLevel, int attackRate, int decay1Rate, int decay1Level, int decay2Rate, int releaseRate)
{
    Shape shape;
    const float tl = static_cast<float>(juce::jlimit(0, 127, totalLevel));
    if (attackRate <= 0 || tl >= kSilence) {
        shape.silent = true;
        shape.points = { { 0.0f, kSilence }, { kHoldUnits, kSilence } };
        shape.keyOffTime = kHoldUnits;
        return shape;
    }
    
    // Attack: the chip sweeps the whole range at the attack rate, then TL is added on top
    float t = timeForRate(attackRate);
    shape.points.push_back({ 0.0f, kSilence });
    shape.points.push_back({ t, tl });
    
    // Decay 1 down to the sustain attenuation TL + 4*D1L; D1L 15 is the chip's "all the way down"
    const float sustain = decay1Level >= 15 ? kSilence : juce::jmin(kSilence, tl + 4.0f * static_cast<float>(juce::jlimit(0, 15, decay1Level)));
    float level = tl;
    if (decay1Rate > 0 && sustain > tl) {
        t += (sustain - tl) / kSilence * timeForRate(decay1Rate);
        level = sustain;
        shape.points.push_back({ t, level });
    }
    
    // Held: decay 2 slopes on from the sustain level; with D1R 0 the envelope never reaches it and stays at the peak
    const float keyOff = t + kHoldUnits;
    if (level < kSilence && decay1Rate > 0 && decay2Rate > 0) {
        const float drop = kHoldUnits / timeForRate(decay2Rate) * kSilence;
        if (level + drop >= kSilence) {
            t += (kSilence - level) / kSilence * timeForRate(decay2Rate);
            level = kSilence;
            shape.points.push_back({ t, level });
        } else {
            level += drop;
        }
    }
    shape.points.push_back({ keyOff, level });
    shape.keyOffTime = keyOff;
    
    // Release from wherever the level was; RR 0..15 is the chip rate 2*RR+1
    if (level < kSilence) {
        const float releaseTime = (kSilence - level) / kSilence * timeForRate(2 * juce::jlimit(0, 15, releaseRate) + 1);
        shape.points.push_back({ keyOff + releaseTime, kSilence });
    }
    return shape;
}

void EnvelopeDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(UiTheme::dark);
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(UiTheme::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 3.0f, 1.0f);
    
    if (envelopePath.isEmpty()) return;
    
    juce::Path filled(envelopePath);
    filled.lineTo(envelopePath.getCurrentPosition().x, getLocalBounds().reduced(8, 8).toFloat().getBottom());
    filled.closeSubPath();
    g.setColour(lineColour.withAlpha(0.12f));
    g.fillPath(filled);
    g.setColour(lineColour);
    g.strokePath(envelopePath, juce::PathStrokeType(1.5f));
}

void EnvelopeDisplay::setLineColour(juce::Colour colour)
{
    lineColour = colour;
    repaint();
}

void EnvelopeDisplay::resized()
{
    updateEnvelopePath();
}

void EnvelopeDisplay::setYM2151Parameters(int newTotalLevel, int newAttackRate, int newDecay1Rate, int newDecay1Level, int newDecay2Rate, int newReleaseRate)
{
    totalLevel = newTotalLevel;
    attackRate = newAttackRate;
    decay1Rate = newDecay1Rate;
    decay1Level = newDecay1Level;
    decay2Rate = newDecay2Rate;
    releaseRate = newReleaseRate;
    updateEnvelopePath();
    repaint();
}

void EnvelopeDisplay::updateEnvelopePath()
{
    auto bounds = getLocalBounds().reduced(8.0f, 8.0f).toFloat();
    envelopePath.clear();
    if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0) return;
    
    const auto shape = computeShape(totalLevel, attackRate, decay1Rate, decay1Level, decay2Rate, releaseRate);
    // Time is drawn on a log scale so an instant attack and a two-second release both stay readable
    const float unit = bounds.getWidth() / kLogUnitsAcross;
    auto x = [&](float time) { return bounds.getX() + juce::jmin(bounds.getWidth(), std::log2(1.0f + time) * unit); };
    auto y = [&](float attenuation) { return bounds.getY() + attenuation / kSilence * bounds.getHeight(); };
    
    bool first = true;
    for (const auto& p : shape.points) {
        if (first) { envelopePath.startNewSubPath(x(p.time), y(p.attenuation)); first = false; }
        else envelopePath.lineTo(x(p.time), y(p.attenuation));
    }
}
