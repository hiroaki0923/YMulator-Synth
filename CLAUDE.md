# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## ⚠️ CRITICAL: ALWAYS READ RELEVANT DOCUMENTATION FIRST

**BEFORE starting ANY development task, you MUST:**

1. **Read the relevant sections in docs/** - These contain detailed specifications and implementation guides
2. **Follow the exact procedures** described in the documentation
3. **Reference the docs/** when making architectural decisions

**Key documents to consult:**

- **Setup/Development**: `docs/ymulatorsynth-implementation-guide.md` section 1.4 (MANDATORY for any setup work)
- **Architecture Decisions**: `docs/ymulatorsynth-adr.md` (consult before making design choices)
- **Technical Specifications**: `docs/ymulatorsynth-technical-spec.md` (for MIDI, parameters, formats)
- **Overall Design**: `docs/ymulatorsynth-design-main.md` (for system architecture)
- **JUCE Implementation Details**: `docs/ymulatorsynth-juce-implementation-details.md` (for JUCEパラメータシステム, MIDI CC, Factory Preset実装)
- **VOPM Format Specification**: `docs/ymulatorsynth-vopm-format-spec.md` (for .opmファイル形式とプリセット管理)
- **Phase 1 Completion Roadmap**: `docs/ymulatorsynth-phase1-completion-roadmap.md` (for 基盤構築完了への具体的手順)

**❌ DO NOT:**
- Skip reading documentation before implementation
- Deviate from documented procedures without justification
- Make architectural decisions without consulting ADRs

**✅ DO:**
- Always reference specific document sections when implementing
- Follow documented naming conventions and structures
- Consult technical specs for exact parameter ranges and formats

## Project Overview

YMulator-Synth is a modern FM synthesis Audio Unit plugin for macOS that emulates classic Yamaha sound chips (YM2151/OPM and YM2608/OPNA). It features a VOPM-like interface and is designed for use in Digital Audio Workstations.

## Development Prerequisites

- macOS 10.13 or later
- Xcode Command Line Tools
- Python 3.8+ (for JUCE build scripts)
- VSCode with CMake Tools extension (recommended)
- Git with submodule support

## Technology Stack

- **Language**: C++17 with Objective-C++ for Audio Unit integration
- **Framework**: JUCE for UI and audio processing (see [ADR-001](docs/ymulatorsynth-adr.md#adr-001-uiフレームワークの選定))
- **Build System**: CMake (3.22+)
- **Audio Format**: Audio Unit v3 (with v2 compatibility) (see [ADR-004](docs/ymulatorsynth-adr.md#adr-004-audio-unitバージョンの選定))
- **FM Emulation**: ymfm library by Aaron Giles (see [ADR-002](docs/ymulatorsynth-adr.md#adr-002-fm音源エミュレーションライブラリの選定))

## Build Commands

```bash
# Initial setup
git submodule update --init --recursive

# Build (Release) - minimal output
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful" || echo "Build failed"

# Build (Debug) - minimal output
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful" || echo "Build failed"

# Build with full output (when debugging build issues)
cmake --build . --parallel

# Clean and rebuild - minimal output
cmake --build . --clean-first --parallel > /dev/null 2>&1 && echo "Clean build successful" || echo "Clean build failed"

# Install
cmake --install .

# Run tests - quiet output, only show result
ctest --output-on-failure --quiet

# Run tests with full output (when debugging test issues)  
ctest --output-on-failure

# Validate Audio Unit - minimal output
auval -a
auval -v aumu ChpS Hrki > /dev/null 2>&1 && echo "auval PASSED" || echo "auval FAILED"

# Validate Audio Unit with full output (when debugging validation issues)
auval -v aumu ChpS Hrki

# Fix Audio Unit registration issues
killall -9 AudioComponentRegistrar

# View Audio Unit logs
log show --predicate 'subsystem == "com.apple.audio.AudioToolbox"' --last 5m
```

**⚠️ FOR ANY SETUP/BUILD WORK: FIRST READ [Implementation Guide Section 1.5](docs/ymulatorsynth-implementation-guide.md#15-開発環境セットアップvscode--cmake) - Contains detailed procedures, exact project structure, and VSCode configuration.**

## Architecture

The project follows a layered architecture with lock-free communication between threads:

1. **Audio Unit Host Interface Layer** - Handles DAW communication
2. **Plugin Core Controller** - Central coordination and state management
3. **Voice Management Layer** - Polyphonic voice allocation
4. **Sound Generation Layer** - ymfm integration for FM synthesis
5. **UI Components Layer** - JUCE-based parameter controls

Key architectural decisions:
- Lock-free threading model for real-time audio processing (see [ADR-009](docs/ymulatorsynth-adr.md#adr-009-スレッドモデルとロックフリー通信の実装方針))
- Double-buffering for parameter synchronization
- Factory pattern for preset management
- Observer pattern for UI updates
- Traditional voice allocation (see [ADR-007](docs/ymulatorsynth-adr.md#adr-007-midiチャンネルとチップ割り当て方式の選定))

For complete architectural overview, see [Design Document](docs/ymulatorsynth-design-main.md).

## Key Implementation Notes

**⚠️ BEFORE implementing any features, READ the relevant documentation sections:**

- **Phase 1 Completion**: **MUST READ** [Phase 1 Roadmap](docs/ymulatorsynth-phase1-completion-roadmap.md) for 残り30%の実装手順
- **JUCE Implementation**: **MUST READ** [JUCE Details](docs/ymulatorsynth-juce-implementation-details.md) for パラメータシステム、MIDI CC、Factory Preset実装
- **VOPM Format**: **MUST READ** [VOPM Spec](docs/ymulatorsynth-vopm-format-spec.md) for .opmファイル形式とプリセット管理
- **Latency Modes**: Ultra Low (64), Balanced (128), Relaxed (256) samples → **MUST READ** [ADR-008](docs/ymulatorsynth-adr.md#adr-008-レイテンシーとcpu使用率のトレードオフ設計)
- **MIDI CC Mapping**: Full VOPMex compatibility → **MUST READ** [Technical Spec Section 1.5](docs/ymulatorsynth-technical-spec.md#15-midi実装仕様)
- **Preset Format**: .opm files with VOPM structure → **MUST READ** [Implementation Guide Section 1.7](docs/ymulatorsynth-implementation-guide.md#17-opmファイルフォーマット仕様)
- **Recording**: S98 format for chiptune player compatibility → **MUST READ** [ADR-003](docs/ymulatorsynth-adr.md#adr-003-音声記録フォーマットの選定)
- **Voice Count**: YM2151 (8 channels), YM2608 (6 FM + 3 SSG channels)
- **Threading Model**: Lock-free real-time processing → **MUST READ** [ADR-009](docs/ymulatorsynth-adr.md#adr-009-スレッドモデルとロックフリー通信の実装方針)

## Performance Targets

- CPU usage: < 15% (Balanced mode, 4-core system)
- Memory footprint: < 50MB
- Latency: < 3ms for parameter updates
- Voice stealing: Automatic when exceeding 8 voices

## Testing

**⚠️ BEFORE writing any tests, READ [Implementation Guide Section 1.6](docs/ymulatorsynth-implementation-guide.md#16-テスト戦略とテストコード) for test strategy and examples.**

- Unit tests: Test individual components (operators, envelopes, LFOs)
- Integration tests: MIDI processing and parameter updates  
- Performance tests: Verify < 3ms latency requirement
- Audio Unit validation: Use `auval` before distribution

## Development Status Tracking

**⚠️ ALWAYS UPDATE [Development Status](docs/ymulatorsynth-development-status.md) when completing features or milestones.**

- Track progress against the implementation plan in [Design Document](docs/ymulatorsynth-design-main.md#3-実装計画)
- Update completion percentages for each phase and task
- Record commit hashes and completion dates
- Note any changes to the original timeline or scope
- Update technical achievements and confirmed functionality

**Progress in Commit Messages:**
- For regular commits: Focus on technical changes only
- For major milestones: Lightly mention progress (e.g., "Complete Phase 1 basic audio implementation")
- Phase completions or significant feature completions warrant brief progress notes
- Avoid detailed progress percentages in commit messages

## Key Project Structure

```
src/
├── PluginProcessor.cpp    # Main audio processing
├── PluginEditor.cpp       # UI implementation
├── dsp/                   # FM synthesis and register management
├── ui/                    # User interface components
├── core/                  # Voice management and core logic
└── utils/                 # Utilities and helper functions
```

## 🎯 Coding Rules and Best Practices

Based on the comprehensive improvements implemented in 2025-06, the following rules MUST be followed for all future development:

### 1. **Constants and Magic Numbers**

**❌ NEVER use magic numbers**
```cpp
// BAD
writeRegister(0x20 + channel, 0xC7);
if (velocity > 127) return;
```

**✅ ALWAYS use named constants from appropriate headers**
```cpp
// GOOD
writeRegister(YM2151Regs::REG_ALGORITHM_FEEDBACK_BASE + channel, 
               YM2151Regs::PAN_CENTER | algorithmValue);
CS_ASSERT_VELOCITY(velocity);
```

**Required headers:**
- `src/dsp/YM2151Registers.h` - ALL hardware registers, masks, constants
- `src/utils/ParameterIDs.h` - ALL parameter IDs and MIDI CC mappings

### 2. **Parameter Management**

**❌ NEVER use string literals for parameter IDs**
```cpp
// BAD
auto param = audioProcessor.getParameter("op1_tl");
```

**✅ ALWAYS use ParamID namespace functions**
```cpp
// GOOD
auto param = audioProcessor.getParameter(ParamID::Op::tl(1));
```

### 3. **Debug Output and Assertions**

**❌ NEVER use raw DBG() or std::cout**
```cpp
// BAD
DBG("Setting operator parameter");
std::cout << "Channel: " << channel << std::endl;
```

**✅ ALWAYS use CS_* macros from Debug.h**
```cpp
// GOOD
CS_DBG("Setting operator parameter for op " + juce::String(operatorNum));
CS_ASSERT_CHANNEL(channel);
CS_ASSERT_OPERATOR(operatorNum);
```

**Required assertions for all functions:**
- `CS_ASSERT_CHANNEL(ch)` - for channel parameters (0-7)
- `CS_ASSERT_OPERATOR(op)` - for operator parameters (0-3) 
- `CS_ASSERT_NOTE(note)` - for MIDI note numbers (0-127)
- `CS_ASSERT_VELOCITY(vel)` - for MIDI velocities (0-127)
- `CS_ASSERT_PARAMETER_RANGE(val, min, max)` - for parameter validation

### 4. **UI Component Implementation**

**❌ NEVER create individual unique_ptr members for repetitive UI controls**
```cpp
// BAD - Individual members for each control
std::unique_ptr<juce::Slider> totalLevelSlider;
std::unique_ptr<juce::Label> totalLevelLabel;
std::unique_ptr<juce::Slider> attackRateSlider;
// ... 50+ more lines
```

**✅ ALWAYS use data-driven approaches with specification structures**
```cpp
// GOOD - Data-driven with ControlSpec
struct ControlSpec {
    std::string paramIdSuffix;
    std::string labelText;
    int minValue, maxValue, defaultValue;
    int column, row;
};

static const std::vector<ControlSpec> controlSpecs = {
    {"_tl", "TL", 0, 127, 0, 0, 0},
    {"_ar", "AR", 0, 31, 31, 0, 1},
    // ...
};

std::vector<ControlPair> controls;  // Container-based storage
```

### 5. **Performance-Critical UI Updates**

**❌ NEVER update UI indiscriminately on every parameter change**
```cpp
// BAD - Updates on every property change
void valueTreePropertyChanged(...) {
    updateUI();  // Called for every parameter!
}
```

**✅ ALWAYS filter property changes by relevance**
```cpp
// GOOD - Filtered updates
void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
    static const std::set<std::string> relevantProperties = {
        "presetIndex", "isCustomMode"
    };
    
    if (relevantProperties.find(property.toString().toStdString()) == relevantProperties.end()) {
        CS_DBG("Filtered out irrelevant property: " + property.toString());
        return;
    }
    
    updateUI();  // Only for relevant changes
}
```

### 6. **Audio Processing**

**❌ NEVER hardcode audio channel handling**
```cpp
// BAD
outputBuffer[i] = leftSample;
buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);  // Mono to stereo copy
```

**✅ ALWAYS support true stereo with proper channel separation**
```cpp
// GOOD
void generateSamples(float* leftBuffer, float* rightBuffer, int numSamples) {
    for (int i = 0; i < numSamples; i++) {
        leftBuffer[i] = opmOutput.data[0] / YM2151Regs::SAMPLE_SCALE_FACTOR;
        rightBuffer[i] = opmOutput.data[1] / YM2151Regs::SAMPLE_SCALE_FACTOR;
    }
}
```

### 7. **Error Handling and Validation**

**✅ ALWAYS validate inputs at function entry points**
```cpp
void setOperatorParameter(uint8_t channel, uint8_t operator_num, 
                         OperatorParameter param, uint8_t value) {
    CS_ASSERT_CHANNEL(channel);
    CS_ASSERT_OPERATOR(operator_num);
    CS_ASSERT_PARAMETER_RANGE(value, 0, getMaxValueForParam(param));
    
    // Implementation...
}
```

### 8. **Code Organization**

**✅ ALWAYS follow the established namespace and file structure:**
- **YM2151Regs** namespace - Hardware constants only
- **ParamID** namespace - Parameter ID management only  
- **utils/** directory - Reusable utilities and helpers
- **dsp/** directory - Audio processing and synthesis
- **ui/** directory - User interface components
- **core/** directory - Voice management and core logic

### 9. **Documentation and Comments**

**❌ NEVER add implementation comments unless specifically requested**
```cpp
// BAD - Unnecessary implementation comments
// This function sets the operator parameter
void setOperatorParameter(...) {
    // Check if channel is valid
    if (channel >= 8) return;
    // Set the parameter...
}
```

**✅ Comments only for complex algorithms or hardware-specific logic**
```cpp
// GOOD - Hardware documentation
uint8_t kc = (fnum >> YM2151Regs::SHIFT_KEY_CODE) & YM2151Regs::MASK_KEY_CODE;
// YM2151 KC register: upper 7 bits of frequency number
```

### 10. **Build and Testing Requirements**

**✅ MANDATORY for every commit:**
```bash
# Build verification
cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful"

# Audio Unit validation  
auval -v aumu ChpS Hrki > /dev/null 2>&1 && echo "auval PASSED"
```

**🔒 These rules are derived from proven improvements that enhanced code quality, reduced bugs, and improved maintainability. Deviation requires explicit justification and documentation.**