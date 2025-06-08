# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⚠️ CRITICAL: ALWAYS READ RELEVANT DOCUMENTATION FIRST

**BEFORE starting ANY development task, you MUST:**

1. **Read the relevant sections in docs/** - These contain detailed specifications and implementation guides
2. **Follow the exact procedures** described in the documentation
3. **Reference the docs/** when making architectural decisions

**Key documents to consult:**

- **Setup/Development**: `docs/chipsynth-implementation-guide.md` section 1.4 (MANDATORY for any setup work)
- **Architecture Decisions**: `docs/chipsynth-adr.md` (consult before making design choices)
- **Technical Specifications**: `docs/chipsynth-technical-spec.md` (for MIDI, parameters, formats)
- **Overall Design**: `docs/chipsynth-design-main.md` (for system architecture)

**❌ DO NOT:**
- Skip reading documentation before implementation
- Deviate from documented procedures without justification
- Make architectural decisions without consulting ADRs

**✅ DO:**
- Always reference specific document sections when implementing
- Follow documented naming conventions and structures
- Consult technical specs for exact parameter ranges and formats

## Project Overview

ChipSynth-AU is a modern FM synthesis Audio Unit plugin for macOS that emulates classic Yamaha sound chips (YM2151/OPM and YM2608/OPNA). It features a VOPM-like interface and is designed for use in Digital Audio Workstations.

## Development Prerequisites

- macOS 10.13 or later
- Xcode Command Line Tools
- Python 3.8+ (for JUCE build scripts)
- VSCode with CMake Tools extension (recommended)
- Git with submodule support

## Technology Stack

- **Language**: C++17 with Objective-C++ for Audio Unit integration
- **Framework**: JUCE for UI and audio processing (see [ADR-001](docs/chipsynth-adr.md#adr-001-uiフレームワークの選定))
- **Build System**: CMake (3.22+)
- **Audio Format**: Audio Unit v3 (with v2 compatibility) (see [ADR-004](docs/chipsynth-adr.md#adr-004-audio-unitバージョンの選定))
- **FM Emulation**: ymfm library by Aaron Giles (see [ADR-002](docs/chipsynth-adr.md#adr-002-fm音源エミュレーションライブラリの選定))

## Build Commands

```bash
# Initial setup
git submodule update --init --recursive

# Build (Release)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Build (Debug)
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Clean and rebuild
cmake --build . --clean-first

# Install
cmake --install .

# Run tests
ctest

# Validate Audio Unit
auval -a
auval -v aufx ChpS Vend

# Fix Audio Unit registration issues
killall -9 AudioComponentRegistrar

# View Audio Unit logs
log show --predicate 'subsystem == "com.apple.audio.AudioToolbox"' --last 5m
```

**⚠️ FOR ANY SETUP/BUILD WORK: FIRST READ [Implementation Guide Section 1.5](docs/chipsynth-implementation-guide.md#15-開発環境セットアップvscode--cmake) - Contains detailed procedures, exact project structure, and VSCode configuration.**

## Architecture

The project follows a layered architecture with lock-free communication between threads:

1. **Audio Unit Host Interface Layer** - Handles DAW communication
2. **Plugin Core Controller** - Central coordination and state management
3. **Voice Management Layer** - Polyphonic voice allocation
4. **Sound Generation Layer** - ymfm integration for FM synthesis
5. **UI Components Layer** - JUCE-based parameter controls

Key architectural decisions:
- Lock-free threading model for real-time audio processing (see [ADR-009](docs/chipsynth-adr.md#adr-009-スレッドモデルとロックフリー通信の実装方針))
- Double-buffering for parameter synchronization
- Factory pattern for preset management
- Observer pattern for UI updates
- Traditional voice allocation (see [ADR-007](docs/chipsynth-adr.md#adr-007-midiチャンネルとチップ割り当て方式の選定))

For complete architectural overview, see [Design Document](docs/chipsynth-design-main.md).

## Key Implementation Notes

**⚠️ BEFORE implementing any features, READ the relevant documentation sections:**

- **Latency Modes**: Ultra Low (64), Balanced (128), Relaxed (256) samples → **MUST READ** [ADR-008](docs/chipsynth-adr.md#adr-008-レイテンシーとcpu使用率のトレードオフ設計)
- **MIDI CC Mapping**: Full VOPMex compatibility → **MUST READ** [Technical Spec Section 1.5](docs/chipsynth-technical-spec.md#15-midi実装仕様)
- **Preset Format**: .opm files with VOPM structure → **MUST READ** [Implementation Guide Section 1.7](docs/chipsynth-implementation-guide.md#17-opmファイルフォーマット仕様)
- **Recording**: S98 format for chiptune player compatibility → **MUST READ** [ADR-003](docs/chipsynth-adr.md#adr-003-音声記録フォーマットの選定)
- **Voice Count**: YM2151 (8 channels), YM2608 (6 FM + 3 SSG channels)
- **Threading Model**: Lock-free real-time processing → **MUST READ** [ADR-009](docs/chipsynth-adr.md#adr-009-スレッドモデルとロックフリー通信の実装方針)

## Performance Targets

- CPU usage: < 15% (Balanced mode, 4-core system)
- Memory footprint: < 50MB
- Latency: < 3ms for parameter updates
- Voice stealing: Automatic when exceeding 8 voices

## Testing

**⚠️ BEFORE writing any tests, READ [Implementation Guide Section 1.6](docs/chipsynth-implementation-guide.md#16-テスト戦略とテストコード) for test strategy and examples.**

- Unit tests: Test individual components (operators, envelopes, LFOs)
- Integration tests: MIDI processing and parameter updates  
- Performance tests: Verify < 3ms latency requirement
- Audio Unit validation: Use `auval` before distribution

## Key Project Structure

```
src/
├── PluginProcessor.cpp    # Main audio processing
├── PluginEditor.cpp       # UI implementation
├── ymfm/                  # FM synthesis wrapper
└── dsp/                   # Voice allocation
```