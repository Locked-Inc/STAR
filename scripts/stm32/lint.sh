#!/usr/bin/env bash
# Run clang-format in check mode for STM32 firmware C/H sources.
# Usage: ./scripts/stm32/lint.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SOURCES_DIR="${REPO_ROOT}/star-stm32f767-firmware/Sources"

usage() {
    echo "Run clang-format check for STM32 firmware sources"
    echo ""
    echo "Usage: $0"
    echo ""
    echo "Options:"
    echo "  -h, --help       Show this help message"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

if [[ $# -gt 0 ]]; then
    echo "ERROR: Unknown argument: $1"
    usage
    exit 1
fi

if ! command -v clang-format >/dev/null 2>&1; then
    echo "ERROR: clang-format is not installed or not in PATH"
    exit 1
fi

if [[ ! -d "${SOURCES_DIR}" ]]; then
    echo "ERROR: Sources directory not found: ${SOURCES_DIR}"
    exit 1
fi

echo "Running clang-format check under ${SOURCES_DIR}"
find "${SOURCES_DIR}" -type f \( -name '*.c' -o -name '*.h' \) -print0 \
    | xargs -0 -r clang-format --dry-run --Werror

echo "Formatting check: PASS"
