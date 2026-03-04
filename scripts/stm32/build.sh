#!/usr/bin/env bash
# Build STM32F767 firmware with CMake + Ninja.
# Usage: ./scripts/stm32/build.sh [clean] [Release|Debug]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32f767-firmware"
BUILD_DIR="${FIRMWARE_DIR}/build"
TOOLCHAIN_FILE="${FIRMWARE_DIR}/cubeide-gcc.cmake"

usage() {
    echo "Build STM32F767 firmware with CMake + Ninja"
    echo ""
    echo "Usage: $0 [clean] [Release|Debug]"
    echo ""
    echo "Arguments:"
    echo "  clean            Remove existing build directory first"
    echo "  Release|Debug    Build type (default: Release)"
    echo ""
    echo "Options:"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0"
    echo "  $0 Debug"
    echo "  $0 clean Release"
}

BUILD_TYPE="Release"
CLEAN=false

for arg in "$@"; do
    case "$arg" in
        clean)
            CLEAN=true
            ;;
        Release|Debug)
            BUILD_TYPE="$arg"
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $arg"
            usage
            exit 1
            ;;
    esac
done

if [[ ! -d "${FIRMWARE_DIR}" ]]; then
    echo "ERROR: Firmware directory not found: ${FIRMWARE_DIR}"
    exit 1
fi

if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
    echo "ERROR: Toolchain file not found: ${TOOLCHAIN_FILE}"
    exit 1
fi

if [[ "${CLEAN}" == "true" ]]; then
    echo "Cleaning build directory: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake is not installed or not in PATH"
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ERROR: ninja is not installed or not in PATH"
    exit 1
fi

START_TIME="$(date +%s)"

echo "Configuring (${BUILD_TYPE})..."
cmake -S "${FIRMWARE_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -G Ninja

echo "Building..."
cmake --build "${BUILD_DIR}" -j"$(nproc)"

END_TIME="$(date +%s)"
ELAPSED="$((END_TIME - START_TIME))"

echo "Build completed successfully in ${ELAPSED}s"
