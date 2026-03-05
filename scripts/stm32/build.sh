#!/usr/bin/env bash
# Build STM32 firmware with CMake + Ninja.
# Usage: ./scripts/stm32/build.sh [--chip <chip>] [clean] [Release|Debug]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32-firmware"
TOOLCHAIN_FILE="${FIRMWARE_DIR}/cubeide-gcc.cmake"

usage() {
    echo "Build STM32 firmware with CMake + Ninja"
    echo ""
    echo "Usage: $0 [--chip <chip>] [clean] [Release|Debug]"
    echo ""
    echo "Options:"
    echo "  --chip <chip>    Target chip: STM32F767xx or STM32F746xx (default: STM32F767xx)"
    echo "  -h, --help       Show this help message"
    echo ""
    echo "Arguments:"
    echo "  clean            Remove existing build directory first"
    echo "  Release|Debug    Build type (default: Release)"
    echo ""
    echo "Examples:"
    echo "  $0"
    echo "  $0 --chip STM32F746xx"
    echo "  $0 --chip STM32F767xx Debug"
    echo "  $0 --chip STM32F746xx clean Release"
}

BUILD_TYPE="Release"
CLEAN=false
CHIP="STM32F767xx"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --chip)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --chip requires a chip name" >&2
                exit 1
            fi
            CHIP="$2"
            shift 2
            ;;
        clean)
            CLEAN=true
            shift
            ;;
        Release|Debug)
            BUILD_TYPE="$1"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

case "${CHIP}" in
    STM32F767xx) BUILD_DIR="${FIRMWARE_DIR}/build-f767" ;;
    STM32F746xx) BUILD_DIR="${FIRMWARE_DIR}/build-f746" ;;
    *)
        echo "ERROR: Unknown chip: ${CHIP}. Use STM32F767xx or STM32F746xx" >&2
        exit 1
        ;;
esac

if [[ ! -d "${FIRMWARE_DIR}" ]]; then
    echo "ERROR: Firmware directory not found: ${FIRMWARE_DIR}" >&2
    exit 1
fi

if [[ ! -f "${TOOLCHAIN_FILE}" ]]; then
    echo "ERROR: Toolchain file not found: ${TOOLCHAIN_FILE}" >&2
    exit 1
fi

if [[ "${CLEAN}" == "true" ]]; then
    echo "Cleaning build directory: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake is not installed or not in PATH" >&2
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ERROR: ninja is not installed or not in PATH" >&2
    exit 1
fi

START_TIME="$(date +%s)"

echo "Configuring (${CHIP}, ${BUILD_TYPE})..."
cmake -S "${FIRMWARE_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
    -DSTM32_TARGET="${CHIP}" \
    -G Ninja

echo "Building..."
cmake --build "${BUILD_DIR}" -j"$(nproc)"

END_TIME="$(date +%s)"
ELAPSED="$((END_TIME - START_TIME))"

echo "Build completed successfully in ${ELAPSED}s"
echo "ELF: ${BUILD_DIR}/firmware.elf"
