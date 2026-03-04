#!/usr/bin/env bash
# Run clang-format in check mode for STM32 firmware C/H sources.
# Usage: ./scripts/stm32/lint.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SOURCES_DIR="${REPO_ROOT}/star-stm32f767-firmware/Sources"
CLANG_FORMAT_CFG="${REPO_ROOT}/.clang-format"

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

# Warn if clang-format version is not 18.x (STM32CubeIDE target)
CF_VERSION=$(clang-format --version | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
CF_MAJOR=$(echo "${CF_VERSION}" | cut -d. -f1)
if [[ "${CF_MAJOR}" != "18" ]]; then
    echo "WARNING: clang-format version ${CF_VERSION} detected; STM32CubeIDE targets v18.x — results may differ"
fi

# Require a .clang-format config at the repo root
if [[ ! -f "${CLANG_FORMAT_CFG}" ]]; then
    echo "ERROR: .clang-format config not found at repo root: ${CLANG_FORMAT_CFG}"
    echo "       Generate one with: clang-format --style=LLVM --dump-config > .clang-format"
    exit 1
fi

if [[ ! -d "${SOURCES_DIR}" ]]; then
    echo "ERROR: Sources directory not found: ${SOURCES_DIR}"
    exit 1
fi

echo "clang-format version: ${CF_VERSION}"
echo "Config:               ${CLANG_FORMAT_CFG}"
echo "Running check under:  ${SOURCES_DIR}"
echo ""

# Collect files first; handle the empty-set case portably (no xargs -r)
mapfile -d '' FILES < <(
    find "${SOURCES_DIR}" -type f \( -name '*.c' -o -name '*.h' \) -print0
)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "WARNING: No .c/.h files found under ${SOURCES_DIR}"
    exit 0
fi

printf '%s\0' "${FILES[@]}" \
    | xargs -0 clang-format --dry-run --Werror --style=file

echo "Formatting check: PASS"
