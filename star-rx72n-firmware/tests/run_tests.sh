#!/bin/bash
# =============================================================================
# Run Unit Tests for RX72N Protocol Stack
# Host-compiled tests using Unity framework
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

echo "=== RX72N Protocol Stack Unit Tests ==="
echo ""

# Create build directory
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Configure with CMake
echo "Configuring tests..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build tests
echo ""
echo "Building tests..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Run tests
echo ""
echo "Running tests..."
echo "=============================================="
ctest --output-on-failure

echo ""
echo "=============================================="
echo "All tests passed!"
