#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <string>
#include <vector>

namespace ymulatorsynth {

enum class Macro { Brightness, Harmonics, Attack, Decay, Release, Spread };

enum class HarmonicsTemplate { Preset, Saw, Square, Pulse, Bright, Bell, Metal, Sub, Octave, Count };

/** The raw parameters the macro layer works on, operators in voice order (M1, C1, M2, C2). */
struct RawPatch
{
    std::array<int, 4> tl {}, ar {}, d1r {}, d2r {}, rr {}, dt1 {}, mul {};
    int feedback = 0;
    
    bool operator==(const RawPatch& other) const;
    bool operator!=(const RawPatch& other) const { return !(*this == other); }
};

/** Macro positions. Continuous macros are -1..+1 (display -50..+50), centre = 0. */
struct MacroValues
{
    float brightness = 0.0f, attack = 0.0f, decay = 0.0f, release = 0.0f, spread = 0.0f;
    HarmonicsTemplate harmonics = HarmonicsTemplate::Preset;
    
    bool isCentred() const;
};

/**
 * Maps the Quick panel macros onto the raw YM2151 parameters as offsets from
 * an anchor (the values captured when a preset was loaded, generated or
 * saved). The pure mapping is `apply()`; the instance keeps the anchor, keeps
 * it re-based when raw parameters are edited directly, and writes mapped
 * values back to the parameter tree when a macro moves.
 * Spec: docs/ymulatorsynth-quick-panel-design.md section 2.
 */
class MacroMapper : private juce::AudioProcessorValueTreeState::Listener
{
public:
    static constexpr float kDisplayRange = 50.0f;
    
    static RawPatch apply(const RawPatch& anchor, const MacroValues& macros, int algorithm);
    static std::vector<std::string> targetsOf(Macro macro, int algorithm);
    static int encodeDetune1(int signedDetune);
    static int decodeDetune1(int registerValue);
    
    explicit MacroMapper(juce::AudioProcessorValueTreeState& parameters);
    ~MacroMapper() override;
    
    /** Anchor = current raw values, all macros back to centre. */
    void captureAnchor();
    
    /** While suspended, parameter changes neither remap nor re-base (bulk loads). */
    void setSuspended(bool shouldSuspend) { suspended = shouldSuspend; }
    
    void writeAnchorTo(juce::ValueTree& state) const;
    /** Restores the anchor from a saved state; without one, captures the current raw values. */
    bool restoreFromState(const juce::ValueTree& state);
    
    const RawPatch& getAnchor() const { return anchor; }
    /** Replaces the anchor without touching the macros (snapshot restore). */
    void setAnchor(const RawPatch& newAnchor);
    RawPatch currentRaw() const;
    MacroValues currentMacros() const;
    int currentAlgorithm() const;
    
    /** True when either the raw values left the anchor or a macro is off centre. */
    bool isEdited() const;
    
    static const juce::Identifier anchorNodeType;
    
private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void applyMacros();
    void rebase(const juce::String& parameterID, int newValue);
    void writeRaw(const std::string& id, int value);
    int readInt(const std::string& id) const;
    
    juce::AudioProcessorValueTreeState& parameters;
    RawPatch anchor;
    RawPatch lastRaw;
    bool applying = false;
    bool suspended = false;
    std::vector<juce::String> listenedIds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MacroMapper)
};

} // namespace ymulatorsynth
