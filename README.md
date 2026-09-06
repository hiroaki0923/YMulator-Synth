# YMulator Synth

*[日本語版はこちら / Japanese version](README_ja.md)*

YMulator Synth is a YM2151 (OPM) FM synthesizer plugin, the chip behind the X68000 and a generation of arcade boards, with the sound of the original emulated by ymfm and an interface built for making FM sounds rather than just editing registers. It runs as an Audio Unit, VST3 and standalone app on macOS, and as a VST3 on Windows and Linux.

![YMulator Synth Quick view](docs/images/screenshot.png)

*Quick view: shape a sound with seven TONE macros, cook a new one from a recipe, add Motion, and watch what it does.*

![YMulator Synth Detail view](docs/images/screenshot-detail.png)

*Detail view: every register of the four operators, with the knobs a macro moves highlighted.*

## What it does

- **Authentic OPM**: 8-voice YM2151 emulation by Aaron Giles' ymfm, all 8 algorithms, DT1 / DT2 / KS / feedback, the hardware LFO with four waveforms, and the channel 8 noise generator. Chip output at its native 55.9 kHz is resampled to the host rate, so pitch is exact.
- **Two views of one sound**: the **Quick** view works in musical terms; the **Detail** view shows the registers. They are the same parameters, so nothing changes when you switch.
- **TONE macros**: Brightness, Harmonics, Attack, Decay, Release, Spread and Feedback move the operators relative to the loaded preset, so a preset keeps its character while you push it. Editing a register in Detail re-bases the macros around the new value.
- **RECIPE**: pick a category (Bass, Lead, Brass, E.Piano, Bell, Pad, SE, Any) and six directions, press Generate. Undo and A/B let you compare with what you had.
- **Motion**: the tricks of the era, done for you: Wide (a second chip slightly detuned on the other side, all 8 voices kept), Echo (a delayed, quieter copy on alternating sides), vibrato with delay and rise, timbre LFO, tremolo, pan motion, two-stage pitch envelope for drums, brightness sweep, level envelope, portamento / legato and velocity-to-brightness. Rates can follow the host tempo as note values.
- **Arpeggiator**: Up, Down, Up-Down, Random or As played over the held notes, across up to four octaves, either chip style (one channel, only the pitch changes, down to 1/64) or retriggered with a gate; a chord table (Major, Minor, 7th, ...) turns a single note into a chord, Latch keeps the pattern going, and an accent leans on the beat.
- **OUTPUT**: the current sound is rendered on a private chip and played back visually, waveform over level envelope, from key-on through release. When a patch is silent, it says why.
- **Presets**: 8 factory presets plus a 64-voice collection, import of VOPM `.opm` banks, and saving your own. Bank and preset choice survive with the DAW project.
- **MIDI**: VOPMex-compatible CCs for every register, CCs for the macros and Motion amounts, and an optional expressive mode for the mod wheel and aftertouch.

## Requirements

| Platform | Formats | Needs |
|---|---|---|
| macOS 10.13 or later, Intel or Apple Silicon | Audio Unit, VST3, Standalone | An AU or VST3 host (Logic Pro, GarageBand, Ableton Live, Reaper, ...) |
| Windows 10 or later, 64-bit | VST3 | A VST3 host (Ableton Live, FL Studio, Reaper, ...) |
| Linux, x86_64 | VST3, Standalone | A VST3 host (Reaper, Ardour, Bitwig Studio, ...) |

## Installation

Download the package for your platform from [Releases](https://github.com/hiroaki0923/YMulator-Synth/releases), extract it and copy the plugin to your plugin folder, then rescan or restart the DAW.

| Package | Copy to |
|---|---|
| `YMulator-Synth-macOS-AU.zip` | `~/Library/Audio/Plug-Ins/Components/` (or `/Library/Audio/Plug-Ins/Components/`) |
| `YMulator-Synth-macOS-VST3.zip` | `~/Library/Audio/Plug-Ins/VST3/` (or `/Library/Audio/Plug-Ins/VST3/`) |
| `YMulator-Synth-macOS-Standalone.zip` | `/Applications/` |
| `YMulator-Synth-Windows-VST3.zip` | `C:\Program Files\Common Files\VST3\` |
| `YMulator-Synth-Linux-VST3.tar.gz` | `~/.vst3/` (or `/usr/lib/vst3/`) |
| `YMulator-Synth-Linux-Standalone.tar.gz` | anywhere; run the binary |

If macOS does not list the Audio Unit right away, run `killall -9 AudioComponentRegistrar` or log out and in. To build from source, see [Building](#building).

## Getting started

1. Load YMulator Synth on an instrument track. Use a **stereo** track: Wide, Echo and pan motion put things on the left and right.
2. Pick a bank and a preset in the header. The first bank is the factory set; the second is the collection.
3. Play. Velocity moves the carriers, so quieter notes stay the same colour.
4. Turn the **TONE** knobs to shape the sound. Knobs move by dragging up or down; hold Shift for ten times finer steps, or use the scroll wheel. Hover a control for a tooltip.
5. Press a chip in the **MOTION** card to add movement, and turn its knob for the amount. Several can be on at once.
6. Press **Generate** in the RECIPE card for a new sound in the chosen direction. **Undo** brings the previous one back, **A / B** switches between them.
7. When you want the registers, press **Detail**.

### The Quick view

- **RECIPE**: a category and six sliders (dark / bright, simple / complex, soft / hard attack, short / long, still / moving, harmonic / metallic) describe the sound you want; Generate makes one. It replaces the current sound and centres the TONE knobs; the preset box shows "Generated".
- **TONE**: seven macros over whatever is loaded, preset or generated. Brightness moves the modulator levels, Harmonics chooses a ratio template (Preset, Saw, Square, Pulse, Bright, Bell, Metal, Sub, Octave), Feedback is operator 1's feedback, Attack / Decay / Release move the envelopes of all operators, Spread sets DT1 apart. Loading a preset or generating recentres them.
- **ALGORITHM**: the eight connections as diagrams, with a description and the feedback loop highlighted when feedback is on.
- **MOTION**: a chip per feature (Wide, Vib, Growl, Echo, Sweep, Swell, Glide, Arp, Kick, Trem, Pan); each turns on with sensible values, and the knobs set the amounts. **Sync** makes the rates follow the host tempo; the Vib rate knob then becomes a note-value box. The other note values are in the Detail view.
- **OUTPUT**: the current patch played back visually. The waveform follows a moving playhead over the level envelope, so attack, decay and release are visible.

### The Detail view

- The TONE row stays at the top. Touching a TONE knob puts an amber ring on the operator knobs it moves.
- Each operator row shows its role (MOD or CARRIER, NOISE when the noise generator is on), the three knobs that matter most (Level, Ratio, Detune) in musical units with the register value underneath, the envelope, the five envelope knobs, KS, DT2 and AMS. The switch turns the operator on or off (the .opm SLOT mask).
- The MOTION row has every Motion parameter in four cards: LFO (vibrato with delay, rise and waveform; timbre LFO and tremolo), ENVELOPE (the two-stage pitch envelope; sweep and the level envelope), SPACE (Wide amount and placement, echo level and time; pan mode and step) and PLAY (legato and portamento, velocity brightness; arpeggio mode and step, with a "..." button for the chord table, octaves, retrigger and gate, latch and accent). With Sync on, every rate knob turns into a note-value box.
- The bottom row has the hardware LFO (rate, AMD, PMD, waveform), the noise generator (on / off and frequency) and the Expressive MIDI switch.

### Presets and .opm files

- **Factory** (8): Electric Piano, Synth Bass, Brass Section, String Pad, Lead Synth, Organ, Bells, Init.
- **Collection** (64): classic FM voices.
- **Import**: choose "Import OPM File..." in the Bank box to load a VOPM-format `.opm` bank. Imported banks are copied to `~/Library/YMulator-Synth/banks/` (macOS) and appear in every instance from then on.
- **Save**: after editing, press **Save** to store the sound as a new preset in the User bank. Saved values are exactly what the chip plays.
- The preset box shows an EDITED tag once you change a register, and "Generated" after Generate.

## MIDI

Pitch bend (range 1 to 12 semitones, default 2), velocity on the carriers, channel pressure and CC 1 in expressive mode, and the controllers below. Notes are allocated dynamically across the 8 channels, with the oldest voice stolen when more are needed.

### Controllers (VOPMex compatible)

| CC# | Parameter | Range | Description |
|-----|-----------|-------|-------------|
| 14 | Algorithm | 0-7 | FM algorithm selection |
| 15 | Feedback | 0-7 | Operator 1 feedback level |
| 16-19 | TL OP1-4 | 0-127 | Total Level per operator |
| 20-23 | MUL OP1-4 | 0-15 | Multiple per operator |
| 24-27 | DT1 OP1-4 | 0-7 | Detune 1 per operator |
| 28-31 | DT2 OP1-4 | 0-3 | Detune 2 per operator |
| 39-42 | KS OP1-4 | 0-3 | Key Scale per operator |
| 43-46 | AR OP1-4 | 0-31 | Attack Rate per operator |
| 47-50 | D1R OP1-4 | 0-31 | Decay 1 Rate per operator |
| 51-54 | D2R OP1-4 | 0-31 | Decay 2 Rate per operator |
| 55-58 | D1L OP1-4 | 0-15 | Sustain Level per operator |
| 59-62 | RR OP1-4 | 0-15 | Release Rate per operator |
| 70-73 | AME OP1-4 | 0-1 | AMS enable per operator |
| 1 | LFO Frequency | 0-127 | Upper 7 bits of the 8-bit LFRQ register; CC 33 carries the lowest bit |
| 2 | LFO PMD | 0-127 | Pitch modulation depth |
| 3 | LFO AMD | 0-127 | Amplitude modulation depth |
| 12 | LFO Waveform | 0-3 | Saw / Square / Triangle / Noise |
| 75 | LFO PMS | 0-7 | Pitch modulation sensitivity, all channels |
| 76 | LFO AMS | 0-3 | Amplitude modulation sensitivity, all channels |
| 80 | Noise Enable | 0 / 1-127 | Channel 8 noise on/off |
| 82 | Noise Frequency | 0-31 | NFRQ (81 also accepted) |
| 102-107 | Quick macros | position | Brightness, Harmonics, Attack, Decay, Release, Spread (64 = centre) |
| 110-118 | Motion amounts | position | Wide, Vibrato, Timbre, Echo, Sweep, Swell, Porta, Pitch, Velocity brightness |
| 108 | Arpeggio chord | position | None, Major, Minor, 7th, m7, Maj7, Sus4, Sus2, Dim, Aug, 5th, Octave |
| 109 | Arpeggio octaves | position | 1-4 |
| 119 | Arpeggio gate | position | 10-100 % of the step (with Retrigger) |
| 121 | Reset All Controllers | - | Back to natural mode and macros to centre |

Macro CCs move the sound relative to the values the register CCs set: a register CC re-bases the macro, a macro CC works around that base, so both can be used together. Motion CCs never touch the register parameters. The **Expressive MIDI** switch (Detail view, bottom row) turns CC 1 into vibrato depth and aftertouch into Brightness.

By default (VOPMex "natural" mode) the 0-127 CC value is scaled to the parameter's range, and TL, AR, D1R, D1L, D2R and RR run opposite to the register (CC 127 = loudest / fastest), like an analogue synth. Send NRPN 126/127 with data 127 (CC 99=126, CC 98=127, CC 6=127) to switch to register-value input, where the CC value is the register value; data 0 returns to natural mode.

## Building

### Prerequisites

- CMake 3.22 or later and Git.
- **macOS**: Xcode Command Line Tools (`xcode-select --install`). Optional: `brew install googletest` for the tests.
- **Windows**: Visual Studio 2022 or later with the C++ workload.
- **Linux** (Ubuntu / Debian): `cmake build-essential git libgtest-dev libasound2-dev libjack-jackd2-dev libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev`.

JUCE is fetched by CMake; ymfm is a submodule.

### Build

```bash
git clone --recursive https://github.com/hiroaki0923/YMulator-Synth.git
cd YMulator-Synth
./scripts/build.sh setup     # once
./scripts/build.sh build     # or: debug, release, rebuild, clean
```

On macOS the build copies the AU and VST3 into `~/Library/Audio/Plug-Ins/` so the DAW picks them up. Without the script:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release        # Windows: cmake .. -A x64
cmake --build . --config Release --parallel
```

### Tests

```bash
./scripts/test.sh              # every test, split binaries in parallel
./scripts/test.sh --filter Motion
./scripts/test.sh --auval      # Audio Unit validation, macOS
```

Or from `build/`: `ctest --output-on-failure`, the `bin/YMulatorSynthAU_*Tests` binaries, and `auval -v aumu YMul Hrki`. The tests render audio through the chip, so a change that exposes a parameter without wiring it, or moves the wrong operator, fails a test.

`bin/YMulatorSynthAU_UISnapshot` renders the editor to a PNG without a host (see [tools/README.md](tools/README.md)), and `bin/YMulatorSynthAU_SongRender` renders a MIDI file through the plugin to a WAV.

## Documentation

- [CHANGELOG.md](CHANGELOG.md) (English) / [CHANGELOG_ja.md](CHANGELOG_ja.md) (Japanese)
- [Quick view design](docs/ymulatorsynth-quick-panel-design.md), [Motion design](docs/ymulatorsynth-motion-design.md), [architecture decisions](docs/ymulatorsynth-adr.md), [technical specification](docs/ymulatorsynth-technical-spec.md), [.opm format](docs/ymulatorsynth-vopm-format-spec.md)
- [Development status](docs/ymulatorsynth-development-status.md)

## Contributing

Issues and pull requests are welcome. Fork, branch, keep the tests green (`./scripts/test.sh`), and open a pull request against `main`. See `CLAUDE.md` for the coding rules the code base follows.

## License

GPL v3, see [LICENSE](LICENSE).

- [JUCE](https://juce.com/) - GPL v3 / Commercial
- [ymfm](https://github.com/aaronsgiles/ymfm) - BSD 3-Clause
- `.opm` file compatibility with VOPM by Sam

## Acknowledgments

- Aaron Giles for ymfm
- Sam for VOPM and the .opm format
- The chiptune community for keeping these sounds alive

## Support

- Bug reports: [Issues](https://github.com/hiroaki0923/YMulator-Synth/issues)
- Questions: [Discussions](https://github.com/hiroaki0923/YMulator-Synth/discussions)
