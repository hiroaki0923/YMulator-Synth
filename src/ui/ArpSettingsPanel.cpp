#include "ArpSettingsPanel.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../utils/ParameterIDs.h"

ArpSettingsPanel::ArpSettingsPanel(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    using namespace ParamID::Motion;
    
    title = std::make_unique<juce::Label>("", "ARPEGGIO");
    title->setFont(UiTheme::mono(11.0f, true));
    title->setColour(juce::Label::textColourId, UiTheme::green);
    addAndMakeVisible(*title);
    
    auto makeLabel = [&](std::unique_ptr<juce::Label>& label, const juce::String& text) {
        label = std::make_unique<juce::Label>("", text);
        label->setFont(UiTheme::mono(10.0f));
        label->setColour(juce::Label::textColourId, UiTheme::muted);
        addAndMakeVisible(*label);
    };
    makeLabel(chordLabel, "Chord");
    chordBox = std::make_unique<juce::ComboBox>();
    chordBox->addItemList({ "None", "Major", "Minor", "7th", "m7", "Maj7", "Sus4", "Sus2", "Dim", "Aug", "5th", "Octave" }, 1);
    chordBox->setTooltip("Chord table applied to a single held note; two or more held notes are used as they are");
    addAndMakeVisible(*chordBox);
    chordAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), ArpChord, *chordBox);
    
    makeLabel(accentLabel, "Accent");
    accentBox = std::make_unique<juce::ComboBox>();
    accentBox->addItemList({ "Off", "Beat", "2 steps", "3 steps", "4 steps" }, 1);
    accentBox->setTooltip("Which steps keep their level; the others sit back by the depth");
    addAndMakeVisible(*accentBox);
    accentAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), ArpAccent, *accentBox);
    
    makeKnob(octaves, ArpOctaves, "Oct", UiTheme::green);
    makeKnob(gate, ArpGate, "Gate", UiTheme::green, [](double v) { return juce::String(juce::roundToInt(v)) + "%"; });
    makeKnob(accentDepth, ArpAccentDepth, "Depth", UiTheme::carrier);
    
    retriggerButton = std::make_unique<juce::ToggleButton>("Retrig");
    retriggerButton->setTooltip("Key every step on again (off: chip style, only the pitch changes)");
    addAndMakeVisible(*retriggerButton);
    retriggerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), ArpRetrigger, *retriggerButton);
    latchButton = std::make_unique<juce::ToggleButton>("Latch");
    latchButton->setTooltip("The chord keeps playing after the keys are released, until the next chord");
    addAndMakeVisible(*latchButton);
    latchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), ArpLatch, *latchButton);
    
    setSize(kWidth, kHeight);   // last: resized() lays out the children above
}

void ArpSettingsPanel::makeKnob(Knob& target, const char* parameterId, const juce::String& label, juce::Colour accent,
                                std::function<juce::String(double)> formatter)
{
    target.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Small);
    target.knob->setAccentColour(accent);
    if (formatter) target.knob->setValueFormatter(std::move(formatter));
    addAndMakeVisible(*target.knob);
    target.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *target.knob, *this);
}

void ArpSettingsPanel::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::panel);
}

void ArpSettingsPanel::resized()
{
    auto bounds = getLocalBounds().reduced(12, 8);
    title->setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(4);
    
    auto row = bounds.removeFromTop(24);
    chordLabel->setBounds(row.removeFromLeft(44));
    chordBox->setBounds(row.removeFromLeft(88));
    row.removeFromLeft(12);
    accentLabel->setBounds(row.removeFromLeft(48));
    accentBox->setBounds(row.removeFromLeft(84));
    bounds.removeFromTop(8);
    
    const auto knobSize = RotaryKnob::preferredSize(RotaryKnob::Style::Small, RotaryKnob::LabelPosition::Below, false, 42);
    auto knobs = bounds.removeFromTop(knobSize.getHeight());
    int x = knobs.getX();
    for (Knob* k : { &octaves, &gate, &accentDepth }) {
        k->knob->setBounds(x, knobs.getY(), knobSize.getWidth(), knobSize.getHeight());
        x += knobSize.getWidth() + 12;
    }
    x += 8;
    retriggerButton->setBounds(x, knobs.getY() + 4, 78, 22);
    latchButton->setBounds(x, knobs.getY() + 30, 78, 22);
}
