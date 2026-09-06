#include "QuickView.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../dsp/AlgorithmInfo.h"
#include "../utils/ParameterIDs.h"
#include "../core/PatchWorkspace.h"

namespace {
constexpr int kSideWidth = 250;
constexpr int kGap = 14;
constexpr int kFooterHeight = 30;
const char* const kHarmonicsNames[] = { "Preset", "Saw", "Square", "Pulse", "Bright", "Bell", "Metal", "Sub", "Oct" };

juce::String signedText(double v)
{
    const int i = juce::roundToInt(v);
    if (i == 0) return "0";
    return (i > 0 ? "+" : juce::String(juce::CharPointer_UTF8("\xe2\x88\x92"))) + juce::String(std::abs(i));
}

juce::String plainText(double v) { return juce::String(juce::roundToInt(v)); }

juce::String harmonicsText(double v) { return juce::String(kHarmonicsNames[juce::jlimit(0, 8, juce::roundToInt(v))]); }

const juce::String kDot = juce::String(juce::CharPointer_UTF8(" \xc2\xb7 "));
}

// ============================================================================
// Card
// ============================================================================

QuickView::Card::Card(const juce::String& t, const juce::String& n) : title(t), note(n) {}

void QuickView::Card::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(UiTheme::panel);
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(UiTheme::border);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);
    
    g.setColour(UiTheme::green);
    g.setFont(UiTheme::mono(11.0f, true));
    g.drawText(title, 12, 8, getWidth() - 24, 16, juce::Justification::centredLeft);
    if (note.isNotEmpty()) {
        g.setColour(UiTheme::dim);
        g.setFont(UiTheme::sans(11.0f));
        g.drawFittedText(note, bodyBounds(), juce::Justification::centred, 3);
    }
}

juce::Rectangle<int> QuickView::Card::bodyBounds() const
{
    return getLocalBounds().reduced(12, 8).withTrimmedTop(24);
}

juce::Rectangle<int> QuickView::Card::headerBounds() const
{
    return getLocalBounds().reduced(12, 8).withHeight(20);
}

int QuickView::Card::titleWidth() const
{
    return juce::roundToInt(UiTheme::mono(11.0f, true).getStringWidthFloat(title)) + 12;
}

// ============================================================================
// QuickView
// ============================================================================

QuickView::QuickView(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    toneLabel = std::make_unique<juce::Label>("", "TONE");
    toneLabel->setFont(UiTheme::mono(11.0f, true));
    toneLabel->setColour(juce::Label::textColourId, UiTheme::green);
    addAndMakeVisible(*toneLabel);
    toneNote = std::make_unique<juce::Label>("", "Shapes whatever is loaded now, a preset or a generated sound, around its character. Loading or generating recentres the knobs.");
    toneNote->setFont(UiTheme::sans(11.0f));
    toneNote->setColour(juce::Label::textColourId, UiTheme::muted);
    addAndMakeVisible(*toneNote);
    
    addMacroKnob(ParamID::Macro::Brightness, "Brightness", "Modulator level", UiTheme::amber, signedText);
    addMacroKnob(ParamID::Macro::Harmonics, "Harmonics", "Ratio template", UiTheme::amber, harmonicsText);
    addMacroKnob(ParamID::Global::Feedback, "Feedback", "Op1 FB", UiTheme::amber, plainText);
    addMacroKnob(ParamID::Macro::Attack, "Attack", "All AR", UiTheme::green, signedText);
    addMacroKnob(ParamID::Macro::Decay, "Decay", "D1R / D2R", UiTheme::green, signedText);
    addMacroKnob(ParamID::Macro::Release, "Release", "All RR", UiTheme::green, signedText);
    addMacroKnob(ParamID::Macro::Spread, "Spread", "DT1 spread", UiTheme::carrier, signedText);
    
    algorithmCard = std::make_unique<Card>("ALGORITHM", "");
    addAndMakeVisible(*algorithmCard);
    algorithmDisplay = std::make_unique<AlgorithmDisplay>();
    algorithmCard->addAndMakeVisible(*algorithmDisplay);
    algorithmDescription = std::make_unique<juce::Label>("", "");
    algorithmDescription->setFont(UiTheme::sans(12.0f));
    algorithmDescription->setColour(juce::Label::textColourId, UiTheme::muted);
    algorithmDescription->setJustificationType(juce::Justification::topLeft);
    algorithmCard->addAndMakeVisible(*algorithmDescription);
    algorithmCaption = std::make_unique<juce::Label>("", "");
    algorithmCaption->setFont(UiTheme::mono(9.0f));
    algorithmCaption->setColour(juce::Label::textColourId, UiTheme::dim);
    algorithmCard->addAndMakeVisible(*algorithmCaption);
    previousAlgorithmButton = std::make_unique<juce::TextButton>(juce::String(juce::CharPointer_UTF8("\xe2\x80\xb9")));
    previousAlgorithmButton->setTooltip("Previous algorithm");
    previousAlgorithmButton->onClick = [this]() { stepAlgorithm(-1); };
    algorithmCard->addAndMakeVisible(*previousAlgorithmButton);
    nextAlgorithmButton = std::make_unique<juce::TextButton>(juce::String(juce::CharPointer_UTF8("\xe2\x80\xba")));
    nextAlgorithmButton->setTooltip("Next algorithm");
    nextAlgorithmButton->onClick = [this]() { stepAlgorithm(1); };
    algorithmCard->addAndMakeVisible(*nextAlgorithmButton);
    
    generateCard = std::make_unique<Card>("RECIPE", "");
    addAndMakeVisible(*generateCard);
    generateNote = std::make_unique<juce::Label>("", "Pick a category and a direction, then Generate. It replaces the current sound; Undo brings it back.");
    generateNote->setFont(UiTheme::sans(11.0f));
    generateNote->setColour(juce::Label::textColourId, UiTheme::muted);
    generateCard->addAndMakeVisible(*generateNote);
    generatorPanel = std::make_unique<GeneratorPanel>(processor);
    generateCard->addAndMakeVisible(*generatorPanel);
    newSoundButton = std::make_unique<juce::TextButton>("Generate");
    newSoundButton->getProperties().set("accent", true);
    newSoundButton->setColour(juce::TextButton::textColourOffId, UiTheme::dark);
    newSoundButton->setTooltip("Make a new sound from this recipe. It replaces the current sound, which stays in A");
    newSoundButton->onClick = [this]() { generateNewSound(); };
    generateCard->addAndMakeVisible(*newSoundButton);
    undoButton = std::make_unique<juce::TextButton>("Undo");
    undoButton->setTooltip("Back to the sound before the last generation or TONE move");
    undoButton->onClick = [this]() { audioProcessor.getPatchWorkspace().undo(); refresh(); };
    generateCard->addAndMakeVisible(*undoButton);
    
    slotAButton = std::make_unique<juce::TextButton>("A");
    slotAButton->setTooltip("The sound from before the last generation");
    slotAButton->onClick = [this]() { audioProcessor.getPatchWorkspace().selectSlot(ymulatorsynth::PatchWorkspace::Slot::A); refresh(); };
    generateCard->addAndMakeVisible(*slotAButton);
    slotBButton = std::make_unique<juce::TextButton>("B");
    slotBButton->setTooltip("The generated sound; A keeps what was there before");
    slotBButton->onClick = [this]() { audioProcessor.getPatchWorkspace().selectSlot(ymulatorsynth::PatchWorkspace::Slot::B); refresh(); };
    generateCard->addAndMakeVisible(*slotBButton);
    motionCard = std::make_unique<Card>("MOTION", "");
    addAndMakeVisible(*motionCard);
    motionPanel = std::make_unique<MotionPanel>(processor);
    motionCard->addAndMakeVisible(*motionPanel);
    outputCard = std::make_unique<Card>("OUTPUT", "");
    addAndMakeVisible(*outputCard);
    outputScope = std::make_unique<OutputScope>();
    patchPreview = std::make_unique<ymulatorsynth::PatchPreview>();
    outputCard->addAndMakeVisible(*outputScope);
    
    detailLink = std::make_unique<juce::TextButton>(juce::String("DETAIL ") + juce::String(juce::CharPointer_UTF8("\xe2\x96\xb8")));
    detailLink->setTooltip("Open the full operator view");
    detailLink->onClick = [this]() { if (onShowDetail) onShowDetail(); };
    addAndMakeVisible(*detailLink);
    summaryLabel = std::make_unique<juce::Label>("", "");
    summaryLabel->setFont(UiTheme::sans(11.0f));
    summaryLabel->setColour(juce::Label::textColourId, UiTheme::muted);
    addAndMakeVisible(*summaryLabel);
    statusLabel = std::make_unique<juce::Label>("", "");
    statusLabel->setFont(UiTheme::mono(10.0f));
    statusLabel->setColour(juce::Label::textColourId, UiTheme::muted);
    statusLabel->setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(*statusLabel);
    
    refresh();
    startTimerHz(4);
}

QuickView::~QuickView()
{
    stopTimer();
}

void QuickView::addMacroKnob(const char* parameterId, const juce::String& label, const juce::String& caption,
                             juce::Colour accent, std::function<juce::String(double)> formatter)
{
    MacroKnob entry;
    entry.parameterId = parameterId;
    entry.knob = std::make_unique<RotaryKnob>(label, RotaryKnob::Style::Large);
    entry.knob->setAccentColour(accent);
    entry.knob->setSubLabel(caption);
    entry.knob->setValueFormatter(std::move(formatter));
    addAndMakeVisible(*entry.knob);
    entry.binding = KnobBinding::attach(audioProcessor.getParameters(), parameterId, *entry.knob, *this);
    knobs.push_back(std::move(entry));
}

int QuickView::parameterValue(const juce::String& id) const
{
    auto* param = audioProcessor.getParameters().getParameter(id);
    return param ? juce::roundToInt(param->convertFrom0to1(param->getValue())) : 0;
}

void QuickView::stepAlgorithm(int delta)
{
    auto* param = audioProcessor.getParameters().getParameter(ParamID::Global::Algorithm);
    if (param == nullptr) return;
    const int next = (parameterValue(ParamID::Global::Algorithm) + delta + 8) % 8;
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(next)));
    param->endChangeGesture();
    refresh();
}

void QuickView::refresh()
{
    displayedAlgorithm = juce::jlimit(0, 7, parameterValue(ParamID::Global::Algorithm));
    const int feedback = parameterValue(ParamID::Global::Feedback);
    const auto& info = ymulatorsynth::algorithmInfo(displayedAlgorithm);
    algorithmDisplay->setAlgorithm(displayedAlgorithm);
    algorithmDisplay->setFeedbackLevel(feedback);
    algorithmDescription->setText(info.description, juce::dontSendNotification);
    algorithmCaption->setText("ALG " + juce::String(displayedAlgorithm) + " / FB " + juce::String(feedback), juce::dontSendNotification);
    updateSummary();
    updateWorkspaceButtons();
    updatePreview();
    motionPanel->refresh();
}

void QuickView::updatePreview()
{
    // Re-render only when a sound parameter moved; the signature is cheap to compute
    double signature = 0.0;
    int index = 1;
    for (auto* param : audioProcessor.getParameters().processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param))
            if (!ranged->paramID.startsWith("macro_") && !ranged->paramID.startsWith("motion_"))
                signature += static_cast<double>(ranged->getValue()) * static_cast<double>(index++);
    if (signature == previewSignature) return;
    previewSignature = signature;
    
    ymulatorsynth::Preset preset;
    audioProcessor.extractCurrentPreset(preset);
    // Half a second held, then half a second of release
    const int hold = static_cast<int>(ymulatorsynth::PatchPreview::kSampleRate / 2);
    const auto samples = patchPreview->render(preset, hold, hold * 2);
    outputScope->setWaveform(samples, ymulatorsynth::PatchPreview::periodInSamples(),
                             ymulatorsynth::PatchPreview::kSampleRate, static_cast<size_t>(hold));
}

void QuickView::generateNewSound()
{
    audioProcessor.getPatchWorkspace().generate(generatorPanel->currentInput(), juce::Random::getSystemRandom().nextInt64());
    refresh();
}

void QuickView::updateWorkspaceButtons()
{
    using Slot = ymulatorsynth::PatchWorkspace::Slot;
    auto& ws = audioProcessor.getPatchWorkspace();
    undoButton->setEnabled(ws.canUndo());
    const bool haveB = ws.hasSlot(Slot::B);
    slotBButton->setEnabled(haveB);
    slotAButton->setToggleState(ws.activeSlot() == Slot::A, juce::dontSendNotification);
    slotBButton->setToggleState(haveB && ws.activeSlot() == Slot::B, juce::dontSendNotification);
}

void QuickView::updateSummary()
{
    juce::String summary;
    for (int op = 1; op <= 4; ++op) {
        const int tl = parameterValue(ParamID::Op::tl(op));
        const int mul = parameterValue(ParamID::Op::mul(op));
        const int dt = ymulatorsynth::MacroMapper::decodeDetune1(parameterValue(ParamID::Op::dt1(op)));
        summary += "Op" + juce::String(op) + " TL" + juce::String(tl)
                 + " " + juce::String(juce::CharPointer_UTF8("\xc3\x97")) + (mul == 0 ? juce::String("0.5") : juce::String(mul));
        if (dt != 0) summary += " " + signedText(dt);
        summary += kDot;
    }
    const bool lfo = parameterValue(ParamID::Global::LfoAmd) > 0 || parameterValue(ParamID::Global::LfoPmd) > 0;
    summary += juce::String("LFO ") + (lfo ? "on" : "off") + kDot + "Noise " + (parameterValue(ParamID::Global::NoiseEnable) > 0 ? "on" : "off");
    summaryLabel->setText(summary, juce::dontSendNotification);
    
    const double sampleRate = audioProcessor.getSampleRate();
    statusLabel->setText("8 voices" + kDot + "55.9k " + juce::String(juce::CharPointer_UTF8("\xe2\x86\x92")) + " "
                         + (sampleRate > 0 ? juce::String(sampleRate / 1000.0, 1) + "k" : juce::String("host")),
                         juce::dontSendNotification);
}

void QuickView::timerCallback()
{
    if (isShowing()) refresh();
}

void QuickView::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::background);
    g.setColour(UiTheme::dark);
    g.fillRect(0, getHeight() - kFooterHeight, getWidth(), kFooterHeight);
    g.setColour(UiTheme::borderSoft);
    g.fillRect(0, getHeight() - kFooterHeight, getWidth(), 1);
}

void QuickView::resized()
{
    auto bounds = getLocalBounds();
    
    auto footer = bounds.removeFromBottom(kFooterHeight).reduced(20, 0);
    detailLink->setBounds(footer.removeFromLeft(74).withHeight(22).withCentre({ footer.getX() - 37, footer.getCentreY() }));
    footer.removeFromLeft(10);
    statusLabel->setBounds(footer.removeFromRight(170));
    summaryLabel->setBounds(footer);
    
    auto content = bounds.reduced(20, 0).withTrimmedTop(16).withTrimmedBottom(kGap);
    
    // Left column: the recipe (where a new sound starts) above the TONE knobs that shape it.
    // Right column: algorithm, motion, output.
    auto side = content.removeFromRight(kSideWidth);
    content.removeFromRight(kGap + 4);
    const auto knobSize = RotaryKnob::preferredSize(RotaryKnob::Style::Large, RotaryKnob::LabelPosition::Below, true, 100);
    const int toneHeight = 18 + 8 + knobSize.getHeight();
    auto row1 = content.removeFromBottom(toneHeight);
    auto cardArea = side.removeFromTop(toneHeight);
    
    auto titleRow = row1.removeFromTop(18);
    toneLabel->setBounds(titleRow.removeFromLeft(48));
    toneNote->setBounds(titleRow);
    row1.removeFromTop(8);
    const int slot = row1.getWidth() / static_cast<int>(knobs.size());
    for (size_t i = 0; i < knobs.size(); ++i) {
        const int x = row1.getX() + static_cast<int>(i) * slot + (slot - knobSize.getWidth()) / 2;
        knobs[i].knob->setBounds(x, row1.getY(), knobSize.getWidth(), knobSize.getHeight());
    }
    
    algorithmCard->setBounds(cardArea);
    {
        auto body = algorithmCard->bodyBounds();
        auto diagram = body.removeFromLeft(104);
        algorithmDisplay->setBounds(diagram.withHeight(66).withY(body.getY() + 2));
        algorithmCaption->setBounds(diagram.withY(body.getY() + 70).withHeight(14));
        body.removeFromLeft(10);
        auto buttons = body.removeFromBottom(22);
        previousAlgorithmButton->setBounds(buttons.removeFromLeft(28));
        buttons.removeFromLeft(4);
        nextAlgorithmButton->setBounds(buttons.removeFromLeft(28));
        algorithmDescription->setBounds(body.withTrimmedBottom(4));
    }
    
    content.removeFromBottom(kGap);
    side.removeFromTop(kGap);
    
    generateCard->setBounds(content);
    {
        auto header = generateCard->headerBounds();
        header.removeFromLeft(generateCard->titleWidth());
        generateNote->setBounds(header);
        auto body = generateCard->bodyBounds();
        // The action row sits at the bottom right: the recipe above, Generate as its conclusion
        auto actions = body.removeFromBottom(30).withTrimmedBottom(2);
        auto placeRight = [&](juce::Component& c, int width) {
            c.setBounds(actions.removeFromRight(width).withHeight(26).withCentre({ actions.getRight() + width / 2, actions.getCentreY() }));
            actions.removeFromRight(6);
        };
        placeRight(*newSoundButton, 110);
        actions.removeFromRight(10);
        placeRight(*slotBButton, 34);
        placeRight(*slotAButton, 34);
        actions.removeFromRight(8);
        placeRight(*undoButton, 64);
        generatorPanel->setBounds(body.withTrimmedTop(6));
    }
    outputCard->setBounds(side.removeFromBottom(86));
    outputScope->setBounds(outputCard->bodyBounds());
    side.removeFromBottom(10);
    motionCard->setBounds(side);
    motionPanel->setBounds(motionCard->bodyBounds());
}
