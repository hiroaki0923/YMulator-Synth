#include "OperatorPanel.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../core/MacroMapper.h"
#include "../utils/ParameterIDs.h"
#include "../utils/Debug.h"

namespace {
constexpr int kHeaderWidth = 96;
constexpr int kGap = 12;
constexpr int kPrimaryGap = 10;
constexpr int kTinyGap = 8;
constexpr int kEnvelopeWidth = 150;
constexpr int kEnvelopeHeight = 58;
constexpr int kAmsWidth = 34;

juce::String levelText(double tl) { return juce::String(juce::roundToInt((127.0 - tl) * 100.0 / 127.0)); }
juce::String ratioText(double mul) { return mul < 0.5 ? juce::String(juce::CharPointer_UTF8("\xc3\x97" "0.5")) : juce::String(juce::CharPointer_UTF8("\xc3\x97")) + juce::String(juce::roundToInt(mul)); }
juce::String detuneText(double dt1)
{
    const int signedValue = ymulatorsynth::MacroMapper::decodeDetune1(juce::roundToInt(dt1));
    if (signedValue == 0) return "0";
    return (signedValue > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(signedValue));
}
}

const std::vector<OperatorPanel::ControlSpec> OperatorPanel::controlSpecs = {
    { ParamID::Op::TotalLevel,   "Level",  RotaryKnob::Style::Primary, 0 },
    { ParamID::Op::Multiple,     "Ratio",  RotaryKnob::Style::Primary, 0 },
    { ParamID::Op::Detune1,      "Detune", RotaryKnob::Style::Primary, 0 },
    { ParamID::Op::AttackRate,   "AR",     RotaryKnob::Style::Tiny,    1 },
    { ParamID::Op::Decay1Rate,   "D1R",    RotaryKnob::Style::Tiny,    1 },
    { ParamID::Op::SustainLevel, "D1L",    RotaryKnob::Style::Tiny,    1 },
    { ParamID::Op::Decay2Rate,   "D2R",    RotaryKnob::Style::Tiny,    1 },
    { ParamID::Op::ReleaseRate,  "RR",     RotaryKnob::Style::Tiny,    1 },
    { ParamID::Op::KeyScale,     "KS",     RotaryKnob::Style::Tiny,    2 },
    { ParamID::Op::Detune2,      "DT2",    RotaryKnob::Style::Tiny,    2 },
};

OperatorPanel::OperatorPanel(YMulatorSynthAudioProcessor& processor, int operatorNumber)
    : audioProcessor(processor), operatorNum(operatorNumber)
{
    CS_ASSERT_OPERATOR(operatorNumber - 1);
    operatorId = "op" + juce::String(operatorNumber);
    
    slotEnableButton = std::make_unique<juce::ToggleButton>();
    slotEnableButton->setTooltip("Operator on/off");
    addAndMakeVisible(*slotEnableButton);
    slotEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Op::slot_en(operatorNum), *slotEnableButton);
    
    for (const auto& spec : controlSpecs) createControl(spec);
    
    amsEnableButton = std::make_unique<juce::ToggleButton>();
    amsEnableButton->setTooltip("Amplitude modulation from the LFO");
    addAndMakeVisible(*amsEnableButton);
    amsEnableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getParameters(), ParamID::Op::ams_en(operatorNum), *amsEnableButton);
    
    envelopeDisplay = std::make_unique<EnvelopeDisplay>();
    addAndMakeVisible(*envelopeDisplay);
    
    applyRoleColours();
    updateSubLabels();
    updateEnvelopeDisplay();
}

void OperatorPanel::createControl(const ControlSpec& spec)
{
    Control control;
    control.spec = spec;
    control.knob = std::make_unique<RotaryKnob>(spec.label, spec.style);
    
    const juce::String suffix(spec.suffix);
    if (suffix == ParamID::Op::TotalLevel) {
        control.knob->setInverted(true);
        control.knob->setValueFormatter(levelText);
    } else if (suffix == ParamID::Op::Multiple) {
        control.knob->setValueFormatter(ratioText);
    } else if (suffix == ParamID::Op::Detune1) {
        control.knob->setValueFormatter(detuneText);
    }
    addAndMakeVisible(*control.knob);
    
    control.binding = KnobBinding::attach(audioProcessor.getParameters(), operatorId + suffix, *control.knob, *this,
                                          [this]() { updateEnvelopeDisplay(); updateSubLabels(); });
    controls.push_back(std::move(control));
}

juce::Colour OperatorPanel::roleColour() const
{
    return role == Role::Modulator ? UiTheme::modulator : UiTheme::carrier;
}

void OperatorPanel::applyRoleColours()
{
    for (auto& c : controls) {
        if (c.spec.group == 0) c.knob->setAccentColour(roleColour());
        else if (c.spec.group == 1) c.knob->setAccentColour(UiTheme::green);
        else c.knob->setAccentColour(UiTheme::carrier);
    }
    if (envelopeDisplay) envelopeDisplay->setLineColour(role == Role::Modulator ? UiTheme::green : UiTheme::carrier);
}

void OperatorPanel::setRole(Role newRole, const juce::String& hint)
{
    if (role == newRole && roleHint == hint) return;
    role = newRole;
    roleHint = hint;
    applyRoleColours();
    repaint();
}

void OperatorPanel::setHighlightedParameters(const std::set<std::string>& parameterIds)
{
    for (auto& c : controls)
        c.knob->setHighlighted(parameterIds.count((operatorId + c.spec.suffix).toStdString()) > 0);
}

std::vector<std::string> OperatorPanel::highlightedParameterIds() const
{
    std::vector<std::string> ids;
    for (const auto& c : controls)
        if (c.knob->isHighlighted()) ids.push_back((operatorId + c.spec.suffix).toStdString());
    return ids;
}

void OperatorPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(UiTheme::panel);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(role == Role::Modulator ? UiTheme::border : UiTheme::carrierEdge);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    
    auto header = getLocalBounds().reduced(14, 0).removeFromLeft(kHeaderWidth).toFloat();
    const float top = bounds.getCentreY() - 30.0f;
    
    g.setColour(UiTheme::text);
    g.setFont(UiTheme::mono(13.0f, true));
    g.drawText("OP " + juce::String(operatorNum), header.withY(top).withHeight(20.0f).withWidth(40.0f), juce::Justification::centredLeft);
    
    // The role tag spans from the OP label to the right edge of the on/off toggle above it
    const juce::String tagText = role == Role::Modulator ? "MODULATOR" : (role == Role::Carrier ? "CARRIER" : "NOISE");
    const auto tagColour = role == Role::Noise ? UiTheme::amber : roleColour();
    g.setFont(UiTheme::mono(9.0f, true));
    auto tag = juce::Rectangle<float>(header.getX(), top + 24.0f, static_cast<float>(slotEnableButton->getRight()) - header.getX(), 13.0f);
    g.setColour(tagColour);
    g.fillRoundedRectangle(tag, 2.0f);
    g.setColour(UiTheme::dark);
    g.drawText(tagText, tag, juce::Justification::centred);
    
    g.setColour(UiTheme::muted);
    g.setFont(UiTheme::mono(9.0f));
    g.drawFittedText(roleHint, header.withY(top + 41.0f).withHeight(22.0f).toNearestInt(), juce::Justification::topLeft, 2);
    
    g.setColour(UiTheme::border);
    for (int x : separatorXs)
        g.fillRect(static_cast<float>(x), bounds.getCentreY() - 30.0f, 1.0f, 60.0f);
}

void OperatorPanel::resized()
{
    auto bounds = getLocalBounds().reduced(14, 0);
    const int centreY = bounds.getCentreY();
    auto place = [&](juce::Component& c, int x, const juce::Rectangle<int>& size) {
        c.setBounds(x, centreY - size.getHeight() / 2, size.getWidth(), size.getHeight());
        return x + size.getWidth();
    };
    
    int x = bounds.getX();
    slotEnableButton->setBounds(x + 44, centreY - 30, 32, 18);
    x += kHeaderWidth + kGap;
    
    for (auto& c : controls) {
        if (c.spec.group != 0) continue;
        x = place(*c.knob, x, RotaryKnob::preferredSize(c.spec.style, RotaryKnob::LabelPosition::Below, true, 52)) + kPrimaryGap;
    }
    separatorXs.clear();
    separatorXs.push_back(x + kGap / 2);
    x += kGap + 1;
    
    envelopeDisplay->setBounds(x, centreY - kEnvelopeHeight / 2, kEnvelopeWidth, kEnvelopeHeight);
    x += kEnvelopeWidth + kGap;
    
    for (auto& c : controls) {
        if (c.spec.group != 1) continue;
        x = place(*c.knob, x, RotaryKnob::preferredSize(c.spec.style, RotaryKnob::LabelPosition::Below, false, 36)) + kTinyGap;
    }
    separatorXs.push_back(x + kGap / 2);
    x += kGap + 1;
    
    for (auto& c : controls) {
        if (c.spec.group != 2) continue;
        x = place(*c.knob, x, RotaryKnob::preferredSize(c.spec.style, RotaryKnob::LabelPosition::Below, false, 36)) + kTinyGap;
    }
    amsEnableButton->setButtonText("AMS");
    amsEnableButton->setBounds(x, centreY - 9, kAmsWidth + 30, 18);
}

void OperatorPanel::updateEnvelopeDisplay()
{
    if (!envelopeDisplay) return;
    int tl = 0, ar = 31, d1r = 0, d1l = 0, d2r = 0, rr = 7;
    for (const auto& c : controls) {
        const juce::String suffix(c.spec.suffix);
        const int v = juce::roundToInt(c.knob->getValue());
        if (suffix == ParamID::Op::TotalLevel) tl = v;
        else if (suffix == ParamID::Op::AttackRate) ar = v;
        else if (suffix == ParamID::Op::Decay1Rate) d1r = v;
        else if (suffix == ParamID::Op::SustainLevel) d1l = v;
        else if (suffix == ParamID::Op::Decay2Rate) d2r = v;
        else if (suffix == ParamID::Op::ReleaseRate) rr = v;
    }
    envelopeDisplay->setYM2151Parameters(tl, ar, d1r, d1l, d2r, rr);
}

void OperatorPanel::updateSubLabels()
{
    for (auto& c : controls) {
        if (c.spec.group != 0) continue;
        const juce::String suffix(c.spec.suffix);
        const int v = juce::roundToInt(c.knob->getValue());
        if (suffix == ParamID::Op::TotalLevel) c.knob->setSubLabel("TL " + juce::String(v));
        else if (suffix == ParamID::Op::Multiple) c.knob->setSubLabel("MUL " + juce::String(v));
        else if (suffix == ParamID::Op::Detune1) c.knob->setSubLabel("DT1 " + juce::String(v));
    }
}
