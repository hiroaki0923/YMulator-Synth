#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"

class YMulatorSynthAudioProcessor;

/** Detail-view row with every motion parameter: four themed cards two controls high (LFO, ENVELOPE, SPACE, PLAY). */
class MotionStrip : public juce::Component
{
public:
    explicit MotionStrip(YMulatorSynthAudioProcessor& processor);
    ~MotionStrip() override = default;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    /** Height the strip needs for its two rows and the theme titles. */
    int preferredHeight() const;
    
private:
    struct Knob {
        const char* parameterId;
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
        bool isRate = false;
        // A rate knob's note-value twin: shown in its place while Sync is on
        std::unique_ptr<juce::ComboBox> divBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divAttachment;
    };
    struct Group {
        std::vector<Knob> knobs;
        std::vector<juce::ComboBox*> boxes;
        std::vector<juce::ToggleButton*> toggles;
        std::vector<juce::Button*> buttons;   // small plain buttons, e.g. the arpeggio settings
        int theme = 0;
        int row = 0;
        juce::String caption;   // drawn above the group's controls, e.g. "tremolo"
        juce::Rectangle<int> bounds;
    };
    struct Theme {
        juce::String title;
        juce::Rectangle<int> bounds;
        int titleWidth = 0;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    RotaryKnob::Style knobStyle = RotaryKnob::Style::Tiny;
    std::unique_ptr<juce::Label> sectionLabel;
    std::vector<Theme> themes;
    std::vector<Group> groups;
    std::unique_ptr<juce::ComboBox> widePanBox, panModeBox, panRateBox, arpModeBox, arpDivBox, vibWaveBox, timbreWaveBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> widePanAttachment, panModeAttachment, panRateAttachment, arpModeAttachment, arpDivAttachment, vibWaveAttachment, timbreWaveAttachment;
    std::unique_ptr<juce::ToggleButton> syncButton, monoButton, oneShotButton;
    std::unique_ptr<juce::TextButton> arpSettingsButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment, monoAttachment, oneShotAttachment;
    
    int addTheme(const juce::String& title);
    int lowerCaptionTop = 0;   // where the lower row's caption sits, between the rows
    Group& addGroup(int theme, int row, const juce::String& caption = {});
    void addKnob(Group& group, const char* parameterId, const juce::String& label, juce::Colour accent, bool isRate = false,
                 std::function<juce::String(double)> formatter = {}, const char* divParameterId = nullptr);
    juce::ComboBox* addBox(Group& group, std::unique_ptr<juce::ComboBox>& box,
                           std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment,
                           const char* parameterId, const juce::StringArray& items, const juce::String& tooltip);
    juce::ToggleButton* addToggle(Group& group, std::unique_ptr<juce::ToggleButton>& button,
                                  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment,
                                  const char* parameterId, const juce::String& text, const juce::String& tooltip);
    void buildLayout();
    int widthOf(const Group& group) const;
    int knobSlot(const Knob& knob) const;
    void updateRateKnobs();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MotionStrip)
};
