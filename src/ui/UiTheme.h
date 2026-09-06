#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Colours and type for the editor. Carrier = blue, modulator = magenta, envelope = green, focus = amber. */
namespace UiTheme {

inline const juce::Colour background   { 0xff101418 };
inline const juce::Colour strip        { 0xff0d1114 };
inline const juce::Colour panel        { 0xff151b1f };
inline const juce::Colour dark         { 0xff0b0f12 };
inline const juce::Colour border       { 0xff2b353b };
inline const juce::Colour borderSoft   { 0xff1f272c };
inline const juce::Colour text         { 0xffe6efe9 };
inline const juce::Colour muted        { 0xff9fb3a8 };
inline const juce::Colour dim          { 0xff5f7269 };
inline const juce::Colour green        { 0xff52e3a1 };
inline const juce::Colour amber        { 0xffffb454 };
inline const juce::Colour carrier      { 0xff7fb0f0 };
inline const juce::Colour modulator    { 0xffc76aa3 };
inline const juce::Colour carrierEdge  { 0xff35506b };

inline juce::Font mono(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), height,
                                        bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Font sans(float height, bool bold = false)
{
    return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
}


/** Milliseconds as a short knob value: "180" under a second, "1.8s" above. */
inline juce::String formatTime(double milliseconds)
{
    return milliseconds < 1000.0 ? juce::String(juce::roundToInt(milliseconds)) : juce::String(milliseconds / 1000.0, 1) + "s";
}
} // namespace UiTheme
