#include "ToneStrip.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../dsp/AlgorithmInfo.h"
#include "../utils/ParameterIDs.h"

namespace {
constexpr int kKnobWidth = 42;
constexpr int kKnobGap = 12;

juce::String signedText(double v)
{
    const int i = juce::roundToInt(v);
    if (i == 0) return "0";
    return (i > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(i));
}

const char* const kHarmonicsNames[] = { "Preset", "Saw", "Square", "Pulse", "Bright", "Bell", "Metal", "Sub", "Oct" };
}

ToneStrip::ToneStrip(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    using ymulatorsynth::Macro;
    sectionLabel = std::make_unique<juce::Label>("", "TONE");
    sectionLabel->setFont(UiTheme::mono(11.0f, true));
    sectionLabel->setColour(juce::Label::textColourId, UiTheme::green);
    addAndMakeVisible(*sectionLabel);
    
    addMacroKnob(Macro::Brightness, ParamID::Macro::Brightness, "Bright", UiTheme::amber);
    addMacroKnob(Macro::Harmonics, ParamID::Macro::Harmonics, "Harm", UiTheme::amber);
    addMacroKnob(std::nullopt, ParamID::Global::Feedback, "FB", UiTheme::amber);
    addMacroKnob(Macro::Attack, ParamID::Macro::Attack, "Atk", UiTheme::green);
    addMacroKnob(Macro::Decay, ParamID::Macro::Decay, "Dec", UiTheme::green);
    addMacroKnob(Macro::Release, ParamID::Macro::Release, "Rel", UiTheme::green);
    addMacroKnob(Macro::Spread, ParamID::Macro::Spread, "Sprd", UiTheme::carrier);
    
    hintLabel = std::make_unique<juce::Label>("", "Touch a TONE knob to see which operator knobs it moves.");
    hintLabel->setFont(UiTheme::sans(11.0f));
    hintLabel->setColour(juce::Label::textColourId, UiTheme::muted);
    addAndMakeVisible(*hintLabel);
    
    algorithmDisplay = std::make_unique<AlgorithmDisplay>();
    addAndMakeVisible(*algorithmDisplay);
    
    algorithmComboBox = std::make_unique<juce::ComboBox>();
    for (int i = 0; i <= 7; ++i) algorithmComboBox->addItem("ALG " + juce::String(i), i + 1);
    algorithmComboBox->setTooltip("Operator routing");
    addAndMakeVisible(*algorithmComboBox);
    algorithmAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::Algorithm, *algorithmComboBox);
    
    setAlgorithm(algorithmComboBox->getSelectedId() - 1);
}

void ToneStrip::addMacroKnob(std::optional<ymulatorsynth::Macro> macro, const char* parameterId,
                             const juce::String& label, juce::Colour accent)
{
    MacroKnob entry;
    entry.macro = macro;
    entry.parameterId = parameterId;
    entry.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Small);
    entry.knob->setAccentColour(accent);
    if (macro == ymulatorsynth::Macro::Harmonics) {
        entry.knob->setValueFormatter([](double v) {
            const int i = juce::jlimit(0, 8, juce::roundToInt(v));
            return juce::String(kHarmonicsNames[i]);
        });
        entry.knob->setTooltip("Modulator ratio template; Preset keeps the loaded values");
    } else if (macro.has_value()) {
        entry.knob->setValueFormatter(signedText);
        entry.knob->setTooltip("Offset from the loaded preset; 0 leaves it unchanged");
    } else {
        entry.knob->setTooltip("Operator 1 feedback");
    }
    addAndMakeVisible(*entry.knob);
    entry.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *entry.knob, *this);
    
    const auto index = knobs.size();
    auto previousStart = entry.knob->onGestureStart;
    auto previousEnd = entry.knob->onGestureEnd;
    entry.knob->onGestureStart = [this, index, previousStart]() { if (previousStart) previousStart(); knobs[index].dragging = true; updateFocus(); };
    entry.knob->onGestureEnd = [this, index, previousEnd]() { if (previousEnd) previousEnd(); knobs[index].dragging = false; updateFocus(); };
    entry.knob->onHoverChanged = [this, index](bool over) { knobs[index].hovered = over; updateFocus(); };
    knobs.push_back(std::move(entry));
}

void ToneStrip::updateFocus()
{
    if (!onMacroFocus) return;
    // A drag keeps its focus even when the pointer leaves the knob
    for (const auto& k : knobs) if (k.dragging) { onMacroFocus(k.macro); return; }
    for (const auto& k : knobs) if (k.hovered) { onMacroFocus(k.macro); return; }
    onMacroFocus(std::nullopt);
}

void ToneStrip::setAlgorithm(int algorithm)
{
    currentAlgorithm = juce::jlimit(0, 7, algorithm);
    algorithmDisplay->setAlgorithm(currentAlgorithm);
    algorithmComboBox->setTooltip(ymulatorsynth::algorithmInfo(currentAlgorithm).description);
}

void ToneStrip::setFeedback(int feedback)
{
    algorithmDisplay->setFeedbackLevel(juce::jlimit(0, 7, feedback));
}

void ToneStrip::setHighlightedParameters(const std::set<std::string>& parameterIds)
{
    for (auto& k : knobs)
        k.knob->setHighlighted(parameterIds.count(k.parameterId) > 0);
}

std::vector<std::string> ToneStrip::highlightedParameterIds() const
{
    std::vector<std::string> ids;
    for (const auto& k : knobs) if (k.knob->isHighlighted()) ids.push_back(k.parameterId);
    return ids;
}

void ToneStrip::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::strip);
    g.setColour(UiTheme::borderSoft);
    g.fillRect(0, getHeight() - 1, getWidth(), 1);
    g.setColour(UiTheme::border);
    const int sepX = knobs.empty() ? 0 : knobs.back().knob->getRight() + kKnobGap;
    g.fillRect(sepX, getHeight() / 2 - 20, 1, 40);
}

void ToneStrip::resized()
{
    auto bounds = getLocalBounds().reduced(20, 0);
    const int centreY = bounds.getCentreY();
    
    auto labelArea = bounds.removeFromLeft(52);
    sectionLabel->setBounds(labelArea.withHeight(20).withCentre(labelArea.getCentre()));
    int x = bounds.getX();
    for (auto& k : knobs) {
        const auto size = RotaryKnob::preferredSize(RotaryKnob::Style::Small, RotaryKnob::LabelPosition::Below, false, kKnobWidth);
        k.knob->setBounds(x, centreY - size.getHeight() / 2, size.getWidth(), size.getHeight());
        x += size.getWidth() + kKnobGap;
    }
    x += kKnobGap + 1 + kKnobGap;
    
    // Right cluster: diagram, then the picker with the structure text under it
    auto right = bounds;
    auto pickerArea = right.removeFromRight(100);
    algorithmComboBox->setBounds(pickerArea.withHeight(26).withCentre(pickerArea.getCentre()));
    right.removeFromRight(10);
    algorithmDisplay->setBounds(right.removeFromRight(120).withHeight(getHeight() - 8).withCentre({ right.getRight() + 60, centreY }));
    right.removeFromRight(16);
    
    hintLabel->setBounds(juce::Rectangle<int>(x, centreY - 15, juce::jmax(0, right.getRight() - x), 30));
}
