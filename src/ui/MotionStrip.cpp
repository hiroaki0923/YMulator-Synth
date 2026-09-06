#include "MotionStrip.h"
#include <array>
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../utils/ParameterIDs.h"

namespace {
constexpr int kGroupGap = 8;
constexpr int kKnobGap = 3;
constexpr int kKnobWidth = 36;
juce::String oneDecimal(double v) { return juce::String(v, 1); }
juce::String milliseconds(double v) { return juce::String(juce::roundToInt(v)); }
}

MotionStrip::MotionStrip(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    using namespace ParamID::Motion;
    sectionLabel = std::make_unique<juce::Label>("", "MOTION");
    sectionLabel->setFont(UiTheme::mono(11.0f, true));
    sectionLabel->setColour(juce::Label::textColourId, UiTheme::green);
    addAndMakeVisible(*sectionLabel);
    
    auto& vib = addGroup("Vibrato");
    addKnob(vib, VibratoDepth, "Depth", UiTheme::green);
    addKnob(vib, VibratoRate, "Rate", UiTheme::green, true, oneDecimal);
    addKnob(vib, VibratoDelay, "Delay", UiTheme::green, false, milliseconds);
    addKnob(vib, VibratoRise, "Rise", UiTheme::green, false, milliseconds);
    
    auto& wide = addGroup("Wide");
    addKnob(wide, Wide, "Amount", UiTheme::carrier);
    widePanBox = std::make_unique<juce::ComboBox>();
    widePanBox->addItemList({ "L / R", "Center" }, 1);
    widePanBox->setTooltip("Where the two chips sit");
    addAndMakeVisible(*widePanBox);
    widePanAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), WidePan, *widePanBox);
    wide.boxes.push_back(widePanBox.get());
    
    auto& timbre = addGroup("Timbre");
    addKnob(timbre, TimbreDepth, "Depth", UiTheme::amber);
    addKnob(timbre, TimbreRate, "Rate", UiTheme::amber, true, oneDecimal);
    
    auto& trem = addGroup("Tremolo");
    addKnob(trem, TremoloDepth, "Depth", UiTheme::carrier);
    addKnob(trem, TremoloRate, "Rate", UiTheme::carrier, true, oneDecimal);
    
    auto& pan = addGroup("Pan");
    panModeBox = std::make_unique<juce::ComboBox>();
    panModeBox->addItemList({ "Off", "Alternate", "Step" }, 1);
    panModeBox->setTooltip("Alternate notes left/right, or step L-C-R on the beat");
    addAndMakeVisible(*panModeBox);
    panModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), PanMode, *panModeBox);
    panRateBox = std::make_unique<juce::ComboBox>();
    panRateBox->addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/2T", "1/4T", "1/8T" }, 1);
    panRateBox->setTooltip("Step length");
    addAndMakeVisible(*panRateBox);
    panRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), PanRate, *panRateBox);
    pan.boxes.push_back(panModeBox.get());
    pan.boxes.push_back(panRateBox.get());
    
    auto& echo = addGroup("Echo");
    addKnob(echo, EchoLevel, "Level", UiTheme::carrier);
    addKnob(echo, EchoTime, "Time", UiTheme::carrier, true, milliseconds);
    
    auto& sweep = addGroup("Sweep", 1);
    addKnob(sweep, SweepAmount, "Amt", UiTheme::amber, false, [](double v) {
        const int i = juce::roundToInt(v);
        return i == 0 ? juce::String("0") : (i > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(i)); });
    addKnob(sweep, SweepTime, "Time", UiTheme::amber, false, [](double v) { return juce::String(v / 1000.0, 1); });
    
    auto& pitch = addGroup("Pitch", 1);
    addKnob(pitch, PitchEnv, "Env", UiTheme::carrier, false, [](double v) {
        const int i = juce::roundToInt(v);
        return i == 0 ? juce::String("0") : (i > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(i)); });
    addKnob(pitch, PitchTime, "Time", UiTheme::carrier, false, milliseconds);
    
    auto& glide = addGroup("Glide", 1);
    monoButton = std::make_unique<juce::ToggleButton>("Legato");
    monoButton->setTooltip("Mono: a new note retunes the sounding one instead of starting another");
    addAndMakeVisible(*monoButton);
    monoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), Mono, *monoButton);
    glide.toggles.push_back(monoButton.get());
    addKnob(glide, PortaTime, "Porta", UiTheme::green, false, milliseconds);
    
    auto& velocity = addGroup("Vel", 1);
    addKnob(velocity, VelBright, "Bright", UiTheme::amber);
    
    auto& arpeggio = addGroup("Arpeggio", 1);
    arpModeBox = std::make_unique<juce::ComboBox>();
    arpModeBox->addItemList({ "Off", "Up", "Down", "Up Down" }, 1);
    arpModeBox->setTooltip("Held notes take turns on one channel");
    addAndMakeVisible(*arpModeBox);
    arpModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), ArpMode, *arpModeBox);
    arpDivBox = std::make_unique<juce::ComboBox>();
    arpDivBox->addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/2T", "1/4T", "1/8T", "1/32", "1/64", "1/16T" }, 1);
    arpDivBox->setTooltip("Step length");
    addAndMakeVisible(*arpDivBox);
    arpDivAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), ArpDiv, *arpDivBox);
    arpeggio.boxes.push_back(arpModeBox.get());
    arpeggio.boxes.push_back(arpDivBox.get());
    
    syncButton = std::make_unique<juce::ToggleButton>("Sync");
    syncButton->setTooltip("Rates follow the host tempo (note values are chosen in the Quick view)");
    syncButton->onClick = [this]() { updateRateKnobs(); };
    addAndMakeVisible(*syncButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), Sync, *syncButton);
    updateRateKnobs();
}

MotionStrip::Group& MotionStrip::addGroup(const juce::String& title, int row)
{
    groups.push_back(Group {});
    groups.back().title = title;
    groups.back().row = row;
    return groups.back();
}

void MotionStrip::addKnob(Group& group, const char* parameterId, const juce::String& label, juce::Colour accent, bool isRate,
                          std::function<juce::String(double)> formatter)
{
    Knob k;
    k.parameterId = parameterId;
    k.isRate = isRate;
    k.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Tiny);
    k.knob->setAccentColour(accent);
    if (formatter) k.knob->setValueFormatter(std::move(formatter));
    addAndMakeVisible(*k.knob);
    k.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *k.knob, *this);
    group.knobs.push_back(std::move(k));
}

void MotionStrip::updateRateKnobs()
{
    // With sync on the rates come from note values; the Hz knobs are shown greyed
    const bool synced = syncButton->getToggleState();
    for (auto& g : groups)
        for (auto& k : g.knobs)
            if (k.isRate) k.knob->setEnabled(!synced);
}

void MotionStrip::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::strip);
    g.setColour(UiTheme::borderSoft);
    g.fillRect(0, 0, getWidth(), 1);
    g.setFont(UiTheme::mono(9.0f));
    for (size_t i = 0; i < groups.size(); ++i) {
        const auto& group = groups[i];
        g.setColour(UiTheme::dim);
        g.drawText(group.title, group.bounds.withHeight(12), juce::Justification::centredLeft);
        if (i > 0 && groups[i - 1].row == group.row) {
            g.setColour(UiTheme::border);
            g.fillRect(group.bounds.getX() - kGroupGap / 2, group.bounds.getY() + 8, 1, group.bounds.getHeight() - 12);
        }
    }
}

void MotionStrip::resized()
{
    auto bounds = getLocalBounds().reduced(20, 0);
    auto labelColumn = bounds.removeFromLeft(58);
    sectionLabel->setBounds(labelColumn.withHeight(20).withCentre({ labelColumn.getCentreX(), bounds.getCentreY() - 12 }));
    syncButton->setBounds(labelColumn.withHeight(20).withCentre({ labelColumn.getCentreX() + 4, bounds.getCentreY() + 12 }));
    bounds.removeFromLeft(6);
    const auto knobSize = RotaryKnob::preferredSize(RotaryKnob::Style::Tiny, RotaryKnob::LabelPosition::Below, false, kKnobWidth);
    const int rowHeight = bounds.getHeight() / 2;
    
    std::array<int, 2> x { bounds.getX(), bounds.getX() };
    for (auto& group : groups) {
        const int row = juce::jlimit(0, 1, group.row);
        const int top = bounds.getY() + row * rowHeight;
        const int centreY = top + rowHeight / 2 + 5;
        const int start = x[static_cast<size_t>(row)];
        int& cx = x[static_cast<size_t>(row)];
        for (auto* toggle : group.toggles) {
            toggle->setBounds(cx, centreY - 10, 78, 20);
            cx += 78 + kKnobGap;
        }
        for (auto& k : group.knobs) {
            k.knob->setBounds(cx, centreY - knobSize.getHeight() / 2, knobSize.getWidth(), knobSize.getHeight());
            cx += knobSize.getWidth() + kKnobGap;
        }
        for (auto* box : group.boxes) {
            box->setBounds(cx, centreY - 10, 60, 22);
            cx += 60 + kKnobGap;
        }
        group.bounds = juce::Rectangle<int>(start, top + 2, cx - start - kKnobGap, rowHeight - 2);
        cx += kGroupGap;
    }
}
