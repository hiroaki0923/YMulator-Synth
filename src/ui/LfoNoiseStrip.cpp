#include "LfoNoiseStrip.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../utils/ParameterIDs.h"

LfoNoiseStrip::LfoNoiseStrip(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    auto sectionLabel = [](const juce::String& text) {
        auto label = std::make_unique<juce::Label>("", text);
        label->setFont(UiTheme::mono(11.0f, true));
        label->setColour(juce::Label::textColourId, UiTheme::green);
        return label;
    };
    lfoLabel = sectionLabel("LFO");
    addAndMakeVisible(*lfoLabel);
    noiseLabel = sectionLabel("NOISE");
    addAndMakeVisible(*noiseLabel);
    
    makeKnob(lfoRate, ParamID::Global::LfoRate, "Rate");
    makeKnob(lfoAmd, ParamID::Global::LfoAmd, "AMD");
    makeKnob(lfoPmd, ParamID::Global::LfoPmd, "PMD");
    
    lfoWaveformComboBox = std::make_unique<juce::ComboBox>();
    lfoWaveformComboBox->addItemList({ "Saw", "Square", "Triangle", "Noise" }, 1);
    lfoWaveformComboBox->setTooltip("LFO waveform");
    addAndMakeVisible(*lfoWaveformComboBox);
    lfoWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Global::LfoWaveform, *lfoWaveformComboBox);
    
    noiseEnableButton = std::make_unique<juce::ToggleButton>();
    noiseEnableButton->setTooltip("Replace operator 4 (C2) with the noise generator");
    addAndMakeVisible(*noiseEnableButton);
    noiseEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Global::NoiseEnable, *noiseEnableButton);
    
    makeKnob(noiseFrequency, ParamID::Global::NoiseFrequency, "Freq");
    
    expressiveButton = std::make_unique<juce::ToggleButton>("Wheel: Vib / Touch: Bright");
    expressiveButton->setTooltip("Mod wheel sets vibrato depth and aftertouch opens Brightness, instead of the compatible CC map");
    addAndMakeVisible(*expressiveButton);
    expressiveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Global::Expressive, *expressiveButton);
    
    statusLabel = std::make_unique<juce::Label>("", "");
    statusLabel->setFont(UiTheme::mono(10.0f));
    statusLabel->setColour(juce::Label::textColourId, UiTheme::muted);
    statusLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*statusLabel);
}

void LfoNoiseStrip::makeKnob(Knob& target, const char* parameterId, const juce::String& label)
{
    target.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Tiny);
    target.knob->setLabelPosition(RotaryKnob::LabelPosition::Right);
    target.knob->setAccentColour(UiTheme::carrier);
    addAndMakeVisible(*target.knob);
    target.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *target.knob, *this);
}

void LfoNoiseStrip::setStatusText(const juce::String& text)
{
    statusLabel->setText(text, juce::dontSendNotification);
}

void LfoNoiseStrip::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::strip);
    g.setColour(UiTheme::borderSoft);
    g.fillRect(0, 0, getWidth(), 1);
    g.setColour(UiTheme::border);
    g.fillRect(separatorX, getHeight() / 2 - 12, 1, 24);
}

void LfoNoiseStrip::resized()
{
    auto bounds = getLocalBounds().reduced(20, 0);
    const int centreY = bounds.getCentreY();
    auto placeKnob = [&](Knob& k, int x) {
        const auto size = RotaryKnob::preferredSize(RotaryKnob::Style::Tiny, RotaryKnob::LabelPosition::Right, false, 32);
        k.knob->setBounds(x, centreY - size.getHeight() / 2, size.getWidth(), size.getHeight());
        return x + size.getWidth() + 10;
    };
    
    int x = bounds.getX();
    lfoLabel->setBounds(x, centreY - 10, 34, 20);
    x += 40;
    x = placeKnob(lfoRate, x);
    x = placeKnob(lfoAmd, x);
    x = placeKnob(lfoPmd, x);
    lfoWaveformComboBox->setBounds(x, centreY - 12, 84, 24);
    x += 84 + 14;
    separatorX = x;
    x += 1 + 14;
    noiseLabel->setBounds(x, centreY - 10, 46, 20);
    x += 50;
    noiseEnableButton->setBounds(x, centreY - 9, 32, 18);
    x += 32 + 10;
    x = placeKnob(noiseFrequency, x);
    x += 14;
    expressiveButton->setBounds(x, centreY - 10, 200, 20);
    x += 200;
    
    statusLabel->setBounds(x, centreY - 10, juce::jmax(0, bounds.getRight() - x), 20);
}
