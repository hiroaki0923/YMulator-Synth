#include "MotionPanel.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../utils/ParameterIDs.h"

namespace {
using namespace ParamID::Motion;

juce::String seconds(double v) { return UiTheme::formatTime(v); }
juce::String signedInt(double v)
{
    const int i = juce::roundToInt(v);
    return i == 0 ? juce::String("0") : (i > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(i));
}
}

const std::vector<MotionPanel::Feature>& MotionPanel::features()
{
    static const std::vector<Feature> list = {
        { "Wide",  Wide,         { { Wide, 50 }, { WidePan, 0 } },                                                     { { Wide, 0 } } },
        { "Vib",   VibratoDepth, { { VibratoDepth, 35 }, { VibratoRate, 5.5f }, { VibratoDelay, 200 }, { VibratoRise, 350 }, { VibratoWave, 0 } }, { { VibratoDepth, 0 } } },
        { "Growl", TimbreDepth,  { { TimbreDepth, 18 }, { TimbreRate, 0.7f }, { TimbreWave, 1 } },                      { { TimbreDepth, 0 } } },
        { "Echo",  EchoLevel,    { { EchoLevel, 50 }, { EchoTime, 180 }, { EchoDiv, 3 } },                              { { EchoLevel, 0 } } },
        { "Sweep", SweepAmount,  { { SweepAmount, 30 }, { SweepTime, 1500 } },                                          { { SweepAmount, 0 } } },
        { "Swell", LevelAttack,  { { LevelAttack, 800 }, { LevelDecay, 0 }, { LevelSustain, 0 } },                      { { LevelAttack, 0 }, { LevelSustain, 0 } } },
        { "Glide", PortaTime,    { { Mono, 1 }, { PortaTime, 120 } },                                                   { { PortaTime, 0 }, { Mono, 0 } } },
        { "Arp",   ArpMode,      { { ArpMode, 1 }, { ArpDiv, 9 }, { Mono, 1 } },                                        { { ArpMode, 0 } } },
        { "Kick",  PitchEnv,     { { PitchEnv, 1200 }, { PitchTime, 18 }, { PitchEnv2, -500 }, { PitchTime2, 140 } },   { { PitchEnv, 0 }, { PitchEnv2, 0 } } },
        { "Trem",  TremoloDepth, { { TremoloDepth, 16 }, { TremoloRate, 4 } },                                          { { TremoloDepth, 0 } } },
        { "Pan",   PanMode,      { { PanMode, 2 }, { PanRate, 3 }, { Sync, 1 } },                                       { { PanMode, 0 } } },
    };
    return list;
}

MotionPanel::MotionPanel(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    for (const auto& feature : features()) {
        auto chip = std::make_unique<juce::TextButton>(feature.name);
        chip->getProperties().set("chip", true);
        chip->setClickingTogglesState(false);
        chip->setTooltip(juce::String("Switch ") + feature.name + " on or off; the knobs set how much");
        const juce::String name(feature.name);
        chip->onClick = [this, name]() { toggleFeature(name); };
        addAndMakeVisible(*chip);
        chips.push_back(std::move(chip));
    }
    offButton = std::make_unique<juce::TextButton>("Off");
    offButton->getProperties().set("chip", true);
    offButton->setTooltip("Every motion off");
    offButton->onClick = [this]() { allOff(); };
    addAndMakeVisible(*offButton);
    
    makeKnob(wide, Wide, "Wide", UiTheme::carrier);
    makeKnob(vibrato, VibratoDepth, "Vib", UiTheme::green);
    makeKnob(timbre, TimbreDepth, "Timbre", UiTheme::amber);
    makeKnob(echo, EchoLevel, "Echo", UiTheme::carrier);
    makeKnob(rate, VibratoRate, "Vib rate", UiTheme::green, [](double v) { return juce::String(v, 1); });
    makeKnob(sweep, SweepAmount, "Sweep", UiTheme::amber, signedInt);
    makeKnob(swell, LevelAttack, "Swell", UiTheme::green, seconds);
    makeKnob(porta, PortaTime, "Porta", UiTheme::green);
    makeKnob(pitch, PitchEnv, "Pitch", UiTheme::carrier, signedInt);
    makeKnob(bright, VelBright, "Bright", UiTheme::amber);
    
    panModeBox = std::make_unique<juce::ComboBox>();
    panModeBox->addItemList({ "Pan off", "Alternate", "Step" }, 1);
    panModeBox->setTooltip("Pan motion: alternate notes left/right, or step L-C-R on the beat");
    addAndMakeVisible(*panModeBox);
    panModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), PanMode, *panModeBox);
    arpModeBox = std::make_unique<juce::ComboBox>();
    arpModeBox->addItemList({ "Arp off", "Up", "Down", "Up Down" }, 1);
    arpModeBox->setTooltip("Held notes take turns on one channel");
    addAndMakeVisible(*arpModeBox);
    arpModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), ArpMode, *arpModeBox);
    
    // The division box must exist before the Sync attachment: attaching fires the
    // button's click handler at once when the stored value is on
    rateDivisionBox = std::make_unique<juce::ComboBox>();
    rateDivisionBox->addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/2T", "1/4T", "1/8T", "1/32", "1/64", "1/16T" }, 1);
    rateDivisionBox->setTooltip("Vibrato rate as a note value while Sync is on");
    addChildComponent(*rateDivisionBox);
    rateDivisionLabel = std::make_unique<juce::Label>("", "Vib rate");
    rateDivisionLabel->setFont(UiTheme::mono(11.0f));
    rateDivisionLabel->setColour(juce::Label::textColourId, UiTheme::muted);
    rateDivisionLabel->setJustificationType(juce::Justification::centred);
    addChildComponent(*rateDivisionLabel);
    rateDivisionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), VibratoDiv, *rateDivisionBox);
    
    syncButton = std::make_unique<juce::ToggleButton>("Sync");
    syncButton->setTooltip("Rates follow the host tempo");
    syncButton->onClick = [this]() { updateSyncVisibility(); };
    addAndMakeVisible(*syncButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), Sync, *syncButton);
    
    updateSyncVisibility();
    refresh();
}

void MotionPanel::makeKnob(Knob& target, const char* parameterId, const juce::String& label, juce::Colour accent, std::function<juce::String(double)> formatter)
{
    target.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Small);
    target.knob->setAccentColour(accent);
    if (formatter) target.knob->setValueFormatter(std::move(formatter));
    addAndMakeVisible(*target.knob);
    target.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *target.knob, *this, [this]() { refresh(); });
}

float MotionPanel::value(const char* parameterId) const
{
    auto* p = audioProcessor.getParameters().getParameter(parameterId);
    return p ? p->convertFrom0to1(p->getValue()) : 0.0f;
}

void MotionPanel::apply(const std::vector<std::pair<const char*, float>>& values)
{
    for (const auto& [id, v] : values)
        if (auto* p = audioProcessor.getParameters().getParameter(id)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(v));
            p->endChangeGesture();
        }
    updateSyncVisibility();
    refresh();
}

bool MotionPanel::isFeatureOn(const juce::String& name) const
{
    for (const auto& f : features())
        if (name == f.name) return value(f.mainParameter) != 0.0f;
    return false;
}

bool MotionPanel::toggleFeature(const juce::String& name)
{
    for (const auto& f : features()) {
        if (name != f.name) continue;
        apply(isFeatureOn(name) ? f.offValues : f.onValues);
        return true;
    }
    return false;
}

void MotionPanel::allOff()
{
    for (const auto& f : features()) apply(f.offValues);
    apply({ { Sync, 0 }, { VelBright, 0 }, { Mono, 0 }, { LfoOneShot, 0 } });
}

void MotionPanel::refresh()
{
    for (size_t i = 0; i < chips.size(); ++i)
        chips[i]->setToggleState(isFeatureOn(features()[i].name), juce::dontSendNotification);
}

void MotionPanel::updateSyncVisibility()
{
    if (!syncButton || !rateDivisionBox || !rate.knob) return;
    const bool synced = syncButton->getToggleState();
    rateDivisionBox->setVisible(synced);
    if (rateDivisionLabel) rateDivisionLabel->setVisible(synced);
    rate.knob->setVisible(!synced);
}

void MotionPanel::resized()
{
    auto bounds = getLocalBounds();
    // Chips: two rows of six, the Off button last
    const int perRow = 6;
    const int chipWidth = (bounds.getWidth() - (perRow - 1) * 4) / perRow;
    std::vector<juce::TextButton*> all;
    for (auto& c : chips) all.push_back(c.get());
    all.push_back(offButton.get());
    for (size_t i = 0; i < all.size(); i += static_cast<size_t>(perRow)) {
        if (i > 0) bounds.removeFromTop(4);
        auto row = bounds.removeFromTop(22);
        for (size_t j = i; j < std::min(all.size(), i + static_cast<size_t>(perRow)); ++j) {
            all[j]->setBounds(row.removeFromLeft(chipWidth));
            row.removeFromLeft(4);
        }
    }
    bounds.removeFromTop(8);
    
    const auto knobSize = RotaryKnob::preferredSize(RotaryKnob::Style::Small, RotaryKnob::LabelPosition::Below, false, 42);
    auto placeRow = [&](std::initializer_list<Knob*> knobs) {
        auto row = bounds.removeFromTop(knobSize.getHeight());
        const int slot = row.getWidth() / static_cast<int>(knobs.size());
        int x = row.getX();
        for (Knob* k : knobs) {
            k->knob->setBounds(x + (slot - knobSize.getWidth()) / 2, row.getY(), knobSize.getWidth(), knobSize.getHeight());
            x += slot;
        }
        return row;
    };
    const auto rowA = placeRow({ &wide, &vibrato, &rate, &timbre, &echo });
    // Under Sync the Vib rate knob gives its place to a note-value box, labelled like the knob
    const auto rateBounds = rate.knob->getBounds();
    rateDivisionBox->setBounds(juce::Rectangle<int>(58, 24).withCentre({ rateBounds.getCentreX(), rateBounds.getY() + 18 }));
    rateDivisionLabel->setBounds(juce::Rectangle<int>(60, 14).withCentre({ rateBounds.getCentreX(), rateBounds.getBottom() - 7 }));
    bounds.removeFromTop(4);
    placeRow({ &sweep, &swell, &porta, &pitch, &bright });
    bounds.removeFromTop(6);
    
    auto bottom = bounds.removeFromTop(24);
    syncButton->setBounds(bottom.removeFromRight(62));
    panModeBox->setBounds(bottom.removeFromLeft(82));
    bottom.removeFromLeft(6);
    arpModeBox->setBounds(bottom.removeFromLeft(82));
}
