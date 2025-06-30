# YMulator-Synth Build Scripts

Cross-platform build and test scripts for YMulator-Synth development.

## Main Scripts

### `build.sh` - Unified Build Script
Handles all build operations with consistent cross-platform behavior.

**Key Features:**
- Automatic project root detection
- Cross-platform path handling  
- Color-coded output
- Error handling and validation
- Parallel build support

**Usage:**
```bash
./scripts/build.sh <command> [options]
```

**Commands:**
- `setup` - Initial project setup (git submodules)
- `build` - Build the project
- `rebuild` - Clean and rebuild
- `clean` - Clean build directory
- `debug` - Debug build configuration
- `test` - Run all tests
- `auval` - Audio Unit validation
- `install` - Install plugin

### `test.sh` - Comprehensive Test Script  
Advanced test runner with filtering and categorization.

**Key Features:**
- Test categorization (unit, integration, regression)
- Pattern-based filtering
- Verbose and quiet modes
- Audio Unit validation
- Build integration

**Usage:**
```bash
./scripts/test.sh [options] [test_filter]
```

**Test Categories:**
- `--unit` - Component unit tests
- `--integration` - Cross-component tests
- `--regression` - Bug regression tests
- `--filter PATTERN` - Custom pattern matching

## Legacy Scripts (Deprecated)

- `build-dev.sh` - Replaced by `./scripts/build.sh debug`
- `build-release.sh` - Replaced by `./scripts/build.sh release`

**Migration:**
```bash
# Old way
./scripts/build-dev.sh

# New way  
./scripts/build.sh debug

# Old way
./scripts/build-release.sh

# New way
./scripts/build.sh release
```

## Environment Requirements

**Prerequisites:**
- CMake 3.22+
- Git (for submodules)
- Xcode Command Line Tools (macOS)
- Python 3.8+ (for JUCE build scripts)

**Platform Support:**
- macOS (primary target)
- Linux (build only, no Audio Unit support)
- Windows (experimental, via WSL recommended)

## Examples

```bash
# First time setup
./scripts/build.sh setup
./scripts/build.sh build

# Development workflow
./scripts/build.sh debug --quiet
./scripts/test.sh --unit

# Release workflow
./scripts/build.sh release
./scripts/test.sh --all
./scripts/test.sh --auval

# Specific testing
./scripts/test.sh --filter "ParameterManager"
./scripts/test.sh --regression --verbose

# Troubleshooting
./scripts/build.sh clean
./scripts/build.sh rebuild --verbose
./scripts/test.sh --list
```

## Script Architecture

Both scripts use:
- **Automatic project root detection** - Works from any directory
- **Robust error handling** - Exit on first error with clear messages
- **Color-coded logging** - INFO (green), WARN (yellow), ERROR (red)
- **Platform compatibility** - Handle macOS/Linux differences
- **Dependency validation** - Check required tools before execution

## Contributing

When modifying scripts:
1. Test on multiple environments if possible
2. Maintain backward compatibility in command interfaces
3. Add appropriate error handling and logging
4. Update this documentation
5. Test both success and failure scenarios