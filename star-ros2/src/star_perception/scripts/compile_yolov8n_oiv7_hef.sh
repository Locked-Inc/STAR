#!/usr/bin/env bash
# Compile yolov8n-oiv7.pt -> .hef targeting Hailo-8L.
#
# **Must run on an x86_64 Linux host with the Hailo Dataflow Compiler +
# hailo_model_zoo installed.** The Pi 5 cannot run the DFC. Expected
# runtime: ~15-30 minutes on a current x86 dev box.
#
# Output: yolov8n-oiv7.hef dropped into this script's directory. Copy
# it to the Pi 5 at:
#   star-ros2/install/star_perception/share/star_perception/models/
# and restart the perception launch. No rebuild needed.
#
# Usage:
#   ./compile_yolov8n_oiv7_hef.sh [--calib-dir /path/to/images]
#
# If --calib-dir is omitted the script will look for a `calib/`
# subdirectory next to the script and fail with a clear message if
# missing. Put ~64-256 representative JPEG/PNG images there. Images
# drawn from the Open Images V7 validation split are ideal; any
# mixed-class indoor/outdoor set works.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
YAML="${PKG_ROOT}/models/yolov8n-oiv7.yaml"
OUT_HEF="${SCRIPT_DIR}/yolov8n-oiv7.hef"

CALIB_DIR=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --calib-dir) CALIB_DIR="$2"; shift 2 ;;
    -h|--help)
      sed -n '2,25p' "$0"; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; exit 2 ;;
  esac
done
[[ -z "${CALIB_DIR}" ]] && CALIB_DIR="${SCRIPT_DIR}/calib"

# -- sanity checks --
if [[ "$(uname -m)" != "x86_64" ]]; then
  echo "ERROR: Hailo Dataflow Compiler only runs on x86_64 Linux." >&2
  echo "       Run this script on your development machine, then copy" >&2
  echo "       the .hef to the Pi 5." >&2
  exit 1
fi
if ! command -v yolo >/dev/null; then
  echo "ERROR: ultralytics 'yolo' CLI not found. Run:" >&2
  echo "       pip install ultralytics" >&2
  exit 1
fi
if ! command -v hailomz >/dev/null; then
  echo "ERROR: hailomz not found. Install the Hailo Model Zoo:" >&2
  echo "       https://github.com/hailo-ai/hailo_model_zoo" >&2
  exit 1
fi
if [[ ! -d "${CALIB_DIR}" ]] || [[ -z "$(ls -A "${CALIB_DIR}" 2>/dev/null)" ]]; then
  echo "ERROR: calibration directory is missing or empty: ${CALIB_DIR}" >&2
  echo "       Put ~64-256 representative JPEG/PNG images there." >&2
  echo "       Open Images V7 validation split is a good source." >&2
  exit 1
fi

# -- step 1: export Ultralytics .pt -> .onnx --
ONNX="${SCRIPT_DIR}/yolov8n-oiv7.onnx"
if [[ ! -f "${ONNX}" ]]; then
  echo "[1/3] Exporting yolov8n-oiv7.pt to ONNX (first use auto-downloads)..."
  # opset=11 matches what the Hailo zoo's yolov8n compile recipe uses.
  # simplify=True folds redundant ops; imgsz=640 is required.
  cd "${SCRIPT_DIR}"
  yolo export model=yolov8n-oiv7.pt format=onnx imgsz=640 opset=11 simplify=True
  # Ultralytics writes yolov8n-oiv7.onnx beside the .pt.
else
  echo "[1/3] ONNX already exported: ${ONNX}"
fi

# -- step 2: compile ONNX -> HEF targeting hailo8l --
echo "[2/3] Compiling HEF targeting hailo8l (~15-30 min on x86)..."
cd "${SCRIPT_DIR}"
hailomz compile \
  --ckpt "${ONNX}" \
  --calib-path "${CALIB_DIR}" \
  --yaml "${YAML}" \
  --classes 601 \
  --hw-arch hailo8l

# hailomz drops the .hef beside the ONNX as yolov8n-oiv7.hef.
PRODUCED="${SCRIPT_DIR}/yolov8n-oiv7.hef"
if [[ ! -f "${PRODUCED}" ]]; then
  echo "ERROR: hailomz did not produce ${PRODUCED}." >&2
  exit 1
fi

# -- step 3: done --
echo "[3/3] Success: ${PRODUCED}"
ls -lh "${PRODUCED}"
echo
echo "Next: copy this .hef to the Pi 5, then on the Pi:"
echo "  cp ~/yolov8n-oiv7.hef \\"
echo "    /workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models/"
echo "  ros2 launch star_perception perception.launch.py \\"
echo "    hef_path:=/workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models/yolov8n-oiv7.hef \\"
echo "    classes_yaml:=/workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models/oiv7_classes.yaml"
