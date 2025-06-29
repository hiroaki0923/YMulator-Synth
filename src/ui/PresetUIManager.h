#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class YMulatorSynthAudioProcessor;

/**
 * PresetUIManager - Extracted from MainComponent
 * 
 * Handles all preset-related UI functionality:
 * - Bank and preset selection ComboBoxes
 * - Save preset button
 * - File dialogs for loading/saving OPM files
 * - Preset change event handling
 * 
 * Part of Phase 2 refactoring to split MainComponent responsibilities.
 */
class PresetUIManager : public juce::Component,
                        public juce::ValueTree::Listener
{
public:
    explicit PresetUIManager(YMulatorSynthAudioProcessor& processor);
    ~PresetUIManager() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // ValueTree::Listener overrides for preset list updates
    void valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                 const juce::Identifier& property) override;
    
    // Public interface for MainComponent integration
    void updateBankComboBox();
    void updatePresetComboBox();
    void refreshPresetDisplay();

private:
    YMulatorSynthAudioProcessor& audioProcessor;
    
    // UI Components
    std::unique_ptr<juce::ComboBox> bankComboBox;
    std::unique_ptr<juce::Label> bankLabel;
    std::unique_ptr<juce::ComboBox> presetComboBox;
    std::unique_ptr<juce::Label> presetLabel;
    std::unique_ptr<juce::TextButton> savePresetButton;
    
    // UI state management
    bool isUpdatingFromState = false;
    
    // Setup methods
    void setupComponents();
    
    // Event handlers
    void onBankChanged();
    void onPresetChanged();
    void loadOpmFileDialog();
    void savePresetDialog();
    void savePresetToFile(const juce::File& file, const juce::String& presetName);
    
    // Unused ValueTree::Listener methods
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged(juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged(juce::ValueTree&) override {}
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetUIManager)
};