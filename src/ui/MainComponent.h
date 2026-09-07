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
#include "MotionStrip.h"
#include "QuickView.h"
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
    
    enum class ViewMode { Quick, Detail };
    
    explicit MainComponent(YMulatorSynthAudioProcessor& processor);
    ~MainComponent() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    /** Highlights the raw parameters the macro drives for the current algorithm; nullopt clears. */
    void setMacroFocus(std::optional<ymulatorsynth::Macro> macro);
    std::vector<std::string> highlightedParameterIds() const;
    
    /** Re-derives operator roles and hints from the algorithm, feedback and noise parameters. */
    void refreshRoles();
    
    /** Switches between the Quick and Detail views; the choice is kept in the plugin state. */
    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const { return viewMode; }
    const OperatorPanel& getOperatorPanel(int index) const { return *operatorPanels[static_cast<size_t>(index)]; }
    
private:
    YmLookAndFeel lookAndFeel;      // first member: outlives every child that uses it
    YMulatorSynthAudioProcessor& audioProcessor;
    
    std::unique_ptr<juce::TextButton> quickModeButton;
    std::unique_ptr<juce::TextButton> detailModeButton;
    std::unique_ptr<PresetUIManager> presetUIManager;
    juce::TooltipWindow tooltipWindow { this, 600 };   // tooltips need one window in the editor to show at all
    
    std::unique_ptr<QuickView> quickView;
    std::unique_ptr<ToneStrip> toneStrip;
    std::array<std::unique_ptr<OperatorPanel>, 4> operatorPanels;
    std::unique_ptr<MotionStrip> motionStrip;
    std::unique_ptr<LfoNoiseStrip> lfoNoiseStrip;
    
    ViewMode viewMode = ViewMode::Quick;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    int parameterValue(const char* id) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
