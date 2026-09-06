#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include <vector>
#include "RotaryKnob.h"
#include "KnobBinding.h"
#include "AlgorithmDisplay.h"
#include "../core/MacroMapper.h"
#include "GeneratorPanel.h"
#include "OutputScope.h"
#include "../core/PatchPreview.h"

class YMulatorSynthAudioProcessor;

/**
 * The Quick view: seven large macro knobs, the algorithm card, and the areas
 * reserved for the generator, A/B compare, motion and the output scope.
 * Everything here edits the same parameters the Detail view shows.
 */
class QuickView : public juce::Component,
                  private juce::Timer
{
public:
    explicit QuickView(YMulatorSynthAudioProcessor& processor);
    ~QuickView() override;
    
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    /** Refreshes the algorithm card and status line from the parameters. */
    void refresh();
    int getDisplayedAlgorithm() const { return displayedAlgorithm; }
    
    std::function<void()> onShowDetail;
    
private:
    struct MacroKnob {
        const char* parameterId;
        std::unique_ptr<RotaryKnob> knob;
        KnobBinding binding;
    };
    
    /** Framed panel with a section title; the body is left to a later step where noted. */
    class Card : public juce::Component {
    public:
        Card(const juce::String& title, const juce::String& note);
        void paint(juce::Graphics& g) override;
        juce::Rectangle<int> bodyBounds() const;
        juce::Rectangle<int> headerBounds() const;
        int titleWidth() const;
    private:
        juce::String title, note;
    };
    
    YMulatorSynthAudioProcessor& audioProcessor;
    std::vector<MacroKnob> knobs;
    std::unique_ptr<juce::Label> toneLabel, toneNote;
    
    std::unique_ptr<Card> algorithmCard, generateCard, compareCard, motionCard, outputCard;
    std::unique_ptr<GeneratorPanel> generatorPanel;
    std::unique_ptr<OutputScope> outputScope;
    std::unique_ptr<ymulatorsynth::PatchPreview> patchPreview;
    double previewSignature = -1.0;
    std::unique_ptr<juce::TextButton> newSoundButton, undoButton, slotAButton, slotBButton;
    std::unique_ptr<juce::Label> generateNote, compareNote;
    std::unique_ptr<AlgorithmDisplay> algorithmDisplay;
    std::unique_ptr<juce::Label> algorithmTitle, algorithmDescription, algorithmCaption;
    std::unique_ptr<juce::TextButton> previousAlgorithmButton, nextAlgorithmButton;
    std::unique_ptr<juce::TextButton> detailLink;
    std::unique_ptr<juce::Label> summaryLabel, statusLabel;
    int displayedAlgorithm = 0;
    
    void addMacroKnob(const char* parameterId, const juce::String& label, const juce::String& caption, juce::Colour accent,
                      std::function<juce::String(double)> formatter);
    void stepAlgorithm(int delta);
    void generateNewSound();
    void updateWorkspaceButtons();
    int parameterValue(const juce::String& id) const;
    void timerCallback() override;
    void updateSummary();
    void updatePreview();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickView)
};
