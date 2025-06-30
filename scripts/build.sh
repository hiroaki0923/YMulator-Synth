#!/bin/bash

# YMulator-Synth Build Script
# Usage: ./scripts/build.sh [command] [options]

set -e  # Exit on error

# Get project root directory (where this script is located)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_usage() {
    cat << EOF
YMulator-Synth Build Script

USAGE:
    ./scripts/build.sh <command> [options]

COMMANDS:
    setup       Initial project setup (git submodules)
    configure   Configure build system (CMake)
    build       Build the project
    clean       Clean build directory
    rebuild     Clean and rebuild
    test        Run all tests
    auval       Run Audio Unit validation
    install     Install the plugin
    debug       Configure and build debug version
    release     Configure and build release version (default)

OPTIONS:
    -q, --quiet     Suppress build output
    -v, --verbose   Show verbose output
    -j, --jobs N    Use N parallel jobs (default: auto)

EXAMPLES:
    ./scripts/build.sh setup           # Initial setup
    ./scripts/build.sh build           # Quick build
    ./scripts/build.sh rebuild -q      # Clean rebuild with quiet output
    ./scripts/build.sh test             # Run all tests
    ./scripts/build.sh debug           # Debug build
    ./scripts/build.sh auval           # Validate Audio Unit

EOF
}

check_dependencies() {
    if ! command -v cmake &> /dev/null; then
        log_error "CMake not found. Install with: brew install cmake"
        exit 1
    fi
    
    if ! command -v git &> /dev/null; then
        log_error "Git not found. Please install Git."
        exit 1
    fi
}

verify_project_structure() {
    if [[ ! -f "$PROJECT_ROOT/CMakeLists.txt" ]]; then
        log_error "CMakeLists.txt not found in project root: $PROJECT_ROOT"
        log_error "Please run this script from the project root or scripts directory"
        exit 1
    fi
    
    if [[ ! -d "$PROJECT_ROOT/src" ]]; then
        log_error "src/ directory not found in project root"
        exit 1
    fi
}

setup_project() {
    log_info "Setting up project..."
    cd "$PROJECT_ROOT"
    
    if [[ -d ".git" ]]; then
        log_info "Updating git submodules..."
        git submodule update --init --recursive
    else
        log_warn "Not a git repository, skipping submodule update"
    fi
    
    log_info "Setup completed"
}

configure_build() {
    local build_type=${1:-Release}
    local quiet=${2:-false}
    
    log_info "Configuring build (${build_type})..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    if [[ "$quiet" == "true" ]]; then
        cmake .. -DCMAKE_BUILD_TYPE="$build_type" > /dev/null 2>&1
    else
        cmake .. -DCMAKE_BUILD_TYPE="$build_type"
    fi
    
    log_info "Configuration completed"
}

build_project() {
    local quiet=${1:-false}
    local jobs=${2:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}
    
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_warn "Build directory not found, configuring first..."
        configure_build "Release" "$quiet"
    fi
    
    log_info "Building project..."
    cd "$BUILD_DIR"
    
    if [[ "$quiet" == "true" ]]; then
        if cmake --build . --parallel "$jobs" > /dev/null 2>&1; then
            log_info "Build successful"
        else
            log_error "Build failed"
            exit 1
        fi
    else
        cmake --build . --parallel "$jobs"
        log_info "Build completed"
    fi
}

clean_build() {
    log_info "Cleaning build directory..."
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        log_info "Build directory cleaned"
    else
        log_warn "Build directory does not exist"
    fi
}

run_tests() {
    local quiet=${1:-false}
    
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_error "Build directory not found. Run 'build' command first."
        exit 1
    fi
    
    log_info "Running tests..."
    cd "$BUILD_DIR"
    
    if [[ "$quiet" == "true" ]]; then
        if ctest --output-on-failure --quiet; then
            log_info "All tests passed"
        else
            log_error "Some tests failed"
            exit 1
        fi
    else
        ctest --output-on-failure
    fi
}

validate_audio_unit() {
    local quiet=${1:-false}
    
    log_info "Validating Audio Unit..."
    
    if [[ "$quiet" == "true" ]]; then
        if auval -v aumu YMul Hrki > /dev/null 2>&1; then
            log_info "Audio Unit validation PASSED"
        else
            log_error "Audio Unit validation FAILED"
            exit 1
        fi
    else
        auval -v aumu YMul Hrki
    fi
}

install_plugin() {
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_error "Build directory not found. Run 'build' command first."
        exit 1
    fi
    
    log_info "Installing plugin..."
    cd "$BUILD_DIR"
    cmake --install .
    log_info "Installation completed"
}

# Parse command line arguments
QUIET=false
VERBOSE=false
JOBS=""
COMMAND=""

while [[ $# -gt 0 ]]; do
    case $1 in
        setup|configure|build|clean|rebuild|test|auval|install|debug|release)
            COMMAND=$1
            shift
            ;;
        -q|--quiet)
            QUIET=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Show usage if no command provided
if [[ -z "$COMMAND" ]]; then
    show_usage
    exit 1
fi

# Check dependencies and project structure
check_dependencies
verify_project_structure

# Execute command
case $COMMAND in
    setup)
        setup_project
        ;;
    configure)
        configure_build "Release" "$QUIET"
        ;;
    build)
        build_project "$QUIET" "$JOBS"
        ;;
    clean)
        clean_build
        ;;
    rebuild)
        clean_build
        configure_build "Release" "$QUIET"
        build_project "$QUIET" "$JOBS"
        ;;
    test)
        run_tests "$QUIET"
        ;;
    auval)
        validate_audio_unit "$QUIET"
        ;;
    install)
        install_plugin
        ;;
    debug)
        configure_build "Debug" "$QUIET"
        build_project "$QUIET" "$JOBS"
        ;;
    release)
        configure_build "Release" "$QUIET"
        build_project "$QUIET" "$JOBS"
        ;;
    *)
        log_error "Unknown command: $COMMAND"
        show_usage
        exit 1
        ;;
esac