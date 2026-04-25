#!/usr/bin/env bash
#
# train_door_state_yolov8n.sh
#
# One-shot training script for the STAR door-state classifier. Produces
# `door_state_yolov8n.onnx` ready to drop into
# `star-compliance/star_compliance/models/`.
#
# Requires:
#   - A machine with Python 3.10+ and an NVIDIA GPU with CUDA 11.8+
#     (training takes ~20 min on an RTX 3060, ~2 hr on CPU).
#   - ~8 GB free disk for ultralytics + dataset + checkpoints.
#   - Internet access to download the base weights + dataset.
#
# Usage:
#     cd STAR
#     bash scripts/door_state_training/train_door_state_yolov8n.sh
#
# Environment-variable overrides (all optional):
#     EPOCHS=50              number of training epochs
#     IMG_SIZE=320           input resolution (larger = more accurate, slower)
#     BATCH_SIZE=64          mini-batch size
#     DEVICE=0               CUDA device index, or "cpu" to force CPU
#     DATASET_ROOT=.../data  where to stage the DoorDetect-Class-Dataset
#     OUTPUT_ONNX=...        final ONNX filename
#
# On success, the script prints a diff-ready summary and the path to
# the ONNX weights. Copy that file to the compliance-engine models/
# directory and commit.

set -euo pipefail

STAR_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TRAIN_DIR="$STAR_DIR/scripts/door_state_training"
WORK_DIR="${WORK_DIR:-$TRAIN_DIR/.work}"
DATASET_ROOT="${DATASET_ROOT:-$WORK_DIR/dataset}"
RUNS_DIR="${RUNS_DIR:-$WORK_DIR/runs}"
OUTPUT_ONNX="${OUTPUT_ONNX:-$TRAIN_DIR/door_state_yolov8n.onnx}"

EPOCHS="${EPOCHS:-50}"
IMG_SIZE="${IMG_SIZE:-320}"
BATCH_SIZE="${BATCH_SIZE:-64}"
DEVICE="${DEVICE:-0}"

GREEN='\033[0;32m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; NC='\033[0m'
say()  { echo -e "${GREEN}[train]${NC} $*"; }
warn() { echo -e "${YELLOW}[train]${NC} $*"; }
die()  { echo -e "${RED}[train]${NC} $*" >&2; exit 1; }

mkdir -p "$WORK_DIR" "$DATASET_ROOT" "$RUNS_DIR"

# -- 1. Python + ultralytics ------------------------------------------------
if ! command -v python3 >/dev/null 2>&1; then
    die "python3 not on PATH. Install Python 3.10+ first."
fi

say "Creating virtual env at $WORK_DIR/venv ..."
if [[ ! -d "$WORK_DIR/venv" ]]; then
    python3 -m venv "$WORK_DIR/venv"
fi
# shellcheck disable=SC1091
source "$WORK_DIR/venv/bin/activate"

say "Installing training deps (ultralytics, onnx, onnxruntime)..."
pip install --upgrade pip >/dev/null
pip install \
    "ultralytics>=8.3.0" \
    "onnx>=1.16" \
    "onnxruntime>=1.17" \
    "onnxsim>=0.4.35" \
    "opencv-python>=4.9" \
    >/dev/null

# -- 2. Dataset download ---------------------------------------------------
#
# The DoorDetect-Class-Dataset is distributed via Google Drive from
# https://github.com/gasparramoa/DoorDetect-Class-Dataset
#
# Cropped subset structure (what we use):
#   Cropped/
#     train/{closed,open,semi-open}/*.jpg
#     val/{closed,open,semi-open}/*.jpg
#     test/{closed,open,semi-open}/*.jpg
#
# gdown pulls a Google Drive folder. Swap FILE_ID when the repo's
# download link changes - check the README on github.com/gasparramoa.
#
# This step is skipped if DATASET_ROOT already has train/ val/ test/.
if [[ ! -d "$DATASET_ROOT/train" ]]; then
    say "Downloading DoorDetect-Class-Dataset (~300 MB) via gdown..."
    pip install "gdown>=5.0" >/dev/null
    GDRIVE_URL="${GDRIVE_URL:-https://drive.google.com/drive/folders/1nI9rtgPbh25qh14vKXQvBpI4S1Szzukk}"
    cd "$WORK_DIR"
    gdown --folder "$GDRIVE_URL" -O dataset-raw \
        || die "Dataset download failed. Check GDRIVE_URL or download manually per scripts/door_state_training/README.md"

    # Expected layout after gdown; the repo arranges as
    # dataset-raw/DoorDetect-Class/Cropped/{Train,Val,Test}/{classN}
    # Normalize into $DATASET_ROOT/{train,val,test}/{open,closed,ajar}.
    say "Normalizing dataset layout..."
    CROPPED_ROOT="$(find dataset-raw -type d -name 'Cropped' -print -quit)"
    [[ -n "$CROPPED_ROOT" ]] || die "Cropped/ folder not found in downloaded tree."

    for split_src in Train Val Test; do
        split_dst="$(echo "$split_src" | tr '[:upper:]' '[:lower:]')"
        for cls_src in closed open semi-open; do
            cls_dst="$cls_src"
            [[ "$cls_src" == "semi-open" ]] && cls_dst="ajar"
            src="$CROPPED_ROOT/$split_src/$cls_src"
            dst="$DATASET_ROOT/$split_dst/$cls_dst"
            mkdir -p "$dst"
            if [[ -d "$src" ]]; then
                cp -r "$src"/* "$dst"/ 2>/dev/null || true
            fi
        done
    done
    cd "$STAR_DIR"
fi

for split in train val test; do
    for cls in open closed ajar; do
        count=$(find "$DATASET_ROOT/$split/$cls" -type f \
            \( -name "*.jpg" -o -name "*.png" -o -name "*.jpeg" \) 2>/dev/null | wc -l)
        printf "  %-5s / %-7s  %4d images\n" "$split" "$cls" "$count"
    done
done

# -- 3. Train yolov8n-cls ---------------------------------------------------
say "Starting YOLOv8n-cls training (epochs=$EPOCHS, imgsz=$IMG_SIZE, device=$DEVICE)..."

cd "$WORK_DIR"
yolo classify train \
    model=yolov8n-cls.pt \
    data="$DATASET_ROOT" \
    epochs="$EPOCHS" \
    imgsz="$IMG_SIZE" \
    batch="$BATCH_SIZE" \
    device="$DEVICE" \
    patience=15 \
    project="$RUNS_DIR" \
    name="door_state_yolov8n" \
    exist_ok=true \
    save_period=10

# The best checkpoint lives at runs/door_state_yolov8n/weights/best.pt
BEST_PT="$RUNS_DIR/door_state_yolov8n/weights/best.pt"
[[ -f "$BEST_PT" ]] || die "Training did not produce a best.pt at $BEST_PT"

# -- 4. Validate on the held-out test split --------------------------------
say "Running held-out test-split evaluation..."
yolo classify val \
    model="$BEST_PT" \
    data="$DATASET_ROOT" \
    imgsz="$IMG_SIZE" \
    split=test \
    device="$DEVICE" \
    project="$RUNS_DIR" \
    name="door_state_eval" \
    exist_ok=true \
    || warn "Evaluation step returned non-zero; inspect logs."

# -- 5. Export to ONNX -----------------------------------------------------
say "Exporting $BEST_PT to ONNX..."
yolo export \
    model="$BEST_PT" \
    format=onnx \
    imgsz="$IMG_SIZE" \
    opset=12 \
    simplify=true

# ultralytics writes best.onnx alongside best.pt
BEST_ONNX="$RUNS_DIR/door_state_yolov8n/weights/best.onnx"
[[ -f "$BEST_ONNX" ]] || die "ONNX export failed; expected $BEST_ONNX"

cp "$BEST_ONNX" "$OUTPUT_ONNX"
MD5=$(md5sum "$OUTPUT_ONNX" 2>/dev/null | awk '{print $1}' \
      || md5 -q "$OUTPUT_ONNX")
BYTES=$(wc -c < "$OUTPUT_ONNX")

say "========================================"
say "Training complete."
say ""
say "ONNX:    $OUTPUT_ONNX"
say "Size:    $BYTES bytes"
say "md5:     $MD5"
say ""
say "Next steps:"
say "  1. Copy the ONNX to the compliance-engine models dir:"
say "     cp \"$OUTPUT_ONNX\" \\"
say "        \"$STAR_DIR/star-compliance/star_compliance/models/door_state_yolov8n.onnx\""
say ""
say "  2. Run the compliance-engine smoke tests to confirm the node"
say "     picks up the weights:"
say "     cd $STAR_DIR/star-compliance"
say "     PYTHONPATH=. pytest tests/"
say ""
say "  3. Update star_compliance/models/README.md with the md5 hash,"
say "     dataset commit, and training-log location."
say ""
say "  4. Commit + push:"
say "     cd $STAR_DIR"
say "     git add star-compliance/star_compliance/models/door_state_yolov8n.onnx"
say "     git add star-compliance/star_compliance/models/README.md"
say "     git commit -m 'Add trained door_state_yolov8n.onnx weights'"
say "     git push origin main"
say "========================================"
