# YMulator-Synth Testing Framework

## Test Architecture

This directory contains a comprehensive DAW-independent testing framework for YMulator-Synth Audio Unit plugin.

### Key Components

1. **MockAudioProcessorHost** - Simulates DAW environment
2. **AudioOutputVerifier** - Validates audio output characteristics  
3. **MidiSequenceGenerator** - Creates MIDI test sequences
4. **Split Test Architecture** - 6 focused test binaries for improved performance
5. **PluginBasicTest** - Core functionality tests
6. **SimpleParameterTest** - Parameter system validation
7. **ParameterDebugTest** - Diagnostic utilities

### Test Framework Improvements

The testing framework has been significantly enhanced with:

- **Split Test Binaries**: 6 focused executables instead of 1 monolithic binary
- **Parallel Execution**: Tests run simultaneously for ~5x speed improvement
- **CI Optimization**: Avoids 2-minute timeout issues in GitHub Actions
- **Flexible Execution**: Choose between parallel, sequential, or unified modes
- **Backward Compatibility**: Traditional unified binary still available

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

### Split Test Binaries (Recommended)

For improved CI performance and faster development, tests are split into 6 focused binaries:

```bash
# Build all test executables
cd build && cmake --build . --parallel

# Run all tests in parallel (fastest, ~2.5 seconds)
./bin/YMulatorSynthAU_BasicTests --gtest_brief &
./bin/YMulatorSynthAU_PresetTests --gtest_brief &
./bin/YMulatorSynthAU_ParameterTests --gtest_brief &
./bin/YMulatorSynthAU_PanTests --gtest_brief &
./bin/YMulatorSynthAU_IntegrationTests --gtest_brief &
./bin/YMulatorSynthAU_QualityTests --gtest_brief &
wait

# Run specific test categories
./bin/YMulatorSynthAU_BasicTests         # Basic functionality
./bin/YMulatorSynthAU_PresetTests        # Preset management
./bin/YMulatorSynthAU_ParameterTests     # Parameter system
./bin/YMulatorSynthAU_PanTests           # Pan functionality
./bin/YMulatorSynthAU_IntegrationTests   # Component integration
./bin/YMulatorSynthAU_QualityTests       # Audio quality
```

### Test Script Interface

```bash
# Using test script (auto-detects split binaries)
./scripts/test.sh                    # Parallel execution with split binaries
./scripts/test.sh --split            # Explicit split binaries (parallel)
./scripts/test.sh --split-seq        # Sequential split binaries
./scripts/test.sh --unified          # Traditional unified binary

# Legacy interface
ctest --output-on-failure            # Runs all test executables
./bin/YMulatorSynthAU_Tests          # Unified binary (slower)
```

### Performance Comparison

| Method | Execution Time | Notes |
|--------|----------------|-------|
| Split Binaries (Parallel) | ~2.5 seconds | Recommended for CI/development |
| Split Binaries (Sequential) | ~8 seconds | Good for debugging |
| Unified Binary | ~15+ seconds | Traditional, may timeout in CI |

### Debug Mode

```bash
# Debug specific test with verbose output
./bin/YMulatorSynthAU_BasicTests --gtest_filter="PluginBasicTest.*" --gtest_output=xml
./bin/YMulatorSynthAU_ParameterTests --gtest_filter="ParameterDebugTest.*"
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