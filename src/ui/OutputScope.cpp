#include "OutputScope.h"
#include "UiTheme.h"
#include <cmath>

namespace {
constexpr int kSearch = 1024;          // samples scanned for a trigger point before the window
constexpr int kMinPeriod = 24;         // narrowest window, about 2 kHz at 48 kHz
constexpr float kSilence = 1.0e-4f;
}

OutputScope::OutputScope(const ymulatorsynth::ScopeBuffer& buffer, std::function<double()> period)
    : scope(buffer), periodInSamples(std::move(period)), frame(static_cast<size_t>(kMaxWindow), 0.0f), latest(static_cast<size_t>(kCapture), 0.0f)
{
    setInterceptsMouseClicks(false, false);
}

OutputScope::~OutputScope()
{
    stopTimer();
}

void OutputScope::visibilityChanged()
{
    if (isVisible()) startTimerHz(30); else stopTimer();
}

void OutputScope::timerCallback()
{
    const size_t written = scope.totalWritten();
    if (written == lastSeen) return;
    lastSeen = written;
    
    scope.readLatest(latest.data(), latest.size());
    
    const int period = periodInSamples ? juce::roundToInt(periodInSamples()) : 0;
    int window = period > 0 ? period * kPeriods : kMaxWindow;
    if (window > kMaxWindow && period > 0) window = period * 2;
    window = juce::jlimit(kMinPeriod, kMaxWindow, window);
    
    // Trigger: the last rising zero crossing that still leaves a full window after it
    const size_t limit = latest.size() - static_cast<size_t>(window);
    size_t start = limit;
    for (size_t i = limit; i > 1; --i) {
        if (latest[i - 1] < 0.0f && latest[i] >= 0.0f) { start = i; break; }
    }
    frame.assign(latest.begin() + static_cast<long>(start), latest.begin() + static_cast<long>(start) + window);
    float p = 0.0f;
    for (float v : frame) p = std::max(p, std::abs(v));
    peak = p;
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
    
    // Level bar under the trace
    auto bar = bounds.reduced(2.0f, 0.0f).removeFromBottom(4.0f).translated(0.0f, -2.0f);
    g.setColour(UiTheme::borderSoft);
    g.fillRoundedRectangle(bar, 2.0f);
    const float level = juce::jlimit(0.0f, 1.0f, peak);
    g.setColour(level > 0.95f ? UiTheme::amber : UiTheme::green);
    g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * level), 2.0f);
}
