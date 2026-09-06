#include "OutputScope.h"
#include "UiTheme.h"
#include <cmath>

namespace {
constexpr float kSilence = 1.0e-4f;
}

OutputScope::OutputScope()
{
    setInterceptsMouseClicks(false, false);
}

void OutputScope::setWaveform(const std::vector<float>& samples, double periodInSamples)
{
    frame.clear();
    peak = 0.0f;
    if (samples.empty()) { repaint(); return; }
    
    size_t loudest = 0;
    for (size_t i = 0; i < samples.size(); ++i)
        if (std::abs(samples[i]) > peak) { peak = std::abs(samples[i]); loudest = i; }
    
    const size_t window = static_cast<size_t>(juce::jlimit(8, static_cast<int>(samples.size()), juce::roundToInt(periodInSamples * kPeriods)));
    // Start at the last rising zero crossing before the loudest sample, keeping the window inside the render
    size_t start = juce::jmin(loudest, samples.size() - window);
    for (size_t i = start; i > 0; --i)
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f) { start = i; break; }
    start = juce::jmin(start, samples.size() - window);
    frame.assign(samples.begin() + static_cast<long>(start), samples.begin() + static_cast<long>(start + window));
    repaint();
}

void OutputScope::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(UiTheme::dark);
    g.fillRoundedRectangle(bounds, 3.0f);
    
    auto plot = bounds.reduced(2.0f, 4.0f).withTrimmedBottom(5.0f);
    g.setColour(UiTheme::border);
    g.drawHorizontalLine(static_cast<int>(plot.getCentreY()), plot.getX(), plot.getRight());
    
    if (frame.size() > 1) {
        // The trace is normalised to its own peak so quiet sounds stay readable; the bar shows the true level
        juce::Path path;
        const float midY = plot.getCentreY();
        const float scale = plot.getHeight() * 0.48f / juce::jmax(peak, 0.05f);
        const float step = plot.getWidth() / static_cast<float>(frame.size() - 1);
        for (size_t i = 0; i < frame.size(); ++i) {
            const float x = plot.getX() + step * static_cast<float>(i);
            const float y = midY - juce::jlimit(-1.0f, 1.0f, frame[i]) * scale;
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(peak > kSilence ? UiTheme::green : UiTheme::dim);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    
    auto bar = bounds.reduced(2.0f, 0.0f).removeFromBottom(4.0f).translated(0.0f, -2.0f);
    g.setColour(UiTheme::borderSoft);
    g.fillRoundedRectangle(bar, 2.0f);
    const float level = juce::jlimit(0.0f, 1.0f, peak);
    g.setColour(level > 0.95f ? UiTheme::amber : UiTheme::green);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * level), 2.0f);
}
