# STAR ML model artifacts

This directory holds trained model weights consumed by the compliance
engine. Weights are committed to the repository so a CI build can
produce deterministic runs; the raw training data lives externally.

## door_state_yolov8n.onnx

**Status:** **pending training** (plan task #4)

**Purpose:** Classify a rectified left camera frame into
`{open, closed, ajar, none}` for the ADA 404.2.3 door clear-width
pipeline.

**Base model:** YOLOv8n (Ultralytics, MIT license)

**Dataset:** DoorDet (arXiv:2508.07714). Multi-class door dataset with
~30k annotated images. Download instructions at the paper's GitHub.
Verify dataset license before redistributing any fine-tuned weights.

**Training recipe (to run on a CUDA GPU):**

```bash
pip install ultralytics
git clone https://github.com/ultralytics/ultralytics
cd ultralytics

# Prepare DoorDet in the standard YOLOv8 format
# (data.yaml with train / val paths, class list: [open, closed, ajar, none])

yolo detect train \
    model=yolov8n.pt \
    data=doordet.yaml \
    imgsz=320 \
    epochs=50 \
    patience=10 \
    batch=64 \
    device=0

# Export to ONNX
yolo export model=runs/detect/train/weights/best.pt \
    format=onnx \
    imgsz=320 \
    simplify=True
```

**Acceptance:**

- mAP@0.5 > 0.75 on `open` / `closed`
- mAP@0.5 > 0.55 on `ajar` (harder class)
- Latency < 150 ms per frame on Pi5 CPU via OpenCV DNN

Commit the resulting `best.onnx` here as `door_state_yolov8n.onnx`
with a provenance line appended to this README documenting the hash,
dataset version, and training-log commit.

**Fallback:** When this file is absent, the
`door_state_classifier.DoorStateClassifier` falls back to a geometric
heuristic on the `/stereo/points2` cloud. The classifier result
documents `source="heuristic-fallback"` in its output so reports
can disclose degraded-mode operation.

## Future artifacts (not yet scheduled)

- `protruding_object_filter.onnx` - optional classifier to distinguish
  structural features (sconces, handrails) from ADA 307 violations.
  Likely deferred beyond the capstone window.
