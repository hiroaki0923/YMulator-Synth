#!/bin/bash

# YMulator-Synth Test Script
# Usage: ./scripts/test.sh [options] [test_filter]

set -e  # Exit on error

# Get project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1"
}

show_usage() {
    cat << EOF
YMulator-Synth Test Script

USAGE:
    ./scripts/test.sh [options] [test_filter]

OPTIONS:
    -a, --all           Run all tests (default)
    -u, --unit          Run unit tests only
    -i, --integration   Run integration tests only
    -r, --regression    Run regression tests only
    -f, --filter REGEX  Run tests matching regex pattern
    -l, --list          List available tests
    -v, --verbose       Show verbose test output
    -q, --quiet         Show minimal output
    --gtest-only        Show only Google Test output (suppress JUCE debug)
    --auval             Run Audio Unit validation
    --build             Build before running tests
    --gtest-args ARGS   Pass additional arguments to gtest
    --split             Use split test binaries (parallel execution, default)
    --split-seq         Use split test binaries (sequential execution)
    --unified           Use unified test binary (YMulatorSynthAU_Tests)

TEST CATEGORIES:
    Unit Tests:          Basic component testing
    Integration Tests:   Cross-component testing  
    Regression Tests:    Bug regression testing
    Performance Tests:   Audio quality and timing

EXAMPLES:
    ./scripts/test.sh                           # Run all tests with split binaries (parallel)
    ./scripts/test.sh -u                        # Unit tests only
    ./scripts/test.sh -f "ParameterManager"     # Tests matching "ParameterManager"
    ./scripts/test.sh --gtest-only             # Only Google Test output (no JUCE debug)
    ./scripts/test.sh -l                        # List all available tests
    ./scripts/test.sh --auval                   # Audio Unit validation only
    ./scripts/test.sh --build -v               # Build then run tests verbosely
    ./scripts/test.sh --split                   # Use split binaries (parallel, fast)
    ./scripts/test.sh --split-seq               # Use split binaries (sequential)
    ./scripts/test.sh --unified                 # Use unified binary (slower, traditional)

EOF
}

check_build_exists() {
    if [[ ! -d "$BUILD_DIR" ]]; then
        log_error "Build directory not found: $BUILD_DIR"
        log_info "Run './scripts/build.sh build' first, or use --build option"
        exit 1
    fi
    
    # Check if split test binaries exist (preferred)
    SPLIT_BINARIES_EXIST=false
    if [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_BasicTests" ]] && \
       [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_PresetTests" ]] && \
       [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_ParameterTests" ]] && \
       [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_PanTests" ]] && \
       [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_IntegrationTests" ]] && \
       [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_QualityTests" ]]; then
        SPLIT_BINARIES_EXIST=true
    fi
    
    # Check if unified test binary exists
    UNIFIED_BINARY_EXISTS=false
    if [[ -f "$BUILD_DIR/bin/YMulatorSynthAU_Tests" ]]; then
        UNIFIED_BINARY_EXISTS=true
    fi
    
    if [[ "$SPLIT_BINARIES_EXIST" == "false" ]] && [[ "$UNIFIED_BINARY_EXISTS" == "false" ]]; then
        log_error "No test executables found. Build may be incomplete."
        log_info "Run './scripts/build.sh rebuild' to rebuild the project"
        exit 1
    fi
    
    # Set default test mode based on available binaries
    if [[ "$TEST_BINARY_MODE" == "auto" ]]; then
        if [[ "$SPLIT_BINARIES_EXIST" == "true" ]]; then
            TEST_BINARY_MODE="split"
            log_debug "Auto-detected split test binaries - using parallel execution"
        else
            TEST_BINARY_MODE="unified"
            log_debug "Auto-detected unified test binary - using traditional execution"
        fi
    fi
}

build_if_needed() {
    log_info "Building project before running tests..."
    cd "$PROJECT_ROOT"
    ./scripts/build.sh build -q
}

list_tests() {
    check_build_exists
    
    log_info "Available test suites:"
    cd "$BUILD_DIR"
    
    echo ""
    echo "=== Unit Tests ==="
    ./bin/YMulatorSynthAU_Tests --gtest_list_tests | grep -E "^[A-Z].*Test\." | grep -E "(Test\.|ParameterManager|StateManager|VoiceManager|YmfmWrapper|PresetManager)" | sort
    
    echo ""
    echo "=== Integration Tests ==="
    ./bin/YMulatorSynthAU_Tests --gtest_list_tests | grep -E "^[A-Z].*Test\." | grep -E "(Integration|Comprehensive)" | sort
    
    echo ""
    echo "=== Regression Tests ==="
    ./bin/YMulatorSynthAU_Tests --gtest_list_tests | grep -E "^[A-Z].*Test\." | grep -E "(Regression|Pan.*Test|RandomPan)" | sort
    
    echo ""
    echo "=== Performance/Quality Tests ==="
    ./bin/YMulatorSynthAU_Tests --gtest_list_tests | grep -E "^[A-Z].*Test\." | grep -E "(Quality|Performance|Audio)" | sort
    
    echo ""
    log_info "Use -f 'pattern' to run specific test patterns"
}

run_unit_tests() {
    local quiet=$1
    local gtest_args=$2
    
    log_info "Running unit tests..."
    cd "$BUILD_DIR"
    
    local filter="*Test.*"
    # Exclude integration and regression tests
    local exclude="--gtest_filter=${filter}:-*Integration*:*Comprehensive*:*Regression*:*Quality*:*Performance*:*RandomPan*:*AudioQuality*"
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    if [[ "$GTEST_ONLY" == "true" ]]; then
        # Redirect both stdout and stderr to capture only Google Test output
        env $env_vars ./bin/YMulatorSynthAU_Tests $exclude $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:"
    elif [[ "$quiet" == "true" ]]; then
        env $env_vars ./bin/YMulatorSynthAU_Tests $exclude $gtest_args --gtest_brief=1 2>/dev/null
    else
        env $env_vars ./bin/YMulatorSynthAU_Tests $exclude $gtest_args
    fi
}

run_integration_tests() {
    local quiet=$1
    local gtest_args=$2
    
    log_info "Running integration tests..."
    cd "$BUILD_DIR"
    
    local filter="--gtest_filter=*Integration*:*Comprehensive*"
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    if [[ "$GTEST_ONLY" == "true" ]]; then
        # Redirect both stdout and stderr to capture only Google Test output
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:"
    elif [[ "$quiet" == "true" ]]; then
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>/dev/null
    else
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args
    fi
}

run_regression_tests() {
    local quiet=$1
    local gtest_args=$2
    
    log_info "Running regression tests..."
    cd "$BUILD_DIR"
    
    local filter="--gtest_filter=*Regression*:*RandomPan*"
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    if [[ "$GTEST_ONLY" == "true" ]]; then
        # Redirect both stdout and stderr to capture only Google Test output
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:"
    elif [[ "$quiet" == "true" ]]; then
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>/dev/null
    else
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args
    fi
}

run_quality_tests() {
    local quiet=$1
    local gtest_args=$2
    
    log_info "Running audio quality tests..."
    cd "$BUILD_DIR"
    
    local filter="--gtest_filter=*Quality*:*Performance*:*Audio*"
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    if [[ "$GTEST_ONLY" == "true" ]]; then
        # Redirect both stdout and stderr to capture only Google Test output
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:"
    elif [[ "$quiet" == "true" ]]; then
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>/dev/null
    else
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args
    fi
}

run_filtered_tests() {
    local pattern=$1
    local quiet=$2
    local gtest_args=$3
    
    log_info "Running tests matching pattern: $pattern"
    cd "$BUILD_DIR"
    
    local filter="--gtest_filter=*${pattern}*"
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    if [[ "$GTEST_ONLY" == "true" ]]; then
        # Redirect both stdout and stderr to capture only Google Test output
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:"
    elif [[ "$quiet" == "true" ]]; then
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args --gtest_brief=1 2>/dev/null
    else
        env $env_vars ./bin/YMulatorSynthAU_Tests $filter $gtest_args
    fi
}

run_split_tests_parallel() {
    local quiet=$1
    local gtest_args=$2
    
    log_info "Running all tests using split binaries (parallel execution)..."
    cd "$BUILD_DIR"
    
    # Define split test binaries
    local binaries=(
        "YMulatorSynthAU_BasicTests"
        "YMulatorSynthAU_PresetTests"
        "YMulatorSynthAU_ParameterTests"
        "YMulatorSynthAU_PanTests"
        "YMulatorSynthAU_IntegrationTests"
        "YMulatorSynthAU_QualityTests"
    )
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    # Start all test binaries in parallel
    local pids=()
    for binary in "${binaries[@]}"; do
        if [[ "$GTEST_ONLY" == "true" ]]; then
            env $env_vars ./bin/$binary $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:" &
        elif [[ "$quiet" == "true" ]]; then
            env $env_vars ./bin/$binary $gtest_args --gtest_brief=1 2>/dev/null &
        else
            env $env_vars ./bin/$binary $gtest_args &
        fi
        pids+=("$!")
    done
    
    # Wait for all tests to complete
    local failed=false
    for pid in "${pids[@]}"; do
        if ! wait "$pid"; then
            failed=true
        fi
    done
    
    if [[ "$failed" == "true" ]]; then
        log_error "Some tests failed"
        exit 1
    else
        log_info "All split tests passed (parallel execution)"
    fi
}

run_split_tests_sequential() {
    local quiet=$1
    local gtest_args=$2
    
    log_info "Running all tests using split binaries (sequential execution)..."
    cd "$BUILD_DIR"
    
    # Define split test binaries
    local binaries=(
        "YMulatorSynthAU_BasicTests"
        "YMulatorSynthAU_PresetTests"
        "YMulatorSynthAU_ParameterTests"
        "YMulatorSynthAU_PanTests"
        "YMulatorSynthAU_IntegrationTests"
        "YMulatorSynthAU_QualityTests"
    )
    
    # Set environment for GTEST_ONLY mode
    local env_vars=""
    if [[ "$GTEST_ONLY" == "true" ]]; then
        env_vars="JUCE_DISABLE_LOGGING=1"
    fi
    
    # Run each test binary sequentially
    for binary in "${binaries[@]}"; do
        log_info "Running $binary..."
        if [[ "$GTEST_ONLY" == "true" ]]; then
            env $env_vars ./bin/$binary $gtest_args --gtest_brief=1 2>&1 | grep -E "^\[|\[==========\]|\[  PASSED  \]|\[  FAILED  \]|^Running|^Note:"
        elif [[ "$quiet" == "true" ]]; then
            env $env_vars ./bin/$binary $gtest_args --gtest_brief=1 2>/dev/null
        else
            env $env_vars ./bin/$binary $gtest_args
        fi
        
        if [[ $? -ne 0 ]]; then
            log_error "Test binary $binary failed"
            exit 1
        fi
    done
    
    log_info "All split tests passed (sequential execution)"
}

run_all_tests() {
    local quiet=$1
    local gtest_args=$2
    
    if [[ "$TEST_BINARY_MODE" == "split" ]]; then
        run_split_tests_parallel "$quiet" "$gtest_args"
    elif [[ "$TEST_BINARY_MODE" == "split-seq" ]]; then
        run_split_tests_sequential "$quiet" "$gtest_args"
    else
        # Unified binary mode
        log_info "Running all tests using unified binary..."
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
    fi
}

validate_audio_unit() {
    local quiet=$1
    
    log_info "Running Audio Unit validation..."
    
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

# Parse command line arguments
QUIET=false
VERBOSE=false
BUILD_FIRST=false
LIST_TESTS=false
GTEST_ONLY=false
TEST_MODE="all"
TEST_BINARY_MODE="auto"  # auto, split, split-seq, unified
FILTER_PATTERN=""
GTEST_ARGS=""

while [[ $# -gt 0 ]]; do
    case $1 in
        -a|--all)
            TEST_MODE="all"
            shift
            ;;
        -u|--unit)
            TEST_MODE="unit"
            shift
            ;;
        -i|--integration)
            TEST_MODE="integration"
            shift
            ;;
        -r|--regression)
            TEST_MODE="regression"
            shift
            ;;
        --quality)
            TEST_MODE="quality"
            shift
            ;;
        -f|--filter)
            TEST_MODE="filter"
            FILTER_PATTERN="$2"
            shift 2
            ;;
        -l|--list)
            LIST_TESTS=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -q|--quiet)
            QUIET=true
            shift
            ;;
        --gtest-only)
            GTEST_ONLY=true
            QUIET=true  # Automatically enable quiet mode
            shift
            ;;
        --auval)
            TEST_MODE="auval"
            shift
            ;;
        --build)
            BUILD_FIRST=true
            shift
            ;;
        --gtest-args)
            GTEST_ARGS="$2"
            shift 2
            ;;
        --split)
            TEST_BINARY_MODE="split"
            shift
            ;;
        --split-seq)
            TEST_BINARY_MODE="split-seq"
            shift
            ;;
        --unified)
            TEST_BINARY_MODE="unified"
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            # Treat unknown arguments as test filter patterns
            if [[ -z "$FILTER_PATTERN" ]]; then
                TEST_MODE="filter"
                FILTER_PATTERN="$1"
            else
                log_error "Unknown option: $1"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Build if requested
if [[ "$BUILD_FIRST" == "true" ]]; then
    build_if_needed
fi

# List tests if requested
if [[ "$LIST_TESTS" == "true" ]]; then
    list_tests
    exit 0
fi

# Check build exists
check_build_exists

# Execute test mode
case $TEST_MODE in
    all)
        run_all_tests "$QUIET" "$GTEST_ARGS"
        ;;
    unit)
        run_unit_tests "$QUIET" "$GTEST_ARGS"
        ;;
    integration)
        run_integration_tests "$QUIET" "$GTEST_ARGS"
        ;;
    regression)
        run_regression_tests "$QUIET" "$GTEST_ARGS"
        ;;
    quality)
        run_quality_tests "$QUIET" "$GTEST_ARGS"
        ;;
    filter)
        if [[ -z "$FILTER_PATTERN" ]]; then
            log_error "Filter pattern required with -f option"
            exit 1
        fi
        run_filtered_tests "$FILTER_PATTERN" "$QUIET" "$GTEST_ARGS"
        ;;
    auval)
        validate_audio_unit "$QUIET"
        ;;
    *)
        log_error "Unknown test mode: $TEST_MODE"
        show_usage
        exit 1
        ;;
esac