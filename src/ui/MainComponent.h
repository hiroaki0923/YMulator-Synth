#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <optional>
#include <string>
#include <vector>
#include "YmLookAndFeel.h"
#include "OperatorPanel.h"
#include "ToneStrip.h"
#include "LfoNoiseStrip.h"
#include "PresetUIManager.h"
#include "../core/MacroMapper.h"

class YMulatorSynthAudioProcessor;

/**
 * Editor root. Header (mode, preset, pan), the TONE row, four operator rows
 * and the LFO/noise footer. Keeps operator roles in step with the algorithm
 * and highlights the raw knobs a macro drives while it is being touched.
 */
class MainComponent : public juce::Component,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater
{
public:
    static constexpr int kWidth = 1000;
    static constexpr int kHeight = 640;
    
    explicit MainComponent(YMulatorSynthAudioProcessor& processor);
    ~MainComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    /** Highlights the raw parameters the macro drives for the current algorithm; nullopt clears. */
    void setMacroFocus(std::optional<ymulatorsynth::Macro> macro);
    std::vector<std::string> highlightedParameterIds() const;
    
    /** Re-derives operator roles and hints from the algorithm, feedback and noise parameters. */
    void refreshRoles();
    const OperatorPanel& getOperatorPanel(int index) const { return *operatorPanels[static_cast<size_t>(index)]; }
    
private:
    YmLookAndFeel lookAndFeel;      // first member: outlives every child that uses it
    YMulatorSynthAudioProcessor& audioProcessor;
    
    std::unique_ptr<juce::TextButton> quickModeButton;
    std::unique_ptr<juce::TextButton> detailModeButton;
    std::unique_ptr<PresetUIManager> presetUIManager;
    std::unique_ptr<juce::ComboBox> globalPanComboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> globalPanAttachment;
    
    std::unique_ptr<ToneStrip> toneStrip;
    std::array<std::unique_ptr<OperatorPanel>, 4> operatorPanels;
    std::unique_ptr<LfoNoiseStrip> lfoNoiseStrip;
    
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    int parameterValue(const char* id) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
