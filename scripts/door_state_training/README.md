# Training the STAR Door-State Classifier

Fine-tunes **YOLOv8n-cls** on the **DoorDetect-Class-Dataset** to
classify a rectified camera frame as `open`, `closed`, or `ajar`.
The output is the ONNX weights file that the compliance engine
(`door_state_classifier.py`) loads at runtime.

**Target machine:** any Linux or macOS box with Python 3.10+ and,
ideally, an NVIDIA GPU. An RTX 3060 finishes the default recipe in
~20 min. CPU-only training works but takes 1.5-2 hours.

---

## Quickstart (the happy path)

```bash
cd STAR
bash scripts/door_state_training/train_door_state_yolov8n.sh
```

That's it. The script creates a virtualenv, installs Ultralytics
and ONNX tooling, downloads the dataset from the author's Google
Drive, normalizes it into the YOLOv8 classification layout, trains
for 50 epochs at 320x320, validates on the held-out test split,
and exports `door_state_yolov8n.onnx` into this directory.

When it finishes, follow the 4-step "Next steps" the script prints
at the end. Typically:

```bash
cp scripts/door_state_training/door_state_yolov8n.onnx \
   final_capstone/compliance-engine/star_compliance/models/door_state_yolov8n.onnx

cd final_capstone/compliance-engine && PYTHONPATH=. pytest tests/
# expect: 62+ passed, 17+ skipped

cd ../..
git add final_capstone/compliance-engine/star_compliance/models/door_state_yolov8n.onnx
git add final_capstone/compliance-engine/star_compliance/models/README.md
git commit -m "Add trained door_state_yolov8n.onnx weights"
git push origin main
```

---

## Manual walkthrough (when the script fails)

### 1. Environment

```bash
# Verify Python and CUDA
python3 --version          # want >= 3.10
nvidia-smi                 # want a CUDA GPU; if absent, set DEVICE=cpu below
df -h $PWD                 # want >= 8 GB free

# Create the venv manually
python3 -m venv scripts/door_state_training/.work/venv
source scripts/door_state_training/.work/venv/bin/activate
pip install --upgrade pip
pip install ultralytics onnx onnxruntime onnxsim opencv-python gdown
```

### 2. Download the dataset

The DoorDetect-Class-Dataset is hosted on Google Drive. The public
download link rotates occasionally - if the script's default fails,
grab the current one from
https://github.com/gasparramoa/DoorDetect-Class-Dataset#download.

```bash
cd scripts/door_state_training/.work
gdown --folder "https://drive.google.com/drive/folders/1nI9rtgPbh25qh14vKXQvBpI4S1Szzukk" \
      -O dataset-raw
```

Expected download: ~300 MB compressed, ~1 GB extracted.

### 3. Normalize to YOLOv8 classification layout

YOLOv8 classification training expects:

```
dataset/
  train/
    open/*.jpg
    closed/*.jpg
    ajar/*.jpg
  val/
    open/
    closed/
    ajar/
  test/
    open/
    closed/
    ajar/
```

The dataset ships as `Cropped/Train/`, `Cropped/Val/`, `Cropped/Test/`
with subfolders named `open`, `closed`, and `semi-open`. Rename
`semi-open -> ajar` so it matches what the compliance engine expects.

```bash
CROPPED="$(find dataset-raw -type d -name Cropped)"
for split in Train Val Test; do
  lower=$(echo $split | tr A-Z a-z)
  for cls in open closed semi-open; do
    dst_cls=$cls; [[ "$cls" == "semi-open" ]] && dst_cls=ajar
    mkdir -p dataset/$lower/$dst_cls
    cp -r $CROPPED/$split/$cls/* dataset/$lower/$dst_cls/
  done
done

# Sanity check: roughly 1200 images total, 3 classes
find dataset -type f -name '*.jpg' | wc -l
```

### 4. Train

```bash
yolo classify train \
    model=yolov8n-cls.pt \
    data=$PWD/dataset \
    epochs=50 \
    imgsz=320 \
    batch=64 \
    device=0 \
    patience=15 \
    project=runs \
    name=door_state_yolov8n
```

Training metrics to watch (in the per-epoch output):

| Metric | Target | Meaning |
|---|---|---|
| top1 accuracy | >= 0.85 | raw classification accuracy on val split |
| top1 (open) | >= 0.90 | per-class recall on `open` class |
| top1 (closed) | >= 0.90 | per-class recall on `closed` class |
| top1 (ajar) | >= 0.75 | harder class; rare in the dataset |

If top1 < 0.80, something went wrong - see Troubleshooting below.

### 5. Validate on the held-out test split

```bash
yolo classify val \
    model=runs/door_state_yolov8n/weights/best.pt \
    data=$PWD/dataset \
    imgsz=320 \
    split=test
```

**Acceptance threshold for the capstone**:
- Overall top-1 accuracy: >= 0.80
- Per-class top-1: open >= 0.85, closed >= 0.85, ajar >= 0.70

If you miss the ajar threshold, try re-training with
`imgsz=416 epochs=75 batch=32` - ajar is the hardest class because
the dataset has only ~150 examples. Augmentation help:
`yolo classify train ... augment=True erasing=0.4 hsv_v=0.4`.

### 6. Export to ONNX

```bash
yolo export \
    model=runs/door_state_yolov8n/weights/best.pt \
    format=onnx \
    imgsz=320 \
    opset=12 \
    simplify=true
```

Copy the resulting `best.onnx` to its destination:

```bash
cp runs/door_state_yolov8n/weights/best.onnx \
   ../../../final_capstone/compliance-engine/star_compliance/models/door_state_yolov8n.onnx
```

### 7. Verify the compliance engine picks up the weights

```bash
cd final_capstone/compliance-engine
PYTHONPATH=. python3 - <<'EOF'
from star_compliance.detectors.door_state_classifier import DoorStateClassifier
cls = DoorStateClassifier(
    weights_path="star_compliance/models/door_state_yolov8n.onnx"
)
print("available:", cls.is_available())
EOF
# expected: available: True
```

Then run the full test suite to confirm no regressions:

```bash
PYTHONPATH=. pytest tests/ -v
# expect: 62+ passed, 17+ skipped, no new failures
```

### 8. Update the models/README.md provenance

Replace the "pending training" block in
`final_capstone/compliance-engine/star_compliance/models/README.md`
with a provenance entry documenting:

- date of training
- dataset version (GitHub commit hash of gasparramoa/DoorDetect-Class-Dataset)
- hyperparameters (epochs, imgsz, batch)
- final test-split accuracy per class
- ONNX file md5 hash (from the bottom of the training script's output)
- link to the training log in `runs/door_state_yolov8n/`

### 9. Commit

```bash
git add final_capstone/compliance-engine/star_compliance/models/door_state_yolov8n.onnx
git add final_capstone/compliance-engine/star_compliance/models/README.md
git commit -m "Add trained door_state_yolov8n.onnx weights

Fine-tuned YOLOv8n-cls on gasparramoa/DoorDetect-Class-Dataset
Cropped/ subset (1206 images across open/closed/ajar). Training
log: scripts/door_state_training/.work/runs/door_state_yolov8n/.
Test-split top-1 accuracy: <fill in from step 5>. md5 of ONNX
file is recorded in models/README.md."
git push origin main
```

---

## Troubleshooting

### `gdown --folder` fails with "Access denied"

- Google Drive occasionally rate-limits folder downloads.
- Fallback: open the link in a browser, download the ZIP manually,
  extract into `scripts/door_state_training/.work/dataset-raw/`, then
  re-run the script (it'll skip the download step if the folder
  exists).

### CUDA out of memory

- Lower `batch` (try 32, then 16).
- Lower `imgsz` (try 224, then 192).
- If on a laptop GPU, close Chrome/Slack/VS Code before training.

### top1 accuracy stalls below 0.70

- Check the dataset normalization: `find dataset -type f | wc -l`
  should show roughly 1100-1200 jpgs. If it shows < 500, the
  `cp` step copied from the wrong folders - re-run manually and
  verify the Cropped/Train vs Cropped/train casing.
- Verify the class label mapping in `data=$PWD/dataset` matches
  open/closed/ajar and not open/closed/semi-open.

### `yolo export` fails with "ONNX opset not supported"

- Upgrade ultralytics: `pip install --upgrade ultralytics`
- Lower the opset version: `opset=11` or `opset=10`

### Compliance engine tests start failing after dropping the weights in

- Unlikely; the classifier falls back gracefully. If failures appear,
  check the ONNX file isn't corrupt: `python3 -c "import onnx; onnx.load('path/to/door_state_yolov8n.onnx')"`.
- Confirm the file is at the exact path the classifier expects:
  `star_compliance/models/door_state_yolov8n.onnx`.

---

## Dataset license and attribution

The DoorDetect-Class-Dataset (gasparramoa, GitHub) is shared under
the license documented at
https://github.com/gasparramoa/DoorDetect-Class-Dataset. Verify the
license allows redistribution of derived weights before committing
the ONNX file to this repository. If in doubt:

- Cite the authors in `star_compliance/models/README.md`
- Add a line in the commit message
- Do not strip the license text

---

## Alternative datasets

If DoorDetect-Class is unavailable or license-blocked, viable
substitutes:

- **DeepDoors2** (https://github.com/gasparramoa/DeepDoors2) - same
  author, 3 classes, 2D+3D annotations
- **MiguelARD/DoorDetect-Dataset** - bounding-box annotations;
  needs a classifier head added and re-annotation for state
- **DoorDet** (arXiv:2508.07714) - floor-plan derived; wrong
  modality for our camera pipeline

Pick carefully - the classifier threshold on the ajar class is the
usual bottleneck.
