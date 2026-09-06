#include "OutputScope.h"
#include "UiTheme.h"
#include <cmath>

namespace {
constexpr float kSilence = 1.0e-4f;
constexpr int kFrameRate = 30;
constexpr int kEnvelopeHeight = 12;
}

OutputScope::OutputScope()
{
    setInterceptsMouseClicks(false, false);
}

OutputScope::~OutputScope()
{
    stopTimer();
}

void OutputScope::setWaveform(const std::vector<float>& newSamples, double periodInSamples, double sampleRate, size_t noteOffSample)
{
    samples = newSamples;
    period = juce::jmax(1.0, periodInSamples);
    rate = juce::jmax(1.0, sampleRate);
    noteOff = juce::jmin(noteOffSample, samples.size());
    peak = 0.0f;
    for (float v : samples) peak = juce::jmax(peak, std::abs(v));
    
    envelopeBins.assign(static_cast<size_t>(kEnvelopeBins), 0.0f);
    if (!samples.empty()) {
        const double perBin = static_cast<double>(samples.size()) / kEnvelopeBins;
        for (size_t i = 0; i < samples.size(); ++i) {
            auto& bin = envelopeBins[juce::jmin(static_cast<size_t>(kEnvelopeBins - 1), static_cast<size_t>(i / perBin))];
            bin = juce::jmax(bin, std::abs(samples[i]));
        }
    }
    
    loopStartMs = juce::Time::getMillisecondCounterHiRes();
    setPlayhead(0.0);
    updateTimer();
}

void OutputScope::setPlayhead(double seconds)
{
    playhead = juce::jmax(0.0, seconds);
    selectFrame();
    repaint();
}

void OutputScope::selectFrame()
{
    frame.clear();
    if (samples.empty()) return;
    const size_t window = static_cast<size_t>(juce::jlimit(8, static_cast<int>(samples.size()), juce::roundToInt(period * kPeriods)));
    const size_t position = juce::jmin(static_cast<size_t>(playhead * rate), samples.size() - window);
    // Trigger on the first rising zero crossing within one period of the playhead, so the trace stands still
    size_t start = position;
    const size_t limit = juce::jmin(samples.size() - window, position + static_cast<size_t>(period) + 1);
    for (size_t i = position + 1; i <= limit; ++i)
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f) { start = i; break; }
    frame.assign(samples.begin() + static_cast<long>(start), samples.begin() + static_cast<long>(start + window));
}

void OutputScope::visibilityChanged()      { updateTimer(); }
void OutputScope::parentHierarchyChanged() { updateTimer(); }

void OutputScope::updateTimer()
{
    const bool wantsTimer = !samples.empty() && isShowing();
    if (wantsTimer && !isTimerRunning()) { loopStartMs = juce::Time::getMillisecondCounterHiRes() - playhead * 1000.0; startTimerHz(kFrameRate); }
    else if (!wantsTimer && isTimerRunning()) stopTimer();
}

void OutputScope::timerCallback()
{
    if (samples.empty()) { stopTimer(); return; }
    const double renderSeconds = static_cast<double>(samples.size()) / rate;
    const double loopSeconds = renderSeconds + kLoopPauseSeconds;
    double elapsed = (juce::Time::getMillisecondCounterHiRes() - loopStartMs) / 1000.0;
    if (elapsed >= loopSeconds) { loopStartMs += std::floor(elapsed / loopSeconds) * loopSeconds * 1000.0; elapsed = std::fmod(elapsed, loopSeconds); }
    // The pause after the render holds the last (silent) frame before the note starts again
    setPlayhead(juce::jmin(elapsed, renderSeconds));
}

void OutputScope::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(UiTheme::dark);
    g.fillRoundedRectangle(bounds, 3.0f);
    
    auto strip = bounds.reduced(2.0f, 0.0f).removeFromBottom(static_cast<float>(kEnvelopeHeight)).translated(0.0f, -2.0f);
    auto plot = bounds.reduced(2.0f, 3.0f).withTrimmedBottom(static_cast<float>(kEnvelopeHeight) + 2.0f);
    g.setColour(UiTheme::border);
    g.drawHorizontalLine(static_cast<int>(plot.getCentreY()), plot.getX(), plot.getRight());
    
    const bool audible = peak > kSilence;
    if (frame.size() > 1) {
        // Scaled to the render's peak, so the trace grows and fades with the envelope
        juce::Path path;
        const float midY = plot.getCentreY();
        const float scale = plot.getHeight() * 0.48f / juce::jmax(peak, 0.05f);
        const float step = plot.getWidth() / static_cast<float>(frame.size() - 1);
        for (size_t i = 0; i < frame.size(); ++i) {
            const float x = plot.getX() + step * static_cast<float>(i);
            const float y = midY - juce::jlimit(-1.0f, 1.0f, frame[i]) * scale;
            if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
        }
        g.setColour(audible ? UiTheme::green : UiTheme::dim);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }
    
    // Envelope strip: level over the whole render, the key-off as a tick, the playhead as a cursor
    g.setColour(UiTheme::borderSoft);
    g.fillRoundedRectangle(strip, 2.0f);
    if (!envelopeBins.empty() && audible) {
        juce::Path area;
        const float binWidth = strip.getWidth() / static_cast<float>(envelopeBins.size());
        area.startNewSubPath(strip.getX(), strip.getBottom());
        for (size_t i = 0; i < envelopeBins.size(); ++i) {
            const float h = juce::jlimit(0.0f, 1.0f, envelopeBins[i] / juce::jmax(peak, 0.05f)) * (strip.getHeight() - 1.0f);
            area.lineTo(strip.getX() + binWidth * (static_cast<float>(i) + 0.5f), strip.getBottom() - h);
        }
        area.lineTo(strip.getRight(), strip.getBottom());
        area.closeSubPath();
        g.setColour((peak > 0.95f ? UiTheme::amber : UiTheme::green).withAlpha(0.55f));
        g.fillPath(area);
    }
    if (!samples.empty()) {
        const float offX = strip.getX() + strip.getWidth() * static_cast<float>(noteOff) / static_cast<float>(samples.size());
        g.setColour(UiTheme::dim);
        g.drawVerticalLine(static_cast<int>(offX), strip.getY(), strip.getBottom());
        const float cursorX = strip.getX() + strip.getWidth() * juce::jlimit(0.0f, 1.0f, static_cast<float>(playhead * rate / static_cast<double>(samples.size())));
        g.setColour(UiTheme::text);
        g.fillRect(cursorX - 0.5f, strip.getY(), 1.5f, strip.getHeight());
    }
}
