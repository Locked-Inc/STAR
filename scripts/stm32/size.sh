#!/usr/bin/env bash
# Report and enforce STM32 firmware size budgets.
# Usage: ./scripts/stm32/size.sh [--elf <path>] [--chip <chip>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32-firmware"

usage() {
    echo "Report and enforce STM32 firmware size budgets"
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

# Set chip-specific memory limits and derive default ELF path
case "${CHIP}" in
    STM32F767xx)
        FLASH_LIMIT=2097152   # 2 MB Flash
        SRAM_LIMIT=524288     # 512 KB SRAM
        [[ -z "${ELF_PATH}" ]] && ELF_PATH="${FIRMWARE_DIR}/build-f767/firmware.elf"
        ;;
    STM32F746xx)
        FLASH_LIMIT=1048576   # 1 MB Flash
        SRAM_LIMIT=327680     # 320 KB SRAM
        [[ -z "${ELF_PATH}" ]] && ELF_PATH="${FIRMWARE_DIR}/build-f746/firmware.elf"
        ;;
    *)
        echo "ERROR: Unknown chip: ${CHIP}. Use STM32F767xx or STM32F746xx" >&2
        exit 1
        ;;
esac

if ! command -v arm-none-eabi-size >/dev/null 2>&1; then
    echo "ERROR: arm-none-eabi-size is not installed or not in PATH" >&2
    exit 1
fi

if [[ ! -f "${ELF_PATH}" ]]; then
    echo "ERROR: ELF file not found: ${ELF_PATH}" >&2
    exit 1
fi

arm-none-eabi-size --format=berkeley "${ELF_PATH}"

TEXT=$(arm-none-eabi-size --format=berkeley "${ELF_PATH}" | awk 'NR==2{print $1}')
DATA=$(arm-none-eabi-size --format=berkeley "${ELF_PATH}" | awk 'NR==2{print $2}')
BSS=$(arm-none-eabi-size --format=berkeley "${ELF_PATH}" | awk 'NR==2{print $3}')

FLASH_USED=$((TEXT + DATA))
SRAM_USED=$((DATA + BSS))

echo "Chip:  ${CHIP}"
echo "Flash: ${FLASH_USED} / ${FLASH_LIMIT} bytes ($((FLASH_USED * 100 / FLASH_LIMIT))%)"
echo "SRAM:  ${SRAM_USED}  / ${SRAM_LIMIT}  bytes ($((SRAM_USED  * 100 / SRAM_LIMIT))%)"

if [[ "${FLASH_USED}" -gt "${FLASH_LIMIT}" ]]; then
    echo "ERROR: Flash budget exceeded"
    exit 1
fi

if [[ "${SRAM_USED}" -gt "${SRAM_LIMIT}" ]]; then
    echo "ERROR: SRAM budget exceeded"
    exit 1
fi

echo "Size budgets: PASS"
