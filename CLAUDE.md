# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Documentation: what to read, and how to keep it true

Read before changing the area in question. Every document below is kept in step with the code; if you find one that is not, fix the document in the same change.

| Area | Document |
|---|---|
| Architecture (components, threads, data flow, UI map) | `docs/ymulatorsynth-architecture.md` |
| YM2151 register facts (with manual figure numbers and the tests that pin them) | `docs/ym2151-register-facts.md` |
| MIDI CC / NRPN, parameter list and ranges, conversion rules, voices, noise | `docs/ymulatorsynth-technical-spec.md` |
| Quick / Detail UI, TONE macros, generator | `docs/ymulatorsynth-quick-panel-design.md` |
| MOTION features (vibrato, wide, echo, pan, arpeggiator, ...) | `docs/ymulatorsynth-motion-design.md` |
| .opm file format, banks, presets | `docs/ymulatorsynth-vopm-format-spec.md` |
| ymfm usage and OPM sound-design tips | `docs/ymulatorsynth-ymfm-integration-guide.md` |
| Setup, build, test strategy, auval / AU troubleshooting | `docs/ymulatorsynth-implementation-guide.md` |
| Decisions and their status (ADR-001 ... ADR-011) | `docs/ymulatorsynth-adr.md` |
| What shipped when | `CHANGELOG.md` / `CHANGELOG_ja.md`, `docs/ymulatorsynth-development-status.md` |
| Parameter access lessons (a real crash) | `docs/CRASH_PREVENTION_LESSONS.md` |

**Documentation rules (these exist because a year of drift cost real bugs):**

1. **Facts need a source.** A register layout, a file-format detail, a chip behaviour: cite the manual figure, the ymfm code, or the test that pins it. Do not write a hardware fact from memory. If a fact is not in `docs/ym2151-register-facts.md`, add it there with its source before relying on it. OPM differs from OPN: slot address order (M1, M2, C1, C2) differs from key-on bit order (M1, C1, M2, C2), note codes start at C# with gaps, PMD/AMD share register 0x19, noise is slot 32 only.
2. **Tables come from code.** CC tables, parameter lists and ranges, test-binary lists are derived from `src/utils/ParameterIDs.h`, `src/core/ParameterManager.cpp` and `tests/CMakeLists.txt`. Never type them by hand.
3. **Code changes carry their doc changes.** Adding, removing or renaming a parameter, a CC, a component, a UI element or a test binary means updating the document in the table above in the same commit or PR. A PR that changes `src/` without touching the affected doc is incomplete.
4. **State the status.** A design document says whether each part is planned, implemented (with the version) or removed (with the version), and carries a date. Never describe a plan in the present tense. An ADR that was never implemented gets the status "未実装（保留）".
5. **"Done" means tested.** Do not record a feature as complete unless a test verifies the behaviour (not merely that a parameter or a control exists). Do not write progress percentages anywhere.
6. **Do not describe what is not there.** Only the YM2151 (OPM) is implemented. There is no OPNA, SSG, ADPCM, S98 recording, latency mode, per-channel pan or Global Pan. Do not reintroduce these from old text.
7. **Before a release**, grep the documents for class names, parameter ids and test names and confirm they exist in the tree; fix or delete what does not.

## Project Overview

YMulator-Synth is an FM synthesizer plugin that emulates the Yamaha YM2151 (OPM) with ymfm: 8 voices, VOPMex-compatible MIDI CC, .opm presets, a Quick view (relative TONE macros, a patch generator, MOTION effects written as chip registers) and a Detail view (every register). Formats AU, AUv3, VST3 and Standalone on macOS, Windows and Linux. Only the OPM is implemented.

## Development Prerequisites

- macOS: Xcode Command Line Tools (AU, AUv3, VST3, Standalone). Windows: Visual Studio 2022 (VST3, Standalone). Linux: the packages listed in README.md (VST3, Standalone)
- CMake 3.22+, Git with submodule support (ymfm is a submodule; JUCE is fetched by CMake)
- Google Test for the test targets

## Technology Stack

- **Language**: C++17 with Objective-C++ for Audio Unit integration
- **Framework**: JUCE for UI and audio processing (see [ADR-001](docs/ymulatorsynth-adr.md#adr-001-uiフレームワークの選定))
- **Build System**: CMake (3.22+)
- **Plugin formats**: AU (v2 + v3), VST3, Standalone (see [ADR-004](docs/ymulatorsynth-adr.md#adr-004-audio-unitバージョンの選定)); JUCE 9.0.1 fetched by CMake (`cmake/JUCEConfig.cmake`)
- **FM Emulation**: ymfm library by Aaron Giles (see [ADR-002](docs/ymulatorsynth-adr.md#adr-002-fm音源エミュレーションライブラリの選定))

## Build Commands

**⚠️ IMPORTANT: Use the provided build scripts for consistent, cross-platform builds.**

### Quick Start

```bash
# Initial setup (run once)
./scripts/build.sh setup

# Build the project
./scripts/build.sh build

# Run all tests
./scripts/test.sh

# Audio Unit validation
./scripts/test.sh --auval
```

### Build Script Usage

```bash
# Show all available commands
./scripts/build.sh --help

# Common operations
./scripts/build.sh setup           # Initial project setup
./scripts/build.sh build           # Build project
./scripts/build.sh rebuild         # Clean and rebuild
./scripts/build.sh clean           # Clean build directory
./scripts/build.sh debug           # Debug build
./scripts/build.sh install         # Install plugin
./scripts/build.sh test            # Run tests
./scripts/build.sh auval           # Audio Unit validation

# Build options
./scripts/build.sh build --quiet   # Suppress output
./scripts/build.sh build --jobs 8  # Use 8 parallel jobs
```

### Test Script Usage

```bash
# Show all available test options
./scripts/test.sh --help

# Test categories
./scripts/test.sh --all             # Run all tests (default)
./scripts/test.sh --unit            # Unit tests only
./scripts/test.sh --integration     # Integration tests only
./scripts/test.sh --regression      # Regression tests only

# Specific test filtering
./scripts/test.sh --filter "ParameterManager"  # Tests matching pattern
./scripts/test.sh --list                       # List available tests
./scripts/test.sh ParameterManager             # Shorthand for filter

# Test options
./scripts/test.sh --build --verbose  # Build then run tests verbosely
./scripts/test.sh --quiet            # Minimal output
```

### Manual Build (Advanced)

If you need manual control over the build process:

```bash
# Prerequisites check
cmake --version  # Should be 3.22+
git --version    # For submodules

# Initial setup
git submodule update --init --recursive

# Configure and build
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel

# Run tests
ctest --output-on-failure

# Audio Unit validation
auval -v aumu YMul Hrki
```

### Troubleshooting

```bash
# Fix Audio Unit registration issues
killall -9 AudioComponentRegistrar

# View Audio Unit logs
log show --predicate 'subsystem == "com.apple.audio.AudioToolbox"' --last 5m

# Common issues:
# - "cmake: command not found" → Install CMake: brew install cmake
# - Build errors → Try: ./scripts/build.sh clean && ./scripts/build.sh rebuild
# - Test failures → Check: ./scripts/test.sh --verbose
```

**For setup and build work, read the setup section of `docs/ymulatorsynth-implementation-guide.md` first.** Building the plugin targets copies the plug-ins into the user plug-in folders unless configured with `-DYMULATOR_COPY_PLUGIN=OFF`; do not build them while a host has the plugin open.

## Architecture

`docs/ymulatorsynth-architecture.md` is the reference. In short: `PluginProcessor` owns the chip wrapper (`YmfmWrapper`, main chip + shadow chip, native 55,930 Hz resampled to the host) and delegates to `MidiProcessor`, `ParameterManager`, `StateManager`, `MacroMapper`, `PatchWorkspace` (generator, undo, preview), `MotionEngine` (64-sample control-rate register writes) and `VoiceManager`; `PresetManager` handles banks and .opm files. The UI is `MainComponent` hosting the Quick view (`QuickView`) and the Detail view (`ToneStrip`, `OperatorPanel` x4, `MotionStrip`, `LfoNoiseStrip`).

Rules that follow from it:
- The audio thread does MIDI, motion ticks, register writes and rendering; no allocation, no file I/O, no locks (see [ADR-009](docs/ymulatorsynth-adr.md)).
- Voices are allocated dynamically (oldest-note stealing, noise presets prefer channel 7; [ADR-007](docs/ymulatorsynth-adr.md)), so nothing may be keyed to a fixed hardware channel.
- Everything a MOTION feature does is a register write; no post-processing of the chip output (`docs/ymulatorsynth-motion-design.md`).
- Macros are relative to an anchor and never overwrite the raw registers wholesale ([ADR-010](docs/ymulatorsynth-adr.md)).

## Key Implementation Notes

- **MIDI CC**: VOPMex numbering, CC value = register value; Quick macros 102-107, MOTION 110-118, arpeggio 108/109/119, CC 121 recentres the macros. Table and NRPN in `docs/ymulatorsynth-technical-spec.md`; constants in `src/utils/ParameterIDs.h`.
- **Presets**: .opm holds raw registers only; macros, motion and the macro anchor live in the plugin state (`docs/ymulatorsynth-vopm-format-spec.md`).
- **Registers**: `src/dsp/YM2151Registers.h` is the only place for register addresses, masks and tables; `docs/ym2151-register-facts.md` explains each with its source.
- **Voices**: 8 channels, YM2151 only.

## Performance Targets

- CPU usage: < 15% on a 4-core system at 128-sample blocks
- Memory footprint: < 50MB
- Parameter change to chip: within one 64-sample motion tick
- Voice stealing: automatic beyond 8 voices

## Testing

Read the test strategy section of `docs/ymulatorsynth-implementation-guide.md` before writing tests. Expectations come from the YM2151 manual, the reference implementation and the format specs, never from the current behaviour of the code. Register facts have their own tests (`RegisterGoldenTest`, `OperatorSlotOrderTest`, `SlotEnableTest`, `LfoWiringTest`, `PitchAccuracyTest`, `VelocityTest`, `ParameterReachTest`); a new register fact gets a new test.

## Development Status Tracking

- Release changelog lives in `CHANGELOG.md` (English) and `CHANGELOG_ja.md` (Japanese), not in the READMEs. Every user-visible change goes into the Unreleased section of both in the same PR.
- `docs/ymulatorsynth-development-status.md` records what shipped in each version and what was verified how. No percentages, no "complete" without a test.
- Commit messages describe the technical change; they do not carry progress claims.

## Key Project Structure

```
src/
├── PluginProcessor.cpp    # Host contract; delegates to core/
├── PluginEditor.cpp       # Creates MainComponent
├── core/                  # MidiProcessor, ParameterManager, StateManager, MacroMapper, PatchWorkspace,
│                          # PatchGenerator, SnapshotStore, PatchPreview, MotionEngine, VoiceManager, HeldNotes
├── dsp/                   # YmfmWrapper (+ shadow chip, resampler), YM2151Registers.h, AlgorithmInfo.h
├── ui/                    # MainComponent, QuickView, ToneStrip, OperatorPanel, MotionStrip, MotionPanel, ...
└── utils/                 # ParameterIDs.h, PresetManager, VOPMParser, Debug.h
tests/                     # 9 gtest binaries (tests/CMakeLists.txt); test_main isolates user data
tools/                     # ui_snapshot (editor to PNG), song_render (MIDI to WAV), gen_algorithm_svg.py
docs/                      # see the table at the top
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
# Build verification (MUST run from correct path)
cd /Users/hiroaki.kimura/projects/ChipSynth-AU/build && cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful" || echo "Build failed"

# Audio Unit validation (can run from any directory)
auval -v aumu YMul Hrki > /dev/null 2>&1 && echo "auval PASSED" || echo "auval FAILED"
```

**⚠️ PATH CRITICAL:** Always ensure you're in `/Users/hiroaki.kimura/projects/ChipSynth-AU/build/` for build commands. If you get "No rule to make target 'Makefile'" error, you're in the wrong directory.

**🔒 These rules are derived from proven improvements that enhanced code quality, reduced bugs, and improved maintainability. Deviation requires explicit justification and documentation.**

## 🏗️ Refactoring Guidelines and Architecture Principles

### **⚠️ CRITICAL LESSON: Always Maintain Test Coverage During Refactoring**

The 2025-06 extraction of `MidiProcessor`, `ParameterManager` and `StateManager` from a 1,400-line `PluginProcessor` worked because each step was small and tested. The 2026-09 work (MotionEngine, MacroMapper, the pan rework) followed the same pattern.

**✅ CORRECT Process:**
1. **Run the relevant test binaries** → establish a baseline (all passing)
2. **Extract one responsibility at a time** → e.g. one engine, one manager
3. **Test immediately** → after each extraction
4. **Fix issues before proceeding**
5. **Commit small, atomic changes**
6. **Update the architecture document and the design document of the area** in the same change

**❌ WRONG Process:**
1. Extracting several components at once
2. Skipping intermediate testing
3. Large, monolithic commits
4. Living with flaky tests

#### **Component Extraction Best Practices:**

```cpp
// ✅ GOOD - one responsibility, dependencies injected through interfaces
class MotionEngine {
public:
    MotionEngine(YmfmWrapperInterface& ymfm, VoiceManagerInterface& voices);
    void bindParameters(juce::AudioProcessorValueTreeState& parameters);
    void tick(int numSamples);
private:
    YmfmWrapperInterface& ymfm;          // interface, not the concrete wrapper
    VoiceManagerInterface& voices;
};
```

```cpp
// ❌ BAD - mixed responsibilities
class AudioManager {
public:
    void handleMIDI(const MidiMessage& msg);
    void applyPan(int channel, float pan);
    void loadPreset(const Preset& preset);
    void processAudio(AudioBuffer& buffer);
};
```

#### **Dependency Injection Patterns:**

Constructors take interfaces (`YmfmWrapperInterface`, `VoiceManagerInterface`, `MidiProcessorInterface`, `PresetManagerInterface`) so tests can substitute mocks; nothing constructs its own concrete collaborators.

#### **Test Stability Guidelines:**

Randomised behaviour (the Random pan mode, the Random arpeggio order, the random LFO wave) uses a deterministic generator seeded per channel or per instance, and the tests assert the properties (a new note never lands where the last one did) rather than exact sequences.

#### **Split Test Binaries:**

`tests/CMakeLists.txt` builds nine binaries (Basic, Preset, Parameter, Pan, Integration, UI, Quality, Performance and the unified `YMulatorSynthAU_Tests`). Run the binary that covers the area you changed while working; CI runs all of them on macOS and Linux.

## 🧪 Testing Best Practices and Critical Lessons

### **⚠️ CRITICAL LESSON: Never Modify Tests to Hide Implementation Issues**

Based on hard-learned experience during DAW-independent testing framework development:

#### **The Wrong Approach:**
```cpp
// ❌ BAD - Changing test to accommodate unexpected behavior
TEST_F(ParameterTest, OperatorTest) {
    // Originally tested op0_tl, but it returned 0, so changed to op1_tl
    auto param = ParamID::Op::tl(1);  // Changed from 0 to 1
    // ...
}
```

#### **The Right Approach:**
1. **ALWAYS investigate implementation first** when tests fail
2. **Understand the design specification** before assuming bugs
3. **Consult industry standards** (VOPM, YM2151 documentation)
4. **Only modify tests after confirming the implementation behavior is correct**

#### **Real Example from YMulator-Synth:**

**Initial Assumption (WRONG):** "Op0 parameters are broken, returning 0"
**Reality (CORRECT):** YMulator-Synth uses **1-based operator indexing** (Op1-Op4), not 0-based (Op0-Op3)

**Why 1-based indexing is correct:**
- **VOPM compatibility**: VOPM software uses OP1-OP4 labeling
- **YM2151 standard**: Industry documentation refers to Operators 1-4
- **User expectations**: UI displays "Operator 1", "Operator 2", etc.
- **MIDI CC mapping**: Standard uses Op1_TL, Op2_TL, etc.

#### **Testing Methodology:**

**✅ CORRECT Process:**
1. **Test fails** → Investigate root cause
2. **Check specification** → Consult docs/, ADRs, industry standards
3. **Understand design intent** → Verify if behavior is intentional
4. **Fix implementation OR update test** → Based on specification, not convenience

**❌ WRONG Process:**
1. **Test fails** → Immediately change test to pass
2. **Skip investigation** → Assume implementation is correct
3. **Hide potential bugs** → Tests become meaningless

### **⚠️ CRITICAL LESSON 2: Never Use Tolerance Without Technical Justification**

From state save/restore testing experience, another critical anti-pattern was discovered:

#### **The Wrong Approach:**
```cpp
// ❌ BAD - Adding tolerance to "fix" failing tests without investigation
TEST_F(StateSaveRestoreTest, ParameterPersistence) {
    setParameterValue(0.75f);
    auto state = saveState();
    loadState(state);
    // Test was failing, so added tolerance instead of investigating
    EXPECT_NEAR(getParameterValue(), 0.75f, 0.1f);  // ❌ WRONG
}
```

#### **The Root Cause Analysis:**
The test was **designed incorrectly**, not the implementation:

```cpp
// WRONG: Setting parameters AFTER loading preset overwrites values
setParameterValue(0.75f);      // Set to 0.75
setCurrentProgram(3);          // Preset loading overwrites to preset value!
// Result: 0.75f → preset value (e.g., 0.285714f)
```

#### **The Correct Approach:**
```cpp
// ✅ GOOD - Proper test design with exact expectations
TEST_F(StateSaveRestoreTest, ParameterPersistence) {
    setCurrentProgram(3);                    // Load preset first
    setParameterValueWithGesture(0.5f);      // Then modify to create known state
    
    float originalValue = getParameterValue();  // Capture actual quantized value
    auto state = saveState();
    changeState();                           // Make changes
    loadState(state);                        // Restore
    
    // State save/restore must be EXACT - no tolerance
    EXPECT_FLOAT_EQ(getParameterValue(), originalValue);  // ✅ CORRECT
}
```

#### **When Tolerance IS Justified:**

**✅ Hardware Quantization (YM2151 registers):**
```cpp
// Algorithm parameter has only 8 discrete values (0-7)
EXPECT_NEAR(getAlgorithm(), expectedAlgorithm, 0.05f);  // Hardware limitation
```

**✅ Floating-Point Precision:**
```cpp
// Mathematical calculations with inherent precision limits
EXPECT_NEAR(calculateFrequency(note), expectedFreq, 0.001f);  // Math precision
```

**❌ NEVER Use Tolerance For:**
- State save/restore operations (must be exact)
- Digital parameter storage (no precision loss)
- Boolean or discrete value checks
- "Fixing" test design problems

#### **Critical Rules:**
1. **EXPECT_FLOAT_EQ()** for exact digital operations
2. **EXPECT_NEAR()** only with documented technical justification
3. **Document the reason** for any tolerance in comments
4. **Investigate test design** before adding tolerance
5. **Never use tolerance to hide implementation bugs**

**"根拠なく幅を持たせて値をチェックしているテストケースは実装の問題を隠す"** - Always justify tolerance with technical reasoning.

#### **YMulator-Synth Specific Design Rules:**

**Parameter Indexing:**
```cpp
// ✅ CORRECT - Operators 1-4 exist
ParamID::Op::tl(1)  // Operator 1 Total Level
ParamID::Op::tl(2)  // Operator 2 Total Level
ParamID::Op::tl(3)  // Operator 3 Total Level
ParamID::Op::tl(4)  // Operator 4 Total Level

// ❌ WRONG - Operator 0 does NOT exist in this system
ParamID::Op::tl(0)  // Returns 0/default - not a bug, by design
```

**Parameter Quantization:**
```cpp
// ✅ Account for hardware-accurate quantization
EXPECT_NEAR(value, expected, 0.05f);  // Allow for YM2151 discrete values

// ❌ Don't expect perfect floating-point precision
EXPECT_EQ(value, 0.75f);  // Will fail due to quantization
```

### **🎯 Testing Framework Location:**

`tests/` contains `MockAudioProcessorHost` (a host without a DAW), unit tests per component (`tests/unit`), UI tests (`tests/ui`, need a display or xvfb), integration, quality and performance tests. `tests/test_main.cpp` redirects user data to a temporary directory so tests never touch the user's banks.

**Running Tests:**
```bash
# Build the test targets only (the plugin targets copy the built plug-ins into ~/Library; do not build them while a host is running)
cd /Users/hiroaki.kimura/projects/ChipSynth-AU/build && cmake --build . --parallel --target YMulatorSynthAU_Tests YMulatorSynthAU_PanTests YMulatorSynthAU_UITests

# Run the binary for the area you touched
./bin/YMulatorSynthAU_PanTests --gtest_brief=1                     # pan motion, wide, echo
./bin/YMulatorSynthAU_UITests --gtest_brief=1                      # editor, envelope display, knobs
./bin/YMulatorSynthAU_Tests --gtest_filter="MacroMapper*:MotionEngine*"

# Everything (a few seconds per binary)
for b in Basic Preset Parameter Pan Integration UI Quality Performance; do ./bin/YMulatorSynthAU_${b}Tests --gtest_brief=1; done

# ctest works too
ctest -R "PitchAccuracyTest" --output-on-failure
```

### **🔥 Key Takeaway:**

**"Tests should verify expected behavior, not accommodate bugs. When tests fail, investigate the implementation first, understand the design specification second, and only modify tests last—after confirming the implementation behavior matches the intended design."**

This principle saved the project from hiding what initially appeared to be implementation bugs but were actually correct design choices aligned with industry standards.