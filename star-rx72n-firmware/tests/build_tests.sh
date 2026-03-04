#!/bin/bash
# =============================================================================
# Build Unit Tests for RX72N Firmware
# Host-compiled tests using Unity framework and CMake
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=== Building RX72N Firmware Unit Tests ==="
echo ""

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
echo "Configuring tests with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build tests
echo ""
echo "Building all test executables..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=============================================="
echo "Build complete! Test binaries in ${BUILD_DIR}/"
echo "Run tests with: ./run_tests.sh"
echo "=============================================="
