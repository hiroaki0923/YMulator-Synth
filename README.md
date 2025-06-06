# ChipSynth AU

A modern FM synthesis Audio Unit plugin for macOS, bringing the authentic sound of classic YM2151 (OPM) and YM2608 (OPNA) chips to your DAW with a familiar VOPM-like interface.

![ChipSynth AU Screenshot](docs/images/screenshot.png)

## Features

### 🎹 Authentic Chip Emulation
- **YM2151 (OPM)**: 8-channel FM synthesis from X68000 and arcade systems
- **YM2608 (OPNA)**: 6 FM channels + 3 SSG channels from PC-88VA/PC-98
- High-precision emulation for genuine retro sound

### 🎚️ VOPM-Compatible Interface
- Familiar 4-operator FM synthesis controls
- Real-time envelope visualization
- Drag-and-drop envelope editing
- Per-operator parameter adjustment

### 🎵 Professional Features
- **Full MIDI CC Support**: VOPMex-compatible CC mapping
- **Preset Management**: Load/save .opm format patches
- **S98 Recording**: Export your performances for chipmusic players
- **Low Latency**: Optimized for real-time performance (< 3ms)
- **ADPCM Support**: Load WAV samples for OPNA ADPCM channel

### 🔧 Modern Workflow
- VST3 and AU formats
- 64-bit native processing
- Retina display support
- Full automation support

## Requirements

- macOS 10.13 or later
- Audio Unit v3 compatible DAW (Logic Pro, Ableton Live, etc.)
- 64-bit Intel or Apple Silicon processor

## Installation

### Download Release
1. Download the latest release from the [Releases](https://github.com/hiroaki0923/ChipSynth-AU/releases) page
2. Open the DMG file
3. Copy `ChipSynth AU.component` to `/Library/Audio/Plug-Ins/Components/`
4. Restart your DAW

### Build from Source
See [Building](#building) section below.

## Quick Start

1. **Load the plugin** in your DAW's instrument track
2. **Choose a preset** from the built-in library or load an .opm file
3. **Play** using your MIDI keyboard or DAW's piano roll
4. **Adjust parameters** using the knobs and sliders
5. **Save your patch** in .opm format for sharing

## Building

### Prerequisites
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake
brew install cmake
```

### Clone and Build
```bash
# Clone repository with submodules
git clone --recursive https://github.com/hiroaki0923/ChipSynth-AU.git
cd ChipSynth-AU

# Create build directory
mkdir build && cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Install (copies to ~/Library/Audio/Plug-Ins/Components/)
cmake --install .
```

### Development with VSCode
1. Open the project folder in VSCode
2. Install recommended extensions when prompted
3. Use CMake Tools extension for building and debugging

## Technical Specifications

### Audio Processing
- Sample rates: 44.1, 48, 88.2, 96 kHz
- Buffer sizes: 64-4096 samples
- Internal processing: 32-bit float
- Polyphony: 8 voices (OPM) / 6+3 voices (OPNA)

### MIDI Implementation
| CC# | Parameter | Range | Description |
|-----|-----------|-------|-------------|
| 14 | Algorithm | 0-7 | FM algorithm selection |
| 15 | Feedback | 0-7 | Operator 1 feedback level |
| 16-19 | TL OP1-4 | 0-127 | Total Level per operator |
| 43-46 | AR OP1-4 | 0-31 | Attack Rate per operator |
| 75 | PMS | 0-7 | Pitch Modulation Sensitivity |
| 76 | AMS | 0-3 | Amplitude Modulation Sensitivity |

See [docs/MIDI_Implementation.md](docs/MIDI_Implementation.md) for complete CC mapping.

## File Formats

### .opm Format
ChipSynth AU can load and save voice patches in VOPM .opm format:
```
@:0 Piano
LFO: 0 0 0 0 0
CH: 64 7 2 0 0 120 0
M1: 31 5 0 0 0 20 0 1 0 0 0
...
```

### S98 Export
Record your performances and export as S98 files for playback in:
- foobar2000 + foo_input_s98
- WinAMP + in_s98
- Various chipmusic players

## Contributing

We welcome contributions! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development Setup
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Testing
```bash
# Run unit tests
cd build
ctest --output-on-failure

# Run Audio Unit validation
auval -v aufx Ymfm Manu
```

## License

This project is licensed under the MIT License - see [LICENSE](LICENSE) file for details.

### Third-party Libraries
- [JUCE](https://juce.com/) - GPL v3 / Commercial
- [ymfm](https://github.com/aaronsgiles/ymfm) - BSD 3-Clause
- VOPM compatibility inspired by [VOPM](http://www.geocities.jp/sam_kb/VOPM/) by Sam

## Acknowledgments

- Aaron Giles for the amazing ymfm emulation library
- Sam for the original VOPM that inspired this project
- The chiptune community for keeping these sounds alive
- Contributors and testers who helped shape this plugin

## Support

- **Documentation**: [Wiki](https://github.com/hiroaki0923/ChipSynth-AU/wiki)
- **Bug Reports**: [Issues](https://github.com/hiroaki0923/ChipSynth-AU/issues)
- **Discussions**: [Discussions](https://github.com/hiroaki0923/ChipSynth-AU/discussions)

