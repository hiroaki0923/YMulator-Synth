#include "MotionPanel.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../utils/ParameterIDs.h"

namespace {
using ParamID::Motion::Wide;
using ParamID::Motion::WidePan;
using ParamID::Motion::VibratoDepth;
using ParamID::Motion::VibratoRate;
using ParamID::Motion::VibratoDelay;
using ParamID::Motion::VibratoRise;
using ParamID::Motion::TimbreDepth;
using ParamID::Motion::TimbreRate;
using ParamID::Motion::TremoloDepth;
using ParamID::Motion::TremoloRate;
using ParamID::Motion::PanMode;
using ParamID::Motion::PanRate;
using ParamID::Motion::Sync;
using ParamID::Motion::PitchEnv;
using ParamID::Motion::EchoLevel;
using ParamID::Motion::EchoTime;
using ParamID::Motion::EchoDiv;
using ParamID::Motion::SweepAmount;
using ParamID::Motion::SweepTime;
using ParamID::Motion::Mono;
using ParamID::Motion::PortaTime;
using ParamID::Motion::ArpMode;
using ParamID::Motion::ArpDiv;
using ParamID::Motion::VelBright;
using ParamID::Motion::PitchTime;
using ParamID::Motion::PitchEnv2;
using ParamID::Motion::PitchTime2;
}

const std::vector<MotionPanel::MotionPreset>& MotionPanel::presets()
{
    // Every preset states every motion parameter it touches; "Off" clears them all
    static const std::vector<MotionPreset> list = {
        { "Off",   { { Wide, 0 }, { VibratoDepth, 0 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, 0 }, { Sync, 0 }, { EchoLevel, 0 }, { SweepAmount, 0 }, { Mono, 0 }, { PortaTime, 0 }, { ArpMode, 0 }, { VelBright, 0 } } },
        { "Glide", { { Mono, 1 }, { PortaTime, 120 }, { ArpMode, 0 }, { VelBright, 40 } } },
        { "Arp",   { { Mono, 1 }, { ArpMode, 1 }, { ArpDiv, 9 }, { PortaTime, 0 } } },
        { "Sweep", { { Wide, 30 }, { WidePan, 1 }, { VibratoDepth, 0 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, 0 }, { SweepAmount, 32 }, { SweepTime, 1800 }, { EchoLevel, 40 }, { EchoTime, 240 } } },
        { "Echo",  { { Wide, 0 }, { WidePan, 0 }, { VibratoDepth, 0 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, 0 }, { EchoLevel, 55 }, { EchoTime, 180 }, { EchoDiv, 3 } } },
        { "Wide",  { { Wide, 60 }, { WidePan, 0 }, { VibratoDepth, 0 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, 0 } } },
        { "Vib",   { { Wide, 0 }, { VibratoDepth, 40 }, { VibratoRate, 5.5f }, { VibratoDelay, 250 }, { VibratoRise, 400 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, 0 } } },
        { "Growl", { { Wide, 40 }, { WidePan, 0 }, { VibratoDepth, 30 }, { VibratoRate, 6 }, { VibratoDelay, 150 }, { VibratoRise, 300 }, { TimbreDepth, 20 }, { TimbreRate, 0.7f }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, -60 } } },
        { "Pan",   { { Wide, 0 }, { VibratoDepth, 0 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 2 }, { PanRate, 3 }, { Sync, 1 }, { PitchEnv, 0 } } },
        { "Trem",  { { Wide, 0 }, { VibratoDepth, 0 }, { TimbreDepth, 10 }, { TimbreRate, 0.3f }, { TremoloDepth, 16 }, { TremoloRate, 4 }, { PanMode, 0 }, { PitchEnv, 0 } } },
        { "Kick",  { { Wide, 0 }, { VibratoDepth, 0 }, { TimbreDepth, 0 }, { TremoloDepth, 0 }, { PanMode, 0 }, { PitchEnv, 1200 }, { PitchTime, 18 }, { PitchEnv2, -500 }, { PitchTime2, 140 }, { EchoLevel, 0 } } },
    };
    return list;
}

MotionPanel::MotionPanel(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    for (size_t i = 0; i < presets().size(); ++i) {
        auto chip = std::make_unique<juce::TextButton>(presets()[i].name);
        chip->getProperties().set("chip", true);
        chip->setTooltip("Apply this motion; the knobs below can then be adjusted");
        chip->onClick = [this, i]() { applyPreset(static_cast<int>(i)); };
        addAndMakeVisible(*chip);
        chips.push_back(std::move(chip));
    }
    
    makeKnob(wide, Wide, "Wide", UiTheme::carrier);
    makeKnob(vibrato, VibratoDepth, "Vib", UiTheme::green);
    makeKnob(timbre, TimbreDepth, "Timbre", UiTheme::amber);
    makeKnob(echo, EchoLevel, "Echo", UiTheme::carrier);
    makeKnob(rate, VibratoRate, "Rate", UiTheme::green);
    rate.knob->setValueFormatter([](double v) { return juce::String(v, 1); });
    
    panModeBox = std::make_unique<juce::ComboBox>();
    panModeBox->addItemList({ "Pan off", "Alternate", "Step" }, 1);
    panModeBox->setTooltip("Pan motion: alternate notes left/right, or step L-C-R on the beat");
    addAndMakeVisible(*panModeBox);
    panModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), PanMode, *panModeBox);
    
    syncButton = std::make_unique<juce::ToggleButton>("Sync");
    syncButton->setTooltip("Rates follow the host tempo");
    syncButton->onClick = [this]() { updateSyncVisibility(); };
    addAndMakeVisible(*syncButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), Sync, *syncButton);
    
    rateDivisionBox = std::make_unique<juce::ComboBox>();
    rateDivisionBox->addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/2T", "1/4T", "1/8T" }, 1);
    rateDivisionBox->setTooltip("Vibrato rate as a note value");
    addChildComponent(*rateDivisionBox);
    rateDivisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getParameters(), ParamID::Motion::VibratoDiv, *rateDivisionBox);
    
    updateSyncVisibility();
}

void MotionPanel::makeKnob(Knob& target, const char* parameterId, const juce::String& label, juce::Colour accent)
{
    target.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Small);
    target.knob->setAccentColour(accent);
    addAndMakeVisible(*target.knob);
    target.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *target.knob, *this);
}

void MotionPanel::applyPreset(int index)
{
    if (index < 0 || index >= static_cast<int>(presets().size())) return;
    for (const auto& [id, value] : presets()[static_cast<size_t>(index)].values) {
        if (auto* p = audioProcessor.getParameters().getParameter(id)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(value));
            p->endChangeGesture();
        }
    }
    updateSyncVisibility();
}

void MotionPanel::updateSyncVisibility()
{
    const bool synced = syncButton->getToggleState();
    rateDivisionBox->setVisible(synced);
    rate.knob->setVisible(!synced);
}

void MotionPanel::resized()
{
    auto bounds = getLocalBounds();
    // Chips in rows of six
    const int perRow = 6;
    const int chipWidth = (bounds.getWidth() - (perRow - 1) * 4) / perRow;
    for (size_t i = 0; i < chips.size(); ++i) {
        if (i % static_cast<size_t>(perRow) == 0) {
            if (i > 0) bounds.removeFromTop(4);
            auto row = bounds.removeFromTop(22);
            for (size_t j = i; j < std::min(chips.size(), i + static_cast<size_t>(perRow)); ++j) {
                chips[j]->setBounds(row.removeFromLeft(chipWidth));
                row.removeFromLeft(4);
            }
        }
    }
    bounds.removeFromTop(8);
    
    const auto knobSize = RotaryKnob::preferredSize(RotaryKnob::Style::Small, RotaryKnob::LabelPosition::Below, false, 42);
    auto knobRow = bounds.removeFromTop(knobSize.getHeight());
    const int slot = knobRow.getWidth() / 5;
    int x = knobRow.getX();
    for (Knob* k : { &wide, &vibrato, &timbre, &echo, &rate }) {
        k->knob->setBounds(x + (slot - knobSize.getWidth()) / 2, knobRow.getY(), knobSize.getWidth(), knobSize.getHeight());
        x += slot;
    }
    rateDivisionBox->setBounds(rate.knob->getBounds().withHeight(24).withY(knobRow.getY() + 8).expanded(4, 0));
    bounds.removeFromTop(6);
    
    auto bottom = bounds.removeFromTop(24);
    syncButton->setBounds(bottom.removeFromRight(64));
    panModeBox->setBounds(bottom.removeFromLeft(110));
}
