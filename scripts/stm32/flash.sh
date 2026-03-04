#!/usr/bin/env bash
# Flash STM32 firmware using OpenOCD + STLink.
# Usage: ./scripts/stm32/flash.sh [--elf <path>] [--chip <chip>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32-firmware"

usage() {
    echo "Flash STM32 firmware via STLink using OpenOCD"
    echo ""
    echo "Usage: $0 [--elf <path>] [--chip <chip>]"
    echo ""
    echo "Options:"
    echo "  --elf <path>     Path to ELF file"
    echo "  --chip <chip>    Target chip: STM32F767xx or STM32F746xx (default: STM32F767xx)"
    echo "  -h, --help       Show this help message"
}

CHIP="STM32F767xx"
ELF_PATH=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --elf)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --elf requires a file path" >&2
                exit 1
            fi
            ELF_PATH="$2"
            shift 2
            ;;
        --chip)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --chip requires a chip name" >&2
                exit 1
            fi
            CHIP="$2"
            shift 2
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

# Derive default ELF path from chip if not provided
if [[ -z "${ELF_PATH}" ]]; then
    case "${CHIP}" in
        STM32F767xx) ELF_PATH="${FIRMWARE_DIR}/build-f767/firmware.elf" ;;
        STM32F746xx) ELF_PATH="${FIRMWARE_DIR}/build-f746/firmware.elf" ;;
        *)
            echo "ERROR: Unknown chip: ${CHIP}. Use STM32F767xx or STM32F746xx" >&2
            exit 1
            ;;
    esac
fi

if ! command -v openocd >/dev/null 2>&1; then
    echo "ERROR: openocd is not installed or not in PATH" >&2
    echo "Install OpenOCD and retry." >&2
    exit 1
fi

if [[ ! -f "${ELF_PATH}" ]]; then
    echo "ERROR: ELF file not found: ${ELF_PATH}" >&2
    exit 1
fi

echo "Flashing ${ELF_PATH} ..."
openocd \
    -f interface/stlink.cfg \
    -f target/stm32f7x.cfg \
    -c "program ${ELF_PATH} verify reset exit"

echo "Flash completed successfully"
