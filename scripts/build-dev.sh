#!/bin/bash

# YMulator Synth Development Build Script
# Usage: ./scripts/build-dev.sh [--clean] [--install] [--validate]

set -e  # Exit on any error

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"

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
CLEAN=false
INSTALL=false
VALIDATE=false

for arg in "$@"; do
    case $arg in
        --clean)
            CLEAN=true
            ;;
        --install)
            INSTALL=true
            ;;
        --validate)
            VALIDATE=true
            INSTALL=true  # Need to install for validation
            ;;
    esac
done

log_info "Starting YMulator Synth development build..."

# Clean build directory if requested
if [ "$CLEAN" = true ]; then
    log_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"

# Update submodules
log_info "Updating submodules..."
cd "$PROJECT_DIR"
git submodule update --init --recursive

# Configure CMake (only if needed)
if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    log_info "Configuring CMake..."
    cd "$BUILD_DIR"
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
else
    log_info "Using existing CMake configuration"
    cd "$BUILD_DIR"
fi

# Build
log_info "Building Audio Unit (Debug)..."
cmake --build . --config Debug --parallel > /dev/null 2>&1 && log_success "Build completed" || {
    log_error "Build failed"
    exit 1
}

# Check if build succeeded
COMPONENT_PATH="$BUILD_DIR/src/YMulatorSynthAU_artefacts/Debug/AU/YMulator Synth.component"
if [ ! -d "$COMPONENT_PATH" ]; then
    log_error "Build failed - Audio Unit component not found"
    exit 1
fi

# Get component info
BUNDLE_VERSION=$(plutil -extract CFBundleVersion raw "$COMPONENT_PATH/Contents/Info.plist" 2>/dev/null || echo "dev")
BUNDLE_ID=$(plutil -extract CFBundleIdentifier raw "$COMPONENT_PATH/Contents/Info.plist" 2>/dev/null || echo "unknown")

log_info "Component Info:"
echo "   Bundle ID: $BUNDLE_ID"
echo "   Version: $BUNDLE_VERSION"
echo "   Path: $COMPONENT_PATH"

# Install if requested
if [ "$INSTALL" = true ]; then
    log_info "Installing Audio Unit..."
    
    # Install using CMake
    cmake --install . --config Debug
    
    log_success "Audio Unit installed"
    
    # Force Audio Unit cache refresh
    killall -9 AudioComponentRegistrar 2>/dev/null || true
    rm -rf ~/Library/Caches/AudioUnitCache 2>/dev/null || true
    rm -rf ~/Library/Caches/com.apple.audiounits.cache 2>/dev/null || true
    
    log_info "Audio Unit cache cleared"
    
    # Wait for AU registration
    sleep 2
fi

# Validate if requested
if [ "$VALIDATE" = true ]; then
    log_info "Running Audio Unit validation..."
    
    if auval -v aumu ChpS Hrki > /dev/null 2>&1; then
        log_success "Audio Unit validation passed"
    else
        log_warning "Audio Unit validation failed"
        echo "Try running manually: auval -v aumu ChpS Hrki"
    fi
fi

# Show quick usage info
echo ""
log_success "🎉 Development build completed!"
echo ""
echo "🔧 Quick Commands:"
echo "   Clean build:     ./scripts/build-dev.sh --clean"
echo "   Build & install: ./scripts/build-dev.sh --install"
echo "   Build & test:    ./scripts/build-dev.sh --validate"
echo "   Release build:   ./scripts/build-release.sh"
echo ""
echo "🧪 Manual Testing:"
echo "   auval -v aumu ChpS Hrki"
echo "   auval -a | grep -i chipsynth"
echo ""