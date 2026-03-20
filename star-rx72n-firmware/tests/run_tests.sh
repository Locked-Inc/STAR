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

# Validate C compiler with C23 header support.
# gcc-14+ and clang-18+ both provide <stdckdint.h> and <stdbit.h>.
# Default to gcc-14; fall back to clang-18 if gcc-14 is unavailable.
if command -v gcc-14 &> /dev/null; then
  TEST_CC=gcc-14
elif command -v clang-18 &> /dev/null; then
  TEST_CC=clang-18
else
  echo "ERROR: No C23-capable compiler found. Install gcc-14 or clang-18." >&2
  exit 1
fi

if ! echo '#include <stdckdint.h>' | "$TEST_CC" -std=c2x -x c -fsyntax-only - 2>/dev/null; then
  echo "ERROR: $TEST_CC lacks C23 <stdckdint.h>." >&2
  exit 1
fi

# Configure with CMake
echo "Configuring tests (compiler: $TEST_CC)..."
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER="$TEST_CC"

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
