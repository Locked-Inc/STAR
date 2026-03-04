#!/usr/bin/env bash
# Start OpenOCD GDB server and attach arm-none-eabi-gdb.
# Usage: ./scripts/stm32/debug.sh [--elf <path>] [--port <port>] [--chip <chip>]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FIRMWARE_DIR="${REPO_ROOT}/star-stm32-firmware"
GDBINIT_FILE="${FIRMWARE_DIR}/.gdbinit"

usage() {
    echo "Start OpenOCD and attach arm-none-eabi-gdb"
    echo ""
    echo "Usage: $0 [--elf <path>] [--port <port>] [--chip <chip>]"
    echo ""
    echo "Options:"
    echo "  --elf <path>     Path to ELF file (default: build-f767/firmware.elf)"
    echo "  --port <port>    GDB port to use (default 3333)"
    echo "  --chip <chip>    Target chip: STM32F767xx or STM32F746xx (default: STM32F767xx)"
    echo "  -h, --help       Show this help message"
}

ELF_PATH=""
PORT=3333
CHIP="STM32F767xx"

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
        --port)
            if [[ $# -lt 2 ]]; then
                echo "ERROR: --port requires a number" >&2
                exit 1
            fi
            PORT="$2"
            if ! [[ "${PORT}" =~ ^[0-9]+$ ]] || (( PORT < 1 || PORT > 65535 )); then
                echo "ERROR: --port must be a number between 1 and 65535" >&2
                exit 1
            fi
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

case "${CHIP}" in
    STM32F767xx)
        WORKAREASIZE="0x40000"   # 256 KB work area
        [[ -z "${ELF_PATH}" ]] && ELF_PATH="${FIRMWARE_DIR}/build-f767/firmware.elf"
        ;;
    STM32F746xx)
        WORKAREASIZE="0x20000"   # 128 KB work area
        [[ -z "${ELF_PATH}" ]] && ELF_PATH="${FIRMWARE_DIR}/build-f746/firmware.elf"
        ;;
    *)
        echo "ERROR: Unknown chip: ${CHIP}. Use STM32F767xx or STM32F746xx" >&2
        exit 1
        ;;
esac

if ! command -v openocd >/dev/null 2>&1; then
    echo "ERROR: openocd is not installed or not in PATH" >&2
    echo "Install OpenOCD and retry." >&2
    exit 1
fi

if ! command -v arm-none-eabi-gdb >/dev/null 2>&1; then
    echo "ERROR: arm-none-eabi-gdb is not installed or not in PATH" >&2
    exit 1
fi

if ! command -v nc >/dev/null 2>&1; then
    echo "ERROR: nc (netcat) is not installed or not in PATH" >&2
    exit 1
fi

if [[ ! -f "${ELF_PATH}" ]]; then
    echo "ERROR: ELF file not found: ${ELF_PATH}" >&2
    exit 1
fi

# make sure requested port is free
if nc -z localhost "${PORT}" >/dev/null 2>&1; then
    echo "ERROR: Port ${PORT} already in use. Is another OpenOCD instance running?" >&2
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
    -c "set WORKAREASIZE ${WORKAREASIZE}" \
    -c "adapter speed 4000" \
    -c "telnet_port disabled" \
    -c "tcl_port disabled" \
    -c "gdb_port ${PORT}" \
    -c "init" \
    -c "reset halt" \
    >"${OPENOCD_LOG}" 2>&1 &
OPENOCD_PID=$!

# wait up to 5 seconds for the gdb port to appear
for i in $(seq 1 10); do
    if nc -z localhost "${PORT}" >/dev/null 2>&1; then
        break
    fi
    sleep 0.5
done

if ! kill -0 "${OPENOCD_PID}" >/dev/null 2>&1; then
    echo "ERROR: OpenOCD failed to start. See ${OPENOCD_LOG}" >&2
    exit 1
fi

if ! nc -z localhost "${PORT}" >/dev/null 2>&1; then
    echo "ERROR: OpenOCD started but port ${PORT} never became ready. See ${OPENOCD_LOG}" >&2
    exit 1
fi

echo "OpenOCD running on localhost:${PORT} (pid ${OPENOCD_PID})"

GDB_ARGS=("${ELF_PATH}" -ex "target extended-remote localhost:${PORT}")
if [[ -f "${GDBINIT_FILE}" ]]; then
    echo "Loading GDB init file: ${GDBINIT_FILE}"
    # append to ensure ELF symbols are available
    GDB_ARGS+=(-x "${GDBINIT_FILE}")
fi

arm-none-eabi-gdb "${GDB_ARGS[@]}"
