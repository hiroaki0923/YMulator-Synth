#include "MainComponent.h"
#include "UiTheme.h"
#include "../PluginProcessor.h"
#include "../dsp/AlgorithmInfo.h"
#include "../utils/Debug.h"
#include "../utils/ParameterIDs.h"
#include <set>

namespace {
constexpr int kHeaderHeight = 50;
constexpr int kToneHeight = 64;
constexpr int kFooterHeight = 40;
constexpr int kRowGap = 6;
const char* const kRoleParameters[] = { ParamID::Global::Algorithm, ParamID::Global::Feedback, ParamID::Global::NoiseEnable };
const juce::Identifier kViewModeProperty("uiViewMode");
}

MainComponent::MainComponent(YMulatorSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setLookAndFeel(&lookAndFeel);
    
    quickModeButton = std::make_unique<juce::TextButton>("Quick");
    quickModeButton->setTooltip("Seven macro knobs and the algorithm");
    quickModeButton->onClick = [this]() { setViewMode(ViewMode::Quick); };
    addAndMakeVisible(*quickModeButton);
    detailModeButton = std::make_unique<juce::TextButton>("Detail");
    detailModeButton->setTooltip("Every operator register");
    detailModeButton->onClick = [this]() { setViewMode(ViewMode::Detail); };
    addAndMakeVisible(*detailModeButton);
    
    presetUIManager = std::make_unique<PresetUIManager>(processor);
    addAndMakeVisible(*presetUIManager);
    
    quickView = std::make_unique<QuickView>(processor);
    quickView->onShowDetail = [this]() { setViewMode(ViewMode::Detail); };
    addAndMakeVisible(*quickView);
    
    toneStrip = std::make_unique<ToneStrip>(processor);
    toneStrip->onMacroFocus = [this](std::optional<ymulatorsynth::Macro> macro) { setMacroFocus(macro); };
    addAndMakeVisible(*toneStrip);
    
    for (int i = 0; i < 4; ++i) {
        operatorPanels[static_cast<size_t>(i)] = std::make_unique<OperatorPanel>(processor, i + 1);
        addAndMakeVisible(*operatorPanels[static_cast<size_t>(i)]);
    }
    
    motionStrip = std::make_unique<MotionStrip>(processor);
    addAndMakeVisible(*motionStrip);
    
    lfoNoiseStrip = std::make_unique<LfoNoiseStrip>(processor);
    addAndMakeVisible(*lfoNoiseStrip);
    
    for (const char* id : kRoleParameters)
        audioProcessor.getParameters().addParameterListener(id, this);
    refreshRoles();
    
    const juce::String savedMode = audioProcessor.getParameters().state.getProperty(kViewModeProperty, "quick").toString();
    setViewMode(savedMode == "detail" ? ViewMode::Detail : ViewMode::Quick);
    
    setSize(kWidth, kHeight);
    CS_DBG("MainComponent created");
}

MainComponent::~MainComponent()
{
    cancelPendingUpdate();
    for (const char* id : kRoleParameters)
        audioProcessor.getParameters().removeParameterListener(id, this);
    
    lfoNoiseStrip.reset();
    motionStrip.reset();
    for (auto& panel : operatorPanels) panel.reset();
    toneStrip.reset();
    quickView.reset();
    presetUIManager.reset();
    quickModeButton.reset();
    detailModeButton.reset();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(UiTheme::background);
    
    g.setColour(UiTheme::borderSoft);
    g.fillRect(0, kHeaderHeight - 1, getWidth(), 1);
    
    g.setColour(UiTheme::green);
    g.setFont(UiTheme::mono(14.0f, true));
    g.drawText("YMULATOR", 16, 0, 90, kHeaderHeight, juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();
    
    auto header = bounds.removeFromTop(kHeaderHeight);
    header.removeFromLeft(16 + 90 + 12);
    auto modeArea = header.removeFromLeft(124);
    quickModeButton->setBounds(modeArea.removeFromLeft(60).withHeight(26).withCentre({ modeArea.getX() - 30, header.getCentreY() }));
    detailModeButton->setBounds(modeArea.removeFromLeft(60).withHeight(26).withCentre({ modeArea.getX() - 30, header.getCentreY() }));
    header.removeFromRight(16);
    header.removeFromLeft(12);
    presetUIManager->setBounds(header);
    
    quickView->setBounds(bounds);
    
    toneStrip->setBounds(bounds.removeFromTop(kToneHeight));
    lfoNoiseStrip->setBounds(bounds.removeFromBottom(kFooterHeight));
    motionStrip->setBounds(bounds.removeFromBottom(motionStrip->preferredHeight()));
    
    auto rows = bounds.reduced(20, 8);
    const int rowHeight = (rows.getHeight() - kRowGap * 3) / 4;
    for (int i = 0; i < 4; ++i)
        operatorPanels[static_cast<size_t>(i)]->setBounds(rows.getX(), rows.getY() + i * (rowHeight + kRowGap), rows.getWidth(), rowHeight);
    
    const double sampleRate = audioProcessor.getSampleRate();
    lfoNoiseStrip->setStatusText("8 voices " + juce::String(juce::CharPointer_UTF8("\xc2\xb7")) + " 55.9k "
                                 + juce::String(juce::CharPointer_UTF8("\xe2\x86\x92")) + " "
                                 + (sampleRate > 0 ? juce::String(sampleRate / 1000.0, 1) + "k" : juce::String("host")));
}

int MainComponent::parameterValue(const char* id) const
{
    auto* param = audioProcessor.getParameters().getParameter(id);
    return param ? juce::roundToInt(param->convertFrom0to1(param->getValue())) : 0;
}

void MainComponent::refreshRoles()
{
    const int algorithm = juce::jlimit(0, 7, parameterValue(ParamID::Global::Algorithm));
    const int feedback = parameterValue(ParamID::Global::Feedback);
    const bool noise = parameterValue(ParamID::Global::NoiseEnable) > 0;
    const auto& info = ymulatorsynth::algorithmInfo(algorithm);
    
    for (int op = 0; op < 4; ++op) {
        auto role = info.isCarrier(op) ? OperatorPanel::Role::Carrier : OperatorPanel::Role::Modulator;
        juce::String hint;
        if (op == 3 && noise) {
            role = OperatorPanel::Role::Noise;
            hint = "Noise generator replaces this operator";
        } else if (role == OperatorPanel::Role::Carrier) {
            hint = "Sounds directly. Level sets loudness";
        } else {
            hint = "Shapes Op" + juce::String(info.targetOf(op) + 1) + ". Level sets brightness";
            if (op == 0 && feedback > 0) hint += ", feedback " + juce::String(feedback);
        }
        operatorPanels[static_cast<size_t>(op)]->setRole(role, hint);
    }
    toneStrip->setAlgorithm(algorithm);
    toneStrip->setFeedback(feedback);
    quickView->refresh();
}

void MainComponent::setViewMode(ViewMode mode)
{
    viewMode = mode;
    const bool quick = mode == ViewMode::Quick;
    quickView->setVisible(quick);
    toneStrip->setVisible(!quick);
    for (auto& panel : operatorPanels) panel->setVisible(!quick);
    lfoNoiseStrip->setVisible(!quick);
    motionStrip->setVisible(!quick);
    quickModeButton->setToggleState(quick, juce::dontSendNotification);
    detailModeButton->setToggleState(!quick, juce::dontSendNotification);
    audioProcessor.getParameters().state.setProperty(kViewModeProperty, quick ? "quick" : "detail", nullptr);
    if (quick) quickView->refresh();
    else setMacroFocus(std::nullopt);
}

void MainComponent::setMacroFocus(std::optional<ymulatorsynth::Macro> macro)
{
    std::set<std::string> ids;
    if (macro.has_value()) {
        const int algorithm = juce::jlimit(0, 7, parameterValue(ParamID::Global::Algorithm));
        for (const auto& id : ymulatorsynth::MacroMapper::targetsOf(*macro, algorithm)) ids.insert(id);
    }
    for (auto& panel : operatorPanels) panel->setHighlightedParameters(ids);
    toneStrip->setHighlightedParameters(ids);
}

std::vector<std::string> MainComponent::highlightedParameterIds() const
{
    std::vector<std::string> ids;
    for (const auto& panel : operatorPanels)
        for (const auto& id : panel->highlightedParameterIds()) ids.push_back(id);
    for (const auto& id : toneStrip->highlightedParameterIds()) ids.push_back(id);
    return ids;
}

void MainComponent::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void MainComponent::handleAsyncUpdate()
{
    refreshRoles();
}
