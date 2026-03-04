#!/usr/bin/env bash
# Report and enforce STM32F767 firmware size budgets.
# Usage: ./scripts/stm32/size.sh [--elf <path>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32f767-firmware"
DEFAULT_ELF="${FIRMWARE_DIR}/build/star-stm32f767-firmware.elf"

FLASH_LIMIT=2097152
SRAM_LIMIT=524288

usage() {
    echo "Report and enforce STM32F767 firmware size budgets"
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

if ! command -v arm-none-eabi-size >/dev/null 2>&1; then
    echo "ERROR: arm-none-eabi-size is not installed or not in PATH"
    exit 1
fi

if [[ ! -f "${ELF_PATH}" ]]; then
    echo "ERROR: ELF file not found: ${ELF_PATH}"
    exit 1
fi

arm-none-eabi-size --format=berkeley "${ELF_PATH}"

TEXT=$(arm-none-eabi-size --format=berkeley "${ELF_PATH}" | awk 'NR==2{print $1}')
DATA=$(arm-none-eabi-size --format=berkeley "${ELF_PATH}" | awk 'NR==2{print $2}')
BSS=$(arm-none-eabi-size --format=berkeley "${ELF_PATH}" | awk 'NR==2{print $3}')

FLASH_USED=$((TEXT + DATA))
SRAM_USED=$((DATA + BSS))

echo "Flash: ${FLASH_USED} / ${FLASH_LIMIT} bytes ($((FLASH_USED * 100 / FLASH_LIMIT))%)"
echo "SRAM:  ${SRAM_USED}  / ${SRAM_LIMIT}  bytes ($((SRAM_USED  * 100 / SRAM_LIMIT))%)"

if [[ "${FLASH_USED}" -gt "${FLASH_LIMIT}" ]]; then
    echo "ERROR: Flash budget exceeded (2 MB limit)"
    exit 1
fi

if [[ "${SRAM_USED}" -gt "${SRAM_LIMIT}" ]]; then
    echo "ERROR: SRAM budget exceeded (512 KB limit)"
    exit 1
fi

echo "Size budgets: PASS"
