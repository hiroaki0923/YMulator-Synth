#!/bin/bash

# YMulator Synth Release Build Script
# Usage: ./scripts/build-release.sh [version] [--skip-validation]

set -e  # Exit on any error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
RELEASE_DIR="$PROJECT_DIR/releases"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Functions
log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

# Parse arguments
VERSION=${1:-"$(date +%Y%m%d)"}
SKIP_VALIDATION=""
CREATE_GITHUB_RELEASE=false

for arg in "$@"; do
    case $arg in
        --skip-validation)
            SKIP_VALIDATION="--skip-validation"
            ;;
        --github-release)
            CREATE_GITHUB_RELEASE=true
            ;;
        --help)
            echo "Usage: $0 [version] [options]"
            echo ""
            echo "Options:"
            echo "  --skip-validation   Skip auval validation"
            echo "  --github-release    Create GitHub release automatically"
            echo "  --help              Show this help"
            echo ""
            echo "Examples:"
            echo "  $0                           # Build with auto version"
            echo "  $0 v1.0.0                   # Build with specific version"
            echo "  $0 v1.0.0 --github-release  # Build and create GitHub release"
            exit 0
            ;;
    esac
done

log_info "Starting YMulator Synth release build..."
log_info "Version: $VERSION"
log_info "Project Directory: $PROJECT_DIR"

# Check prerequisites
log_info "Checking prerequisites..."

if ! command -v cmake &> /dev/null; then
    log_error "CMake not found. Install with: brew install cmake"
    exit 1
fi

if ! command -v xcodebuild &> /dev/null; then
    log_error "Xcode not found. Install Xcode from App Store"
    exit 1
fi

log_success "Prerequisites check passed"

# Clean and create build directory
log_info "Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
mkdir -p "$RELEASE_DIR"

# Update submodules
log_info "Updating submodules..."
cd "$PROJECT_DIR"
git submodule update --init --recursive

# Configure CMake
log_info "Configuring CMake..."
cd "$BUILD_DIR"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

# Build
log_info "Building Audio Unit..."
cmake --build . --config Release --parallel

# Check if build succeeded
COMPONENT_PATH="$BUILD_DIR/src/YMulatorSynthAU_artefacts/Release/AU/YMulator Synth.component"
if [ ! -d "$COMPONENT_PATH" ]; then
    log_error "Build failed - Audio Unit component not found"
    exit 1
fi

log_success "Build completed successfully"

# Install for validation (if not skipping)
if [ "$SKIP_VALIDATION" != "--skip-validation" ]; then
    log_info "Installing Audio Unit for validation..."
    cmake --install . --config Release
    
    # Wait for AU registration
    sleep 3
    
    # Run auval
    log_info "Running Audio Unit validation..."
    if auval -v aumu ChpS Hrki > /dev/null 2>&1; then
        log_success "Audio Unit validation passed"
    else
        log_warning "Audio Unit validation failed - continuing anyway"
    fi
else
    log_info "Skipping Audio Unit validation"
fi

# Get component info
BUNDLE_VERSION=$(plutil -extract CFBundleVersion raw "$COMPONENT_PATH/Contents/Info.plist" 2>/dev/null || echo "$VERSION")
BUNDLE_ID=$(plutil -extract CFBundleIdentifier raw "$COMPONENT_PATH/Contents/Info.plist" 2>/dev/null || echo "unknown")

log_info "Component Info:"
echo "   Bundle ID: $BUNDLE_ID"
echo "   Version: $BUNDLE_VERSION"

# Create release package
PACKAGE_DIR="$BUILD_DIR/YMulator-Synth-$VERSION"
DMG_NAME="YMulator-Synth-$VERSION.dmg"

log_info "Creating release package..."

# Create package directory structure
mkdir -p "$PACKAGE_DIR/Components"
mkdir -p "$PACKAGE_DIR/Documentation"

# Copy component
cp -R "$COMPONENT_PATH" "$PACKAGE_DIR/Components/"


# Create installation instructions
cat > "$PACKAGE_DIR/README.txt" << 'EOF'
YMulator Synth - YM2151 FM Synthesis Audio Unit

Installation Instructions:
1. Copy "YMulator Synth.component" from the Components folder to:
   ~/Library/Audio/Plug-Ins/Components/
   (or /Library/Audio/Plug-Ins/Components/ for all users)

2. Restart your DAW

3. Look for "YMulator Synth" in the Music Device category

Requirements:
- macOS 10.13 or later
- Audio Unit compatible DAW (Logic Pro, Ableton Live, GarageBand, etc.)

Features:
- 8-voice polyphonic FM synthesis
- VOPM-style interface with all YM2151 parameters
- 8 factory presets showcasing advanced sound design
- Full MIDI CC support (VOPMex compatible + DT2 extensions)
- Real-time parameter control with < 3ms latency

For more information, documentation, and source code:
https://github.com/hiroaki0923/YMulator-Synth

License: GPL v3
EOF

# Copy license and documentation
cp "$PROJECT_DIR/LICENSE" "$PACKAGE_DIR/"
cp "$PROJECT_DIR/README.md" "$PACKAGE_DIR/Documentation/"

# Copy relevant documentation
if [ -d "$PROJECT_DIR/docs" ]; then
    cp -R "$PROJECT_DIR/docs" "$PACKAGE_DIR/Documentation/"
fi

# Create DMG
log_info "Creating DMG package..."
cd "$BUILD_DIR"

if command -v hdiutil &> /dev/null; then
    hdiutil create -volname "YMulator Synth $VERSION" \
                   -srcfolder "$PACKAGE_DIR" \
                   -ov \
                   -format UDZO \
                   "$DMG_NAME"
    
    # Move to releases directory
    mv "$DMG_NAME" "$RELEASE_DIR/"
    
    log_success "DMG created: $RELEASE_DIR/$DMG_NAME"
else
    log_warning "hdiutil not found - creating ZIP instead"
    cd "$BUILD_DIR"
    zip -r "YMulator-Synth-$VERSION.zip" "$(basename "$PACKAGE_DIR")"
    mv "YMulator-Synth-$VERSION.zip" "$RELEASE_DIR/"
    log_success "ZIP created: $RELEASE_DIR/YMulator-Synth-$VERSION.zip"
fi

# Generate release notes
RELEASE_NOTES="$RELEASE_DIR/release-notes-$VERSION.md"
cat > "$RELEASE_NOTES" << EOF
# YMulator Synth $VERSION

A modern FM synthesis Audio Unit plugin bringing authentic YM2151 (OPM) sound to your DAW.

## 🎹 Features
- **8-voice polyphonic FM synthesis** using high-precision YM2151 emulation
- **VOPM-style interface** with all operator parameters
- **44 real-time parameters** including DT1, DT2, Key Scale, Feedback
- **8 factory presets** showcasing advanced sound design
- **Full MIDI CC support** (VOPMex compatible + DT2 extensions)
- **Audio Unit v2/v3 compatible** (Music Effect category)

## 📦 Installation
1. Download and open the DMG file
2. Copy "YMulator Synth.component" to ~/Library/Audio/Plug-Ins/Components/
3. Restart your DAW
4. Find "YMulator Synth" in the Music Effect category

## 🎚️ MIDI CC Mapping
- CC 14: Algorithm (0-7)
- CC 15: Feedback (0-7)  
- CC 16-19: OP1-4 Total Level (0-127)
- CC 20-23: OP1-4 Multiple (0-15)
- CC 24-27: OP1-4 Detune1 (0-7)
- CC 28-31: OP1-4 Detune2 (0-3)
- CC 39-42: OP1-4 Key Scale (0-3)
- CC 43-46: OP1-4 Attack Rate (0-31)
- CC 47-50: OP1-4 Decay1 Rate (0-31)
- CC 51-54: OP1-4 Decay2 Rate (0-31)
- CC 55-58: OP1-4 Release Rate (0-15)
- CC 59-62: OP1-4 Sustain Level (0-15)

## 🎵 Factory Presets
| # | Name | Description |
|---|------|-------------|
| 0 | Electric Piano | DT2 chorusing, rich harmonics |
| 1 | Synth Bass | Aggressive Key Scale, punch |
| 2 | Brass Section | Ensemble spread, DT2 variations |
| 3 | String Pad | Warm feedback, subtle DT2 |
| 4 | Lead Synth | Sharp attack, complex detuning |
| 5 | Organ | Harmonic series, organic character |
| 6 | Bells | Inharmonic relationships |
| 7 | Init | Basic sine wave template |

## 🔧 System Requirements
- macOS 10.13 or later
- Audio Unit compatible DAW (Logic Pro, Ableton Live, GarageBand, etc.)
- 64-bit Intel or Apple Silicon processor

## 📋 Technical Specifications
- Component Version: $BUNDLE_VERSION
- Audio Unit Type: Music Device (aumu)
- Manufacturer: Hrki
- Subtype: ChpS
- Built with: JUCE + ymfm library
- License: GPL v3

## 📝 Build Information
- Build Date: $(date)
- Git Commit: $(cd "$PROJECT_DIR" && git rev-parse --short HEAD 2>/dev/null || echo "unknown")
- Built On: $(uname -a)

For more information, visit: https://github.com/hiroaki0923/YMulator-Synth
EOF

log_success "Release notes created: $RELEASE_NOTES"

# Create GitHub Release if requested
if [ "$CREATE_GITHUB_RELEASE" = true ]; then
    log_info "Creating GitHub release..."
    
    # Check if gh command is available
    if ! command -v gh &> /dev/null; then
        log_error "GitHub CLI (gh) not found. Install with: brew install gh"
        log_info "Continuing without GitHub release creation..."
    else
        # Check if authenticated
        if ! gh auth status &> /dev/null; then
            log_error "Not authenticated with GitHub. Run: gh auth login"
            log_info "Continuing without GitHub release creation..."
        else
            cd "$PROJECT_DIR"
            
            # Determine if this is a prerelease (contains alpha, beta, rc, dev)
            PRERELEASE_FLAG=""
            if [[ "$VERSION" =~ (alpha|beta|rc|dev) ]]; then
                PRERELEASE_FLAG="--prerelease"
                log_info "Detected prerelease version"
            fi
            
            # Create the release
            RELEASE_TITLE="YMulator Synth $VERSION"
            DMG_PATH="$RELEASE_DIR/$DMG_NAME"
            ZIP_PATH="$RELEASE_DIR/YMulator-Synth-$VERSION.zip"
            
            # Determine which file to upload
            UPLOAD_FILE=""
            if [ -f "$DMG_PATH" ]; then
                UPLOAD_FILE="$DMG_PATH"
            elif [ -f "$ZIP_PATH" ]; then
                UPLOAD_FILE="$ZIP_PATH"
            else
                log_error "No package file found for upload"
                exit 1
            fi
            
            # Create release with gh command
            if gh release create "$VERSION" \
                --title "$RELEASE_TITLE" \
                --notes-file "$RELEASE_NOTES" \
                $PRERELEASE_FLAG \
                "$UPLOAD_FILE"; then
                
                log_success "GitHub release created successfully!"
                echo "🔗 Release URL: $(gh release view "$VERSION" --web --json url -q .url 2>/dev/null || echo "Check GitHub releases page")"
            else
                log_error "Failed to create GitHub release"
                log_info "You can create it manually using the generated files"
            fi
        fi
    fi
else
    log_info "Skipping GitHub release creation (use --github-release to enable)"
fi

# Summary
echo ""
log_success "🎉 Release build completed successfully!"
echo ""
echo "📦 Package Location:"
echo "   $RELEASE_DIR/"
echo ""
echo "📋 Files Created:"
ls -la "$RELEASE_DIR" | grep "$VERSION"
echo ""
if [ "$CREATE_GITHUB_RELEASE" = true ]; then
    echo "🚀 GitHub Release:"
    echo "   Created automatically (if gh CLI is configured)"
    echo ""
else
    echo "🚀 Manual GitHub Release Steps:"
    echo "   1. Test the DMG package"
    echo "   2. Go to GitHub → Releases → New Release"
    echo "   3. Tag: $VERSION"
    echo "   4. Upload: $(ls "$RELEASE_DIR"/*"$VERSION"* 2>/dev/null | head -1)"
    echo "   5. Copy release notes from: $RELEASE_NOTES"
    echo ""
    echo "💡 Or use automatic release:"
    echo "   $0 $VERSION --github-release"
    echo ""
fi