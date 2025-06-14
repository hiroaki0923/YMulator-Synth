# YMulator Synth Build Scripts

This directory contains scripts for building and packaging YMulator Synth.

## 🛠️ Scripts Overview

### `build-dev.sh` - Development Build
Quick development builds with debugging support.

```bash
# Basic development build (Debug mode)
./scripts/build-dev.sh

# Clean build from scratch
./scripts/build-dev.sh --clean

# Build and install to ~/Library/Audio/Plug-Ins/Components/
./scripts/build-dev.sh --install

# Build, install, and run auval validation
./scripts/build-dev.sh --validate

# Combined options
./scripts/build-dev.sh --clean --validate
```

### `build-release.sh` - Release Build
Production builds with packaging for distribution.

```bash
# Basic release build with auto-generated version
./scripts/build-release.sh

# Release build with specific version
./scripts/build-release.sh v1.0.0

# Skip auval validation (faster build)
./scripts/build-release.sh v1.0.0 --skip-validation

# Build and create GitHub release automatically
./scripts/build-release.sh v1.0.0 --github-release

# Combined options
./scripts/build-release.sh v1.0.0 --skip-validation --github-release
```

## 📦 Output Locations

### Development Builds
- **Component**: `build/YMulatorSynthAU_artefacts/Debug/AU/YMulator Synth.component`
- **Installed**: `~/Library/Audio/Plug-Ins/Components/YMulator Synth.component`

### Release Builds
- **DMG Package**: `releases/YMulator-Synth-[version].dmg`
- **Release Notes**: `releases/release-notes-[version].md`
- **Component**: `build/YMulatorSynthAU_artefacts/Release/AU/YMulator Synth.component`

## 🔧 Prerequisites

### Required Tools
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install CMake
brew install cmake

# Install GitHub CLI (for automatic releases)
brew install gh

# Xcode (from App Store)
```

### GitHub CLI Setup (Optional)
```bash
# Authenticate with GitHub (one-time setup)
gh auth login

# Verify authentication
gh auth status

# Test access to your repository
gh repo view hiroaki0923/YMulator-Synth
```

### Verify Installation
```bash
# Check tools
cmake --version
xcodebuild -version
auval -h
```

## 🚀 Quick Start Workflow

### For Development
```bash
# 1. Clone and setup
git clone --recursive https://github.com/hiroaki0923/YMulator-Synth.git
cd YMulator-Synth

# 2. Development build and test
./scripts/build-dev.sh --validate

# 3. Make changes, then quick rebuild
./scripts/build-dev.sh --install
```

### For Release

#### Manual Release
```bash
# 1. Ensure everything is committed
git status

# 2. Build release
./scripts/build-release.sh v1.0.0

# 3. Test the DMG
open releases/YMulator-Synth-v1.0.0.dmg

# 4. Create GitHub release manually
```

#### Automatic Release
```bash
# 1. Setup GitHub CLI (one-time)
gh auth login

# 2. Build and release in one command
./scripts/build-release.sh v1.0.0 --github-release

# 3. Release is automatically created on GitHub
```

## 📋 Build Process Details

### Development Build (`build-dev.sh`)
1. Update Git submodules
2. Configure CMake (Debug mode)
3. Build Audio Unit component
4. Optionally install and validate
5. Display quick usage info

### Release Build (`build-release.sh`)
1. Clean build environment
2. Update Git submodules
3. Configure CMake (Release mode)
4. Build Audio Unit component
5. Run auval validation
6. Create package directory structure
7. Copy component and documentation
8. Generate DMG package
9. Create detailed release notes

## 🧪 Testing and Validation

### Manual Testing
```bash
# Check if AU is registered
auval -a | grep -i chipsynth

# Validate specific component
auval -v aumu ChpS Vend

# Verbose validation output
auval -v aumu ChpS Vend -w
```

### DAW Testing
1. Logic Pro: Audio > Audio Units > Music Effect > YMulator Synth
2. Ableton Live: Instruments > Audio Units > YMulator Synth
3. GarageBand: Smart Controls > Audio Units > Music Effect

## 🐛 Troubleshooting

### Build Fails
```bash
# Clean everything and retry
./scripts/build-dev.sh --clean

# Check CMake configuration
cmake --version
cmake -B build --dry-run
```

### Audio Unit Not Found
```bash
# Check installation
ls -la ~/Library/Audio/Plug-Ins/Components/ | grep ChipSynth

# Force AU cache refresh
killall -9 AudioComponentRegistrar

# Check AU registration
auval -a | grep ChpS
```

### Validation Fails
```bash
# Get detailed validation output
auval -v aumu ChpS Vend -w

# Check Info.plist
plutil -p "~/Library/Audio/Plug-Ins/Components/YMulator Synth.component/Contents/Info.plist"
```

## 📁 Directory Structure

```
scripts/
├── README.md           # This file
├── build-dev.sh        # Development build script
└── build-release.sh    # Release build script

releases/               # Created by build-release.sh
├── YMulator-Synth-*.dmg  # Release packages
└── release-notes-*.md  # Generated release notes

build/                  # CMake build directory
└── YMulatorSynthAU_artefacts/
    ├── Debug/AU/       # Development builds
    └── Release/AU/     # Release builds
```

## 💡 Tips

- Use `--clean` when switching between Debug/Release builds
- Test in multiple DAWs before release
- Keep release notes up to date
- Version numbers should follow semantic versioning (v1.0.0)
- Always validate Audio Units before distribution