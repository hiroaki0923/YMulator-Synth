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

- Release changelog lives in `CHANGELOG.md` (English) and `CHANGELOG_ja.md` (Japanese), not in the READMEs
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
# Build verification (MUST run from correct path)
cd /Users/hiroaki.kimura/projects/ChipSynth-AU/build && cmake --build . --parallel > /dev/null 2>&1 && echo "Build successful" || echo "Build failed"

# Audio Unit validation (can run from any directory)
auval -v aumu YMul Hrki > /dev/null 2>&1 && echo "auval PASSED" || echo "auval FAILED"
```

**⚠️ PATH CRITICAL:** Always ensure you're in `/Users/hiroaki.kimura/projects/ChipSynth-AU/build/` for build commands. If you get "No rule to make target 'Makefile'" error, you're in the wrong directory.

**🔒 These rules are derived from proven improvements that enhanced code quality, reduced bugs, and improved maintainability. Deviation requires explicit justification and documentation.**

## 🏗️ Refactoring Guidelines and Architecture Principles

### **⚠️ CRITICAL LESSON: Always Maintain Test Coverage During Refactoring**

Based on the successful Phase 1 refactoring experience (PanProcessor extraction), the following principles MUST be followed:

#### **The Right Approach for Safe Refactoring:**

**✅ CORRECT Process:**
1. **Run full test suite** → Establish baseline (all tests must pass)
2. **Extract small components** → One responsibility at a time (e.g., PanProcessor)
3. **Test immediately** → After each extraction, verify no regressions
4. **Fix issues incrementally** → Address test failures before proceeding
5. **Commit frequently** → Small, atomic commits with clear descriptions
6. **Document architectural changes** → Update design documents

**❌ WRONG Process:**
1. **Extract multiple components** → Risk of complex, intertwined failures
2. **Skip intermediate testing** → Difficult to isolate issues
3. **Large, monolithic commits** → Hard to review and debug
4. **Ignore test instability** → "It'll work eventually" mentality

#### **Component Extraction Best Practices:**

```cpp
// ✅ GOOD - Clear single responsibility
class PanProcessor {
public:
    PanProcessor(YmfmWrapperInterface& ymfm);  // Dependency injection
    void applyGlobalPan(int channel, float panValue);
    void setChannelRandomPan(int channel);
private:
    YmfmWrapperInterface& ymfmWrapper;  // Interface, not concrete
    uint8_t channelRandomPanBits[8];    // Component-specific state
};
```

```cpp
// ❌ BAD - Mixed responsibilities
class AudioManager {
public:
    void handleMIDI(const MidiMessage& msg);     // MIDI responsibility
    void applyPan(int channel, float pan);       // Pan responsibility  
    void loadPreset(const Preset& preset);       // Preset responsibility
    void processAudio(AudioBuffer& buffer);      // Audio responsibility
    // TOO MANY RESPONSIBILITIES!
};
```

#### **Dependency Injection Patterns:**

**✅ CORRECT - Interface-based injection:**
```cpp
// In header
class ParameterManager {
public:
    ParameterManager(YmfmWrapperInterface& ymfm, 
                    std::shared_ptr<PanProcessor> panProcessor);
private:
    std::shared_ptr<PanProcessor> panProcessor;  // Shared ownership
};

// In implementation
ParameterManager::ParameterManager(YmfmWrapperInterface& ymfm,
                                 std::shared_ptr<PanProcessor> panProc)
    : ymfmWrapper(ymfm), panProcessor(panProc) {}
```

**❌ WRONG - Concrete dependencies:**
```cpp
class ParameterManager {
public:
    ParameterManager() {
        panProcessor = new PanProcessor();  // Hard-coded dependency
        ymfmWrapper = new YmfmWrapper();    // Not testable
    }
};
```

#### **Test Stability Guidelines:**

**🎯 Random/Non-Deterministic Tests:**
When dealing with randomized functionality (like RANDOM pan mode):

```cpp
// ✅ GOOD - Ensure variation while maintaining determinism
void PanProcessor::setChannelRandomPan(int channel) {
    uint8_t currentValue = channelRandomPanBits[channel];
    uint8_t newValue;
    
    // Force different value 80% of the time to ensure variation
    do {
        newValue = generateRandomPanValue();
    } while (newValue == currentValue && shouldForceChange());
    
    channelRandomPanBits[channel] = newValue;
}
```

**❌ BAD - Pure randomness without variation guarantee:**
```cpp
void setChannelRandomPan(int channel) {
    // May generate same value repeatedly, causing test flakiness
    channelRandomPanBits[channel] = Random::getSystemRandom().nextInt(3);
}
```

#### **Refactoring Metrics and Success Criteria:**

**Measure refactoring success:**
- **Line count reduction**: Target 10-20% reduction in main classes
- **Test coverage**: Must maintain 100% pass rate
- **Component count**: Each component should have <1000 lines
- **Dependency depth**: Max 3 levels of injection
- **Build time**: Should not increase significantly

**Example from Phase 1 success:**
- ✅ **PluginProcessor.cpp**: 804 → 675 lines (16% reduction)
- ✅ **Test coverage**: 235/235 tests passing (100%)
- ✅ **New components**: PanProcessor (122 lines, focused responsibility)
- ✅ **Build time**: Unchanged (~30 seconds)

#### **Critical Testing Strategy:**

**Split Test Binaries for CI Optimization:**
```bash
# ✅ GOOD - Parallel execution, faster CI
./bin/YMulatorSynthAU_BasicTests --gtest_brief &
./bin/YMulatorSynthAU_PanTests --gtest_brief &
./bin/YMulatorSynthAU_ParameterTests --gtest_brief &
wait  # Total time: ~2.5 seconds

# ❌ BAD - Monolithic, slow CI
./bin/YMulatorSynthAU_Tests  # Total time: 4+ seconds, timeout risk
```

**Always test refactoring with:**
1. **Unit tests** - Individual component functionality
2. **Integration tests** - Component interaction
3. **Regression tests** - Ensure no behavioral changes
4. **Performance tests** - Verify no significant slowdown

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

**DAW-Independent Testing:** `tests/` directory contains:
- **MockAudioProcessorHost**: Simulates DAW environment
- **AudioOutputVerifier**: Validates audio characteristics
- **MidiSequenceGenerator**: Creates test sequences
- **Comprehensive parameter testing**: Without requiring Logic Pro/Ableton

**Running Tests:**
```bash
# Build tests
cd /Users/hiroaki.kimura/projects/ChipSynth-AU/build && cmake --build . --target YMulatorSynthAU_Tests

# ⚠️ IMPORTANT: Full test suite (480 tests) takes 2+ minutes - use targeted testing
# Full test suite (avoid in regular development)
ctest --output-on-failure                               # All tests with output on failure
ctest --output-on-failure --quiet                       # All tests, minimal output

# ===== TARGETED TESTING (RECOMMENDED) =====
# Run specific test by name
ctest -R "PluginBasicTest.PolyphonyTest" --output-on-failure

# Run test categories (much faster than full suite)
ctest -R "PluginBasicTest" --output-on-failure              # Basic plugin tests (~10 tests)
ctest -R "ParameterManagerTest" --output-on-failure         # Parameter tests (~15 tests)
ctest -R "PresetManagerTest" --output-on-failure            # Preset tests (~35 tests)
ctest -R "StateManagerTest" --output-on-failure             # State tests (~25 tests)
ctest -R "VoiceManagerTest" --output-on-failure             # Voice tests (~20 tests)
ctest -R "YmfmWrapperTest" --output-on-failure              # DSP tests (~30 tests)
ctest -R "MainComponentTest" --output-on-failure            # UI tests (~15 tests)
ctest -R "GlobalPanTest" --output-on-failure                # Pan tests (~15 tests)
ctest -R "AudioQualityTest" --output-on-failure             # Audio quality tests (~5 tests)

# Run tests by number range (useful for batching)
ctest --output-on-failure -I 1,50     # Tests 1-50 only
ctest --output-on-failure -I 51,100   # Tests 51-100 only
ctest --output-on-failure -I 101,150  # Tests 101-150 only

# List all available tests (480 total)
ctest -N | grep -E "Test.*#.*|Total Tests:"

# Quick sanity checks (under 30 seconds)
ctest -R "BasicTest.SanityCheck" --output-on-failure
ctest -R "PluginBasicTest.InitializationTest" --output-on-failure

# Debug specific failing tests with verbose output
ctest -R "PluginBasicTest.PolyphonyTest" --output-on-failure --verbose

# Alternative: Direct binary execution for specific test suites
./bin/YMulatorSynthAU_Tests --gtest_filter="ParameterDebugTest.*"
./bin/YMulatorSynthAU_Tests --gtest_filter="PluginBasicTest.PolyphonyTest"
```

### **🔥 Key Takeaway:**

**"Tests should verify expected behavior, not accommodate bugs. When tests fail, investigate the implementation first, understand the design specification second, and only modify tests last—after confirming the implementation behavior matches the intended design."**

This principle saved the project from hiding what initially appeared to be implementation bugs but were actually correct design choices aligned with industry standards.