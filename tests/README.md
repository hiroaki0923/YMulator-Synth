# YMulator-Synth Testing Framework

## Test Architecture

This directory contains a comprehensive DAW-independent testing framework for YMulator-Synth Audio Unit plugin.

### Key Components

1. **MockAudioProcessorHost** - Simulates DAW environment
2. **AudioOutputVerifier** - Validates audio output characteristics  
3. **MidiSequenceGenerator** - Creates MIDI test sequences
4. **PluginBasicTest** - Core functionality tests
5. **SimpleParameterTest** - Parameter system validation
6. **ParameterDebugTest** - Diagnostic utilities

## Important Design Notes

### ⚠️ **Operator Indexing: 1-based, NOT 0-based**

YMulator-Synth uses **1-based operator indexing** (Op1, Op2, Op3, Op4), which is:

- **Consistent with VOPM software** (OP1-OP4 labeling)
- **Aligned with YM2151 documentation** (Operator 1-4)
- **Standard for FM synthesizers** (user expects "Operator 1")
- **MIDI CC compatible** (Op1_TL, Op2_TL mappings)

**Correct parameter usage:**
```cpp
// ✅ CORRECT - these parameters exist
ParamID::Op::tl(1)  // Operator 1 Total Level
ParamID::Op::tl(2)  // Operator 2 Total Level  
ParamID::Op::tl(3)  // Operator 3 Total Level
ParamID::Op::tl(4)  // Operator 4 Total Level

// ❌ WRONG - these do NOT exist in the system
ParamID::Op::tl(0)  // No "Operator 0" - returns 0/default
```

### Parameter Quantization

Many parameters undergo quantization due to hardware constraints:
- **Algorithm**: 8 discrete values (0-7)
- **Total Level**: 128 discrete values (0-127)  
- **Attack Rate**: 32 discrete values (0-31)

**Test tolerance should account for this:**
```cpp
EXPECT_NEAR(value, expected, 0.05f);  // Allow for quantization
```

## Running Tests

```bash
# Build tests
cd build && cmake --build . --target YMulatorSynthAU_Tests

# Run all tests  
ctest --output-on-failure

# Run specific test suites
./bin/YMulatorSynthAU_Tests --gtest_filter="PluginBasicTest.*"
./bin/YMulatorSynthAU_Tests --gtest_filter="ParameterDebugTest.*"

# Debug mode with verbose output
./bin/YMulatorSynthAU_Tests --gtest_filter="*" --gtest_output=xml
```

## Test Coverage

✅ **Covered Functionality:**
- Plugin initialization and cleanup
- MIDI note on/off processing
- Parameter setting and retrieval  
- MIDI CC parameter automation
- Polyphonic voice management
- Stereo audio output generation
- Parameter bounds validation

🔄 **Areas for Future Testing:**
- Preset save/restore (full state management)
- Performance benchmarks (< 3ms latency)
- Voice stealing algorithms
- LFO and envelope behavior
- S98 recording functionality

## Design Philosophy

Tests should verify **expected behavior**, not accommodate bugs. When tests fail:

1. **First**: Investigate if it's a real bug in implementation
2. **Second**: Check if it's a misunderstanding of design
3. **Last resort**: Adjust test expectations if design is intentional

**Never modify tests to hide implementation bugs.**