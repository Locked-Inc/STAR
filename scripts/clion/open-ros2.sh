#!/usr/bin/env bash
# Refresh the host-friendly compile_commands.json and open the ROS2
# workspace in CLion as a Compilation Database project.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
CDB_FILE="${REPO_ROOT}/star-ros2/.clion-cdb/compile_commands.json"

"${SCRIPT_DIR}/rewrite-ros2-compile-commands.sh"

CLION_BIN="${HOME}/Library/Application Support/JetBrains/Toolbox/scripts/clion"
if [[ ! -x "${CLION_BIN}" ]]; then
    CLION_BIN="$(command -v clion || true)"
fi
if [[ -z "${CLION_BIN}" ]]; then
    echo "error: cannot find clion launcher. Install via JetBrains Toolbox or add to PATH." >&2
    exit 1
fi

echo "launching CLion with ${CDB_FILE}"
exec "${CLION_BIN}" "${CDB_FILE}"
