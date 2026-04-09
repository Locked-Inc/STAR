#!/usr/bin/env bash
# file:  build.sh
# brief: Build helper for star-beaglebone-blue.
#
# usage: ./scripts/build.sh [preset] [--clean]
#
#   preset:   cross-debug (default) | cross-release | native-debug | native-release
#   --clean:  delete the build directory before configuring
#
# examples:
#   ./scripts/build.sh                      # cross-compile debug
#   ./scripts/build.sh cross-release        # cross-compile release
#   ./scripts/build.sh native-debug         # build natively on BBB
#   ./scripts/build.sh cross-debug --clean  # clean then build

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "${SCRIPT_DIR}")"

PRESET="${1:-cross-debug}"
CLEAN=false
if [[ "${2:-}" == "--clean" ]] || [[ "${1:-}" == "--clean" ]]; then
    CLEAN=true
    if [[ "${1:-}" == "--clean" ]]; then
        PRESET="cross-debug"
    fi
fi

cd "${REPO_DIR}"

# Derive build directory from preset name
BUILD_DIR="build-${PRESET}"

if [[ "${CLEAN}" == true ]] && [[ -d "${BUILD_DIR}" ]]; then
    echo "==> Cleaning ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

echo "==> Configuring preset: ${PRESET}"
cmake --preset "${PRESET}"

echo "==> Building"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo "==> Running tests"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "==> Done: ${BUILD_DIR}/star-beaglebone-blue"
