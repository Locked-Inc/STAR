#!/usr/bin/env bash
# Start OpenOCD GDB server and attach arm-none-eabi-gdb.
# Usage: ./scripts/stm32/debug.sh [--elf <path>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32f767-firmware"
DEFAULT_ELF="${FIRMWARE_DIR}/build/star-stm32f767-firmware.elf"
GDBINIT_FILE="${FIRMWARE_DIR}/.gdbinit"

usage() {
    echo "Start OpenOCD and attach arm-none-eabi-gdb"
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

if ! command -v arm-none-eabi-gdb >/dev/null 2>&1; then
    echo "ERROR: arm-none-eabi-gdb is not installed or not in PATH"
    exit 1
fi

if [[ ! -f "${ELF_PATH}" ]]; then
    echo "ERROR: ELF file not found: ${ELF_PATH}"
    exit 1
fi

OPENOCD_LOG="${FIRMWARE_DIR}/openocd.log"

cleanup() {
    if [[ -n "${OPENOCD_PID:-}" ]] && kill -0 "${OPENOCD_PID}" >/dev/null 2>&1; then
        kill "${OPENOCD_PID}" >/dev/null 2>&1 || true
        wait "${OPENOCD_PID}" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT

openocd \
    -f interface/stlink.cfg \
    -f target/stm32f7x.cfg \
    -c "gdb_port 3333" \
    >"${OPENOCD_LOG}" 2>&1 &
OPENOCD_PID=$!

sleep 1
if ! kill -0 "${OPENOCD_PID}" >/dev/null 2>&1; then
    echo "ERROR: OpenOCD failed to start. See ${OPENOCD_LOG}"
    exit 1
fi

echo "OpenOCD running on localhost:3333 (pid ${OPENOCD_PID})"

GDB_ARGS=("${ELF_PATH}" -ex "target extended-remote localhost:3333")
if [[ -f "${GDBINIT_FILE}" ]]; then
    echo "Loading GDB init file: ${GDBINIT_FILE}"
    GDB_ARGS=(-x "${GDBINIT_FILE}" "${GDB_ARGS[@]}")
fi

arm-none-eabi-gdb "${GDB_ARGS[@]}"
