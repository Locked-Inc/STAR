#!/usr/bin/env bash
# Open the star-beaglebone-blue project in CLion. CMakePresets.json drives
# the build configurations (native-debug for host indexing/dev,
# cross-debug for ARM cross-compile via the toolchain file).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BBB_DIR="${REPO_ROOT}/star-beaglebone-blue"

CLION_BIN="${HOME}/Library/Application Support/JetBrains/Toolbox/scripts/clion"
if [[ ! -x "${CLION_BIN}" ]]; then
    CLION_BIN="$(command -v clion || true)"
fi
if [[ -z "${CLION_BIN}" ]]; then
    echo "error: cannot find clion launcher. Install via JetBrains Toolbox or add to PATH." >&2
    exit 1
fi

echo "launching CLion on ${BBB_DIR}"
exec "${CLION_BIN}" "${BBB_DIR}"
