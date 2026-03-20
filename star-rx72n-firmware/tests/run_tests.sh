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

# Validate clang-18 availability and C23 header support
if ! command -v clang-18 &> /dev/null; then
  echo "ERROR: clang-18 not found. Install with: apt install clang-18" >&2
  exit 1
fi

if ! echo '#include <stdckdint.h>' | clang-18 -x c -fsyntax-only - 2>/dev/null; then
  echo "ERROR: clang-18 lacks C23 <stdckdint.h>." >&2
  exit 1
fi

# Configure with CMake
echo "Configuring tests..."
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang-18

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
