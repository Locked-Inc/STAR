#!/usr/bin/env bash
# Flash STM32F767 firmware using OpenOCD + STLink.
# Usage: ./scripts/stm32/flash.sh [--elf <path>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32f767-firmware"
DEFAULT_ELF="${FIRMWARE_DIR}/build/star-stm32f767-firmware.elf"

usage() {
    echo "Flash STM32F767 firmware via STLink using OpenOCD"
    echo ""
    echo "Usage: $0 [--elf <path>]"
    echo ""
    echo "Options:"
    echo "  --elf <path>     Path to ELF file"
    echo "  -h, --help       Show this help message"
}

ELF_PATH="${DEFAULT_ELF}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --elf)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --elf requires a file path"
                exit 1
            fi
            ELF_PATH="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
done

if ! command -v openocd >/dev/null 2>&1; then
    echo "ERROR: openocd is not installed or not in PATH"
    echo "Install OpenOCD and retry."
    exit 1
fi

if [[ ! -f "${ELF_PATH}" ]]; then
    echo "ERROR: ELF file not found: ${ELF_PATH}"
    exit 1
fi

echo "Flashing ${ELF_PATH} ..."
openocd \
    -f interface/stlink.cfg \
    -f target/stm32f7x.cfg \
    -c "program ${ELF_PATH} verify reset exit"

echo "Flash completed successfully"
