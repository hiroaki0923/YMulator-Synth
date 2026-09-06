# Changelog

All notable changes to YMulator-Synth. Japanese: [CHANGELOG_ja.md](CHANGELOG_ja.md).

## Unreleased

**🔧 Developer:**
- Tests keep imported banks and user presets in a throw-away directory instead of the user's own YMulator-Synth folder
- Linux CI runs every test binary, the UI tests included, under a virtual display; pull requests now get a Linux job as well

## Version 0.1.0 (2026-09-06)
**Feature Release: Quick view, Motion and MIDI expression**

**✨ New:**
- **Quick view**: seven TONE macros (Brightness, Harmonics, Attack, Decay, Release, Spread, Feedback) that move the operator parameters relative to the loaded preset, a patch generator with categories and Undo / A-B comparison, an algorithm picker and an OUTPUT view that plays the current sound back visually: the waveform at a moving playhead over its level envelope, from key-on through release. The Detail view keeps every register parameter and shows which knobs a macro is moving
- **Algorithm diagrams**: the eight algorithms are drawn as images with the feedback loop highlighted when feedback is on
- **Motion**: control-rate effects on top of the chip, synced to the host tempo when wanted: software vibrato with delay and rise, timbre LFO, tremolo, pan motion, two-stage pitch envelope, brightness sweep, level envelope, portamento / legato, chip arpeggio, velocity-to-brightness, LFO waveforms with one-shot; with Sync on, each rate knob in the Detail view turns into a note-value box. **Wide** runs a second chip instance slightly detuned on the other side so all 8 voices remain, and **Echo** plays a delayed, quieter copy of each note on alternating sides while the note itself keeps its pan
- **MIDI**: CC 102-107 drive the macros and CC 110-118 the Motion amounts; CC 121 also centres the macros. VOPMex numbers 75 / 76 set the channel PMS / AMS. An optional Expressive MIDI switch routes the mod wheel to vibrato depth and aftertouch to Brightness

**🐛 Fixes:**
- **Operator on/off**: the key-on bits were assigned in register-address order (M1, M2, C1, C2). The chip keys them in chain order (M1, C1, M2, C2), so switching off C1 actually silenced M2 and a patch with only M1 and C1 on was silent. The .opm SLOT mask follows the same order
- **Init preset**: all four operators had MUL 0 (x0.5), so the Init voice played an octave low. MUL is now 1 as in VOPM

**⚠️ Change:**
- The 0.0.6 LFO numbers 76-79 are no longer accepted (76 now means AMS, as in VOPMex); 81 for noise frequency still works

## Version 0.0.8 (2026-09-06)
**Bug Fix Release: LFO, SLOT and Preset Saving**

**🐛 Fixes:**
- **LFO depth**: The phase modulation depth was written to an unused register address, so vibrato never sounded. On the YM2151 both depths share one register selected by bit 7; the write is corrected
- **LFO settings from presets**: A preset's LFO rate, depths, waveform, noise and channel AMS/PMS sensitivity were never loaded into the parameters or sent to the chip. Two new parameters, LFO AMS and LFO PMS, carry the sensitivity for all channels. More than 20 of the bundled presets use the LFO and now sound as written
- **SLOT (operator on/off)**: The per-operator enable parameters had been dropped from the parameter tree, leaving the checkboxes disconnected. They are back, drive the key-on register in hardware slot order, and the .opm SLOT mask maps to operators correctly
- **Preset saving**: Both save paths built the preset by hand with truncating conversions; they now use the same extraction as the chip, so saved values match what was heard
- **MIDI CC 33**: The LFO rate LSB was swallowed by a leftover per-channel pan handler on CC 32-39. Those eight per-channel pan parameters never reached the chip (voices are allocated dynamically) and are removed together with their CCs
- **AMS enable**: The per-operator AM enable was a 0-3 integer parameter; it is now a switch
- **Velocity**: MIDI velocity had no effect. It now attenuates the carriers linearly by up to 32 TL steps (about 24 dB) while modulators keep their level, so quieter notes keep their timbre

**⚠️ Change:**
- Presets that use the LFO sound different from 0.0.7 and earlier because the LFO is now audible
- Notes below velocity 127 are quieter than before; velocity 127 is unchanged

**🔧 Developer:**
- Register golden test extended to LFO, sensitivity and noise registers; new tests render audio to confirm PMD/PMS and AMD/AMS take effect, and cover the key-on slot mask
- New test asserts that every parameter changes a chip register, so a parameter can no longer be exposed and ignored

## Version 0.0.7 (2026-09-05)
**Bug Fix Release: Pitch, Operator Mapping, MIDI CC and Host Compatibility**

**🐛 Fixes:**
- **Pitch**: Notes played about 3 semitones flat at 44.1 kHz (no resampling from the chip's 55.9 kHz rate, and a key-code table one semitone sharp)
- **Operator mapping**: C1 and M2 were written to each other's register slots, so every preset played with its second carrier and second modulator exchanged (harsh, noisy timbres compared with VOPM)
- **Preset selection**: Bank/preset choice was never saved; the preset box showed "Init" on reopening and DAW projects did not restore it
- **Multiple instances**: The second instance of the plugin on the same thread was silent
- **VST3 hosts**: Editing a parameter changed the program count, which made some hosts reset to program 0; the count is now fixed and the last program is "Custom"
- **MIDI CC**: The mapping now matches VOPMex (one CC per parameter and operator, CC value = register value, D1L 55-58, RR 59-62, AME 70-73, LFO 1/2/3/12, noise 80/82); the previous build used per-operator CC blocks and scaled values
- **Preset values**: Release Rate (and any parameter whose range does not start at 0) was written one step low because of normalised-value truncation; RR now spans 0-15 as in VOPM
- **Bundled collection**: The 64 bundled presets were not registered in any bank and could not be chosen from the Bank/Preset menus; they now appear as the "Collection" bank

**⚠️ Change:**
- A fixed 2x output gain that clipped single notes was removed; output is about 6 dB lower than 0.0.6

**🔧 Developer:**
- `tools/ui_snapshot`: renders the editor off-screen to a PNG for review without a host
- New regression tests for pitch, operator slot order, preset persistence, multiple instances, program count and CC mapping

## Version 0.0.6 (2025-06-23)
**Quality Enhancement Release: Global Pan & DAW Compatibility**

**🎵 New Features:**
- **Global Pan System**: LEFT/CENTER/RIGHT/RANDOM panning modes for enhanced stereo control
- **Preset Name Preservation**: Global pan changes no longer switch to "Custom" mode
- **Enhanced DAW Compatibility**: Improved GarageBand integration and stability
- **Audio Buffer Optimization**: Fixed duplicate sound and playback delay issues
- **Performance Improvements**: Optimized real-time processing with reduced CPU load

**🔧 Technical Improvements:**
- **Correct ymfm Output Handling**: Fixed audio buffer interpretation (data[0]=left, data[1]=right)
- **Buffer Management**: Implemented proper buffer clearing to prevent audio artifacts
- **Parameter Exception Handling**: Global pan parameters bypass custom preset mode switching
- **YM2151 Register Control**: Accurate panning register manipulation with bit-level precision
- **Resource Management**: Enhanced Audio Unit resource cleanup for stable operation

**🐛 Bug Fixes:**
- Fixed duplicate/overlapping notes during playback
- Resolved 1-2 second audio delay after stopping playback
- Fixed sample rate synchronization issues with various DAWs
- Eliminated audio artifacts from residual buffer data

## Version 0.0.5 (2025-06-16)
**Cross-Platform Release**

**🎵 New Features:**
- **Multi-Platform Support**: Windows, macOS, and Linux binaries
- **Multiple Plugin Formats**: VST3, AU, AUv3, and Standalone versions
- **Enhanced Distribution**: Comprehensive installer packages for all platforms

## Version 0.0.4 (2025-06-15)
**Major Release: Complete Preset Management System**

**🎵 New Features:**
- **Bank/Preset Dual ComboBox System**: Hierarchical preset organization with Factory and imported banks
- **OPM File Import**: Full support for .opm preset files exported from VOPM and compatible applications
- **DAW Project Persistence**: Bank and preset selections automatically restored after DAW restart
- **Enhanced User Experience**: Streamlined UI with File menu removal and optimized layout

**🔧 Technical Improvements:**
- **OPM Parser Robustness**: Fixed whitespace normalization for reliable SLOT mask and noise enable parsing
- **Performance Optimization**: Reduced debug output while maintaining comprehensive error reporting
- **State Management**: ValueTreeState integration for seamless DAW project save/load
- **Memory Management**: Duplicate bank prevention and efficient user data persistence

**🐛 Bug Fixes:**
- Fixed OPM parser handling of multiple spaces/tabs causing incorrect parameter parsing
- Resolved UI layout spacing issues after File menu removal
- Fixed DAW project restore order to load user data before applying presets

## Version 0.0.3 (2025-06-12)
**UI Enhancement Release**

**🎵 New Features:**
- **SLOT Control System**: Individual operator enable/disable via title bar checkboxes
- **OPM File Compatibility**: Full SLOT mask compatibility with existing .opm preset files
- **Visual Feedback**: Clear indication of enabled/disabled operators

**🔧 Technical Improvements:**
- **Backward Compatibility**: All existing presets remain fully functional
- **UI Integration**: Seamless SLOT control integration with parameter system

## Version 0.0.2 (2025-06-11)
**Core Audio Enhancement Release**

**🎵 New Features:**
- **YM2151 Noise Generator**: Hardware-accurate noise synthesis on channel 7
- **LFO Complete Implementation**: 4 waveforms with AMS/PMS modulation
- **Enhanced Envelope System**: Velocity sensitivity and batch optimization

**🔧 Technical Improvements:**
- **Hardware Constraints**: Full YM2151 hardware limitation compliance
- **Performance Optimization**: Efficient envelope processing
- **MIDI CC Expansion**: Additional controllers for noise and LFO parameters

## Version 0.0.1 (2025-06-08)
**Initial Release**

**🎵 Core Features:**
- **YM2151 (OPM) Emulation**: 8-voice polyphonic FM synthesis
- **Professional Interface**: Intuitive 4-operator layout with all parameters
- **8 Factory Presets**: Professional-quality starting sounds
- **MIDI Integration**: Full Note On/Off, CC, and pitch bend support
- **Audio Unit Compatibility**: Native macOS plugin integration
