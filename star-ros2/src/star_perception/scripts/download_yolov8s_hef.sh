#!/usr/bin/env bash
# Download the YOLOv8s .hef from the Hailo Model Zoo for Hailo-8L.
#
# Run on the Pi 5 (or any host with internet) from the package root.
# Places the file at: <package>/models/yolov8s_h8l.hef
#
# After install, ros2 launch star_perception perception.launch.py finds
# it via get_package_share_directory.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
TARGET="${PKG_ROOT}/models/yolov8s_h8l.hef"

# Pinned model zoo URL. If the Hailo Model Zoo reorganises, update this.
URL="https://hailo-model-zoo.s3.eu-west-2.amazonaws.com/ModelZoo/Compiled/v2.15.0/hailo8l/yolov8s.hef"

mkdir -p "${PKG_ROOT}/models"

if [[ -f "${TARGET}" ]]; then
  echo "Already present: ${TARGET}"
  echo "Delete it to force re-download."
  exit 0
fi

echo "Downloading YOLOv8s.hef (Hailo-8L) from:"
echo "  ${URL}"
echo "to:"
echo "  ${TARGET}"

if command -v curl >/dev/null; then
  curl -fL --retry 3 -o "${TARGET}" "${URL}"
elif command -v wget >/dev/null; then
  wget -O "${TARGET}" "${URL}"
else
  echo "Neither curl nor wget available." >&2
  exit 1
fi

echo "OK. File size:"
ls -lh "${TARGET}"

# Auto-copy into the colcon install share if we can find it, so
# `ros2 launch star_perception ...` picks it up without a rebuild.
for candidate in \
    "${PKG_ROOT}/../../../install/star_perception/share/star_perception/models" \
    "/workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models"
do
  if [[ -d "${candidate}" ]]; then
    cp -f "${TARGET}" "${candidate}/"
    echo "Also copied to: ${candidate}"
    break
  fi
done
