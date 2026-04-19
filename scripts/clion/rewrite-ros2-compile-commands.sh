#!/usr/bin/env bash
# Rewrites the colcon-generated compile_commands.json so CLion on the macOS
# host can index the ROS2 workspace that was actually built inside Docker.
#
# colcon writes paths like /workspaces/STAR/star-ros2/... (Docker view).
# CLion on the host sees /Users/<you>/Documents/GitHub/STAR/star-ros2/...
# This script does the path swap and also concatenates per-package
# compile_commands.json files into a single workspace-wide one.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ROS2_DIR="${REPO_ROOT}/star-ros2"
BUILD_DIR="${ROS2_DIR}/build"
OUT_DIR="${ROS2_DIR}/.clion-cdb"
OUT_FILE="${OUT_DIR}/compile_commands.json"

DOCKER_PREFIX="/workspaces/STAR"
HOST_PREFIX="${REPO_ROOT}"

if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "error: ${BUILD_DIR} does not exist. Run a colcon build first." >&2
    echo "  ./build-ros2.sh    (inside the dev container)" >&2
    exit 1
fi

mkdir -p "${OUT_DIR}"

mapfile -t cdb_files < <(find "${BUILD_DIR}" -maxdepth 2 -name compile_commands.json -type f | sort)

if [[ ${#cdb_files[@]} -eq 0 ]]; then
    echo "error: no compile_commands.json files found under ${BUILD_DIR}" >&2
    exit 1
fi

echo "merging ${#cdb_files[@]} compile_commands.json file(s) -> ${OUT_FILE}"

python3 - "${OUT_FILE}" "${DOCKER_PREFIX}" "${HOST_PREFIX}" "${cdb_files[@]}" <<'PY'
import json, sys
out_file, docker_prefix, host_prefix, *inputs = sys.argv[1:]
merged = []
for path in inputs:
    with open(path) as f:
        merged.extend(json.load(f))
def fix(s):
    return s.replace(docker_prefix, host_prefix) if isinstance(s, str) else s
for entry in merged:
    for key in ("directory", "file", "output"):
        if key in entry:
            entry[key] = fix(entry[key])
    if "command" in entry:
        entry["command"] = fix(entry["command"])
    if "arguments" in entry:
        entry["arguments"] = [fix(a) for a in entry["arguments"]]
with open(out_file, "w") as f:
    json.dump(merged, f, indent=2)
print(f"wrote {len(merged)} entries")
PY

echo "done. Open in CLion via: File -> Open -> ${OUT_FILE}"
