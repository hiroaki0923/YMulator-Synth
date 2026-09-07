#include "MotionStrip.h"
#include "ArpSettingsPanel.h"
#include <array>
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../utils/ParameterIDs.h"

namespace {
constexpr int kThemeGap = 8;
constexpr int kGroupGap = 6;
constexpr int kKnobGap = 3;
constexpr int kTitleHeight = 13;
constexpr int kDivSlotWidth = 56;    // a note-value box needs "1/16T" plus the arrow to fit
constexpr int kBoxWidth = 56;
constexpr int kWaveBoxWidth = 48;
constexpr int kLegatoWidth = 70;
constexpr int kOneShotWidth = 68;
constexpr int kSmallButtonWidth = 28;
juce::String oneDecimal(double v) { return juce::String(v, 1); }
juce::String milliseconds(double v) { return juce::String(juce::roundToInt(v)); }
juce::String signedInt(double v)
{
    const int i = juce::roundToInt(v);
    return i == 0 ? juce::String("0") : (i > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(i));
}
const juce::StringArray kDivisions { "1/1", "1/2", "1/4", "1/8", "1/16", "1/2T", "1/4T", "1/8T", "1/32", "1/64", "1/16T" };
const juce::StringArray kWaves { "Sin", "Tri", "Saw", "Sqr", "Rnd" };
}

MotionStrip::MotionStrip(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    sectionLabel = std::make_unique<juce::Label>("", "MOTION");
    sectionLabel->setFont(UiTheme::mono(11.0f, true));
    sectionLabel->setColour(juce::Label::textColourId, UiTheme::green);
    addAndMakeVisible(*sectionLabel);
    
    syncButton = std::make_unique<juce::ToggleButton>("Sync");
    syncButton->setTooltip("Rates follow the host tempo: the rate knobs turn into note values");
    syncButton->onClick = [this]() { updateRateKnobs(); };
    addAndMakeVisible(*syncButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), ParamID::Motion::Sync, *syncButton);
    
    buildLayout();
    updateRateKnobs();
}

void MotionStrip::buildLayout()
{
    using namespace ParamID::Motion;
    // The building blocks; each layout below places them in themes and rows
    auto vibrato = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "vibrato");
        addKnob(g, VibratoDepth, "Depth", UiTheme::green);
        addKnob(g, VibratoRate, "Rate", UiTheme::green, true, oneDecimal, VibratoDiv);
        addKnob(g, VibratoDelay, "Delay", UiTheme::green, false, milliseconds);
        addKnob(g, VibratoRise, "Rise", UiTheme::green, false, milliseconds);
    };
    auto vibWave = [&](int theme, int row) {
        auto& g = addGroup(theme, row);
        addBox(g, vibWaveBox, vibWaveAttachment, VibratoWave, kWaves, "Vibrato wave");
    };
    auto oneShot = [&](int theme, int row) {
        auto& g = addGroup(theme, row);
        addToggle(g, oneShotButton, oneShotAttachment, LfoOneShot, "1shot", "Vibrato and timbre LFO run one cycle and stop");
    };
    auto pitchEnv = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "pitch");
        addKnob(g, PitchEnv, "Env", UiTheme::carrier, false, signedInt);
        addKnob(g, PitchTime, "Time", UiTheme::carrier, false, milliseconds);
        addKnob(g, PitchEnv2, "Env2", UiTheme::carrier, false, signedInt);
        addKnob(g, PitchTime2, "Time2", UiTheme::carrier, false, milliseconds);
    };
    auto timbre = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "timbre");
        addKnob(g, TimbreDepth, "Timbre", UiTheme::amber);
        addKnob(g, TimbreRate, "Rate", UiTheme::amber, true, oneDecimal, TimbreDiv);
        addBox(g, timbreWaveBox, timbreWaveAttachment, TimbreWave, kWaves, "Timbre LFO wave");
    };
    auto sweep = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "sweep");
        addKnob(g, SweepAmount, "Sweep", UiTheme::amber, false, signedInt);
        addKnob(g, SweepTime, "Time", UiTheme::amber, false, UiTheme::formatTime);
    };
    auto velBright = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "velocity");
        addKnob(g, VelBright, "Vel", UiTheme::amber);
    };
    auto tremolo = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "tremolo");
        addKnob(g, TremoloDepth, "Trem", UiTheme::carrier);
        addKnob(g, TremoloRate, "Rate", UiTheme::carrier, true, oneDecimal, TremoloDiv);
    };
    auto levelEg = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "level");
        addKnob(g, LevelAttack, "Atk", UiTheme::green, false, UiTheme::formatTime);
        addKnob(g, LevelDecay, "Dec", UiTheme::green, false, UiTheme::formatTime);
        addKnob(g, LevelSustain, "Sus", UiTheme::green);
    };
    auto wide = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "wide");
        addKnob(g, Wide, "Wide", UiTheme::carrier);
        addBox(g, widePanBox, widePanAttachment, WidePan, { "L / R", "Center" }, "Where the two chips sit");
    };
    auto echo = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "echo");
        addKnob(g, EchoLevel, "Echo", UiTheme::carrier);
        addKnob(g, EchoTime, "Time", UiTheme::carrier, true, milliseconds, EchoDiv);
    };
    auto pan = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "pan");
        addBox(g, panModeBox, panModeAttachment, PanMode, { "Off", "Alt", "Step", "Left", "Right", "Rand" }, "Where the voices sit: centre, alternating left/right per note, stepping L-C-R on the beat, left, right, or a random side per note");
        addBox(g, panRateBox, panRateAttachment, PanRate, { "1/1", "1/2", "1/4", "1/8", "1/16", "1/2T", "1/4T", "1/8T" }, "Step length");
    };
    auto glide = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "glide");
        addToggle(g, monoButton, monoAttachment, Mono, "Legato", "Mono: a new note retunes the sounding one instead of starting another");
        addKnob(g, PortaTime, "Porta", UiTheme::green, false, milliseconds);
    };
    auto arp = [&](int theme, int row) {
        auto& g = addGroup(theme, row, "arpeggio");
        addBox(g, arpModeBox, arpModeAttachment, ArpMode, { "Off", "Up", "Down", "UpDn", "Rnd", "Order" }, "Held notes take turns on one channel");
        addBox(g, arpDivBox, arpDivAttachment, ArpDiv, kDivisions, "Step length");
        arpSettingsButton = std::make_unique<juce::TextButton>(juce::String(juce::CharPointer_UTF8("\xe2\x80\xa6")));
        arpSettingsButton->setTooltip("Chord table, octaves, retrigger and gate, latch, accent");
        arpSettingsButton->onClick = [this]() {
            auto panel = std::make_unique<ArpSettingsPanel>(audioProcessor);
            panel->setLookAndFeel(&getLookAndFeel());   // the callout is a top-level window and would fall back to the default look
            juce::CallOutBox::launchAsynchronously(std::move(panel), arpSettingsButton->getScreenBounds(), nullptr);
        };
        addAndMakeVisible(*arpSettingsButton);
        g.buttons.push_back(arpSettingsButton.get());
    };
    
    // By how it moves: the LFOs (which Sync turns into note values) on top, the per-note envelopes below,
    // then the stereo tricks and the playing aids
    int t = addTheme("LFO");
    vibrato(t, 0); vibWave(t, 0); oneShot(t, 0);
    timbre(t, 1); tremolo(t, 1);
    t = addTheme("ENVELOPE");
    pitchEnv(t, 0);
    sweep(t, 1); levelEg(t, 1);
    t = addTheme("SPACE");
    wide(t, 0); echo(t, 0);
    pan(t, 1);
    t = addTheme("PLAY");
    glide(t, 0); velBright(t, 0);
    arp(t, 1);
}

int MotionStrip::addTheme(const juce::String& title)
{
    themes.push_back(Theme { title, {}, 0 });
    return static_cast<int>(themes.size()) - 1;
}

MotionStrip::Group& MotionStrip::addGroup(int theme, int row, const juce::String& caption)
{
    groups.push_back(Group {});
    groups.back().theme = theme;
    groups.back().row = row;
    groups.back().caption = caption;
    return groups.back();
}

void MotionStrip::addKnob(Group& group, const char* parameterId, const juce::String& label, juce::Colour accent, bool isRate,
                          std::function<juce::String(double)> formatter, const char* divParameterId)
{
    Knob k;
    k.parameterId = parameterId;
    k.isRate = isRate;
    k.knob = std::make_unique<RotaryKnob>(label, knobStyle);
    k.knob->setAccentColour(accent);
    if (formatter) k.knob->setValueFormatter(std::move(formatter));
    addAndMakeVisible(*k.knob);
    k.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *k.knob, *this);
    if (divParameterId != nullptr) {
        k.divBox = std::make_unique<juce::ComboBox>();
        k.divBox->addItemList(kDivisions, 1);
        k.divBox->setTooltip(label + " as a note value while Sync is on");
        addChildComponent(*k.divBox);
        k.divAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), divParameterId, *k.divBox);
    }
    group.knobs.push_back(std::move(k));
}

juce::ComboBox* MotionStrip::addBox(Group& group, std::unique_ptr<juce::ComboBox>& box,
                                    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment,
                                    const char* parameterId, const juce::StringArray& items, const juce::String& tooltip)
{
    box = std::make_unique<juce::ComboBox>();
    box->addItemList(items, 1);
    box->setTooltip(tooltip);
    addAndMakeVisible(*box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getParameters(), parameterId, *box);
    group.boxes.push_back(box.get());
    return box.get();
}

juce::ToggleButton* MotionStrip::addToggle(Group& group, std::unique_ptr<juce::ToggleButton>& button,
                                           std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment,
                                           const char* parameterId, const juce::String& text, const juce::String& tooltip)
{
    button = std::make_unique<juce::ToggleButton>(text);
    button->setTooltip(tooltip);
    addAndMakeVisible(*button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.getParameters(), parameterId, *button);
    group.toggles.push_back(button.get());
    return button.get();
}

void MotionStrip::updateRateKnobs()
{
    // With sync on the rates come from note values: each rate knob gives its place to a note-value box
    const bool synced = syncButton->getToggleState();
    for (auto& g : groups)
        for (auto& k : g.knobs) {
            if (!k.isRate) continue;
            const bool hasBox = k.divBox != nullptr;
            k.knob->setVisible(!(synced && hasBox));
            k.knob->setEnabled(!synced);
            if (hasBox) k.divBox->setVisible(synced);
        }
}

int MotionStrip::knobSlot(const Knob& knob) const
{
    const int knobWidth = RotaryKnob::preferredSize(knobStyle, RotaryKnob::LabelPosition::Below, false, 36).getWidth();
    return knob.divBox ? juce::jmax(kDivSlotWidth, knobWidth) : knobWidth;
}

int MotionStrip::widthOf(const Group& group) const
{
    int width = 0;
    for (auto* toggle : group.toggles) width += (toggle == monoButton.get() ? kLegatoWidth : kOneShotWidth) + kKnobGap;
    for (const auto& k : group.knobs) width += knobSlot(k) + kKnobGap;
    for (auto* box : group.boxes) width += (box == vibWaveBox.get() || box == timbreWaveBox.get() ? kWaveBoxWidth : kBoxWidth) + kKnobGap;
    for (size_t i = 0; i < group.buttons.size(); ++i) width += kSmallButtonWidth + kKnobGap;
    return juce::jmax(0, width - kKnobGap);
}

int MotionStrip::preferredHeight() const
{
    const auto knobSize = RotaryKnob::preferredSize(knobStyle, RotaryKnob::LabelPosition::Below, false, 36);
    return 6 + kTitleHeight + 2 * (knobSize.getHeight() + 2) + kTitleHeight + 4;
}

void MotionStrip::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::strip);
    g.setColour(UiTheme::borderSoft);
    g.fillRect(0, 0, getWidth(), 1);
    for (size_t i = 0; i < themes.size(); ++i) {
        const auto& theme = themes[i];
        g.setFont(UiTheme::mono(9.0f, true));
        g.setColour(UiTheme::dim);
        g.drawText(theme.title, theme.bounds.withHeight(kTitleHeight).withWidth(theme.titleWidth), juce::Justification::centredLeft);
        if (i > 0) {
            g.setColour(UiTheme::border);
            g.fillRect(theme.bounds.getX() - kThemeGap / 2, theme.bounds.getY() + 2, 1, theme.bounds.getHeight() - 4);
        }
    }
    // Each group's caption sits above its own controls: on the title line for the upper row (after the title),
    // between the rows for the lower row
    g.setFont(UiTheme::mono(9.0f));
    g.setColour(UiTheme::dim.withAlpha(0.7f));
    for (const auto& group : groups) {
        if (group.caption.isEmpty()) continue;
        const auto& theme = themes[static_cast<size_t>(group.theme)];
        const int y = group.row == 0 ? theme.bounds.getY() : lowerCaptionTop;
        const int left = group.row == 0 ? theme.bounds.getX() + theme.titleWidth : theme.bounds.getX();
        // A caption wider than its group (velocity over one knob) slides left so it still ends inside the card
        const int width = juce::roundToInt(g.getCurrentFont().getStringWidthFloat(group.caption)) + 2;
        const int x = juce::jmax(left, juce::jmin(group.bounds.getX(), theme.bounds.getRight() - width));
        g.drawText(group.caption, x, y, theme.bounds.getRight() - x, kTitleHeight, juce::Justification::centredLeft, true);
    }
}

void MotionStrip::resized()
{
    auto bounds = getLocalBounds().reduced(20, 0);
    auto labelColumn = bounds.removeFromLeft(58);
    sectionLabel->setBounds(labelColumn.withHeight(20).withCentre({ labelColumn.getCentreX(), bounds.getCentreY() - 12 }));
    syncButton->setBounds(labelColumn.withHeight(20).withCentre({ labelColumn.getCentreX() + 4, bounds.getCentreY() + 12 }));
    bounds.removeFromLeft(6);
    const auto knobSize = RotaryKnob::preferredSize(knobStyle, RotaryKnob::LabelPosition::Below, false, 36);
    const int rowHeight = knobSize.getHeight() + 2;
    const int top = bounds.getY() + 6 + kTitleHeight;
    lowerCaptionTop = top + rowHeight;
    
    int x = bounds.getX();
    for (size_t t = 0; t < themes.size(); ++t) {
        std::array<int, 2> rowWidth { 0, 0 };
        for (const auto& g : groups)
            if (g.theme == static_cast<int>(t))
                rowWidth[static_cast<size_t>(juce::jlimit(0, 1, g.row))] += widthOf(g) + kGroupGap;
        const int themeWidth = juce::jmax(rowWidth[0], rowWidth[1]) - kGroupGap;
        std::array<int, 2> cx { x, x };
        for (auto& g : groups) {
            if (g.theme != static_cast<int>(t)) continue;
            const int row = juce::jlimit(0, 1, g.row);
            const int rowTop = row == 0 ? top : lowerCaptionTop + kTitleHeight;
            const int centreY = rowTop + knobSize.getHeight() / 2 + 1;
            int& gx = cx[static_cast<size_t>(row)];
            const int start = gx;
            for (auto* toggle : g.toggles) {
                const int width = toggle == monoButton.get() ? kLegatoWidth : kOneShotWidth;
                toggle->setBounds(gx, centreY - 10, width, 20);
                gx += width + kKnobGap;
            }
            for (auto& k : g.knobs) {
                const int slot = knobSlot(k);
                k.knob->setBounds(gx + (slot - knobSize.getWidth()) / 2, rowTop, knobSize.getWidth(), knobSize.getHeight());
                if (k.divBox) k.divBox->setBounds(gx, centreY - 11, slot, 22);
                gx += slot + kKnobGap;
            }
            for (auto* box : g.boxes) {
                const int width = box == vibWaveBox.get() || box == timbreWaveBox.get() ? kWaveBoxWidth : kBoxWidth;
                box->setBounds(gx, centreY - 11, width, 22);
                gx += width + kKnobGap;
            }
            for (auto* button : g.buttons) {
                button->setBounds(gx, centreY - 11, kSmallButtonWidth, 22);
                gx += kSmallButtonWidth + kKnobGap;
            }
            g.bounds = juce::Rectangle<int>(start, rowTop, gx - start - kKnobGap, rowHeight);
            gx += kGroupGap - kKnobGap;
        }
        themes[t].bounds = juce::Rectangle<int>(x, bounds.getY() + 4, themeWidth, getHeight() - 8);
        themes[t].titleWidth = juce::roundToInt(UiTheme::mono(9.0f, true).getStringWidthFloat(themes[t].title)) + 6;
        x += themeWidth + kThemeGap;
    }
}
