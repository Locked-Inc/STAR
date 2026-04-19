# Hailo .hef models + class lists

This directory holds pre-compiled Hailo Executable Format (`.hef`)
models for the Hailo-8L NPU on the Raspberry Pi AI HAT+ 13 TOPS, and
YAML class-name lists that map HEF class indices to human-readable
labels.

The `.hef` binaries are NOT committed to git (see `.gitignore`). YAML
class lists ARE committed.

## Files in this directory

| file | committed? | purpose |
|---|---|---|
| `yolov8s_h8l.hef` | no (downloaded) | YOLOv8s / COCO 80 classes, 640x640, ~30 FPS on Hailo-8L. Default shipped model. |
| `yolov8n-oiv7.hef` | no (compiled off-Pi) | YOLOv8n / Open Images V7 601 classes. ADA-relevant labels (Door, Stairs, Handrail, Wheelchair, Cane...). Compile on x86 via `scripts/compile_yolov8n_oiv7_hef.sh`. |
| `coco_classes.yaml` | yes | 80 COCO class names in HEF index order. |
| `oiv7_classes.yaml` | yes | 601 Open Images V7 class names in HEF index order. |
| `yolov8n-oiv7.yaml` | yes | Hailo Model Zoo network config for the OIV7 compile step. |

## Choosing a model at runtime

The `hailo_yolo_node` takes two parameters:

```bash
ros2 launch star_perception perception.launch.py \
  hef_path:=/path/to/model.hef \
  classes_yaml:=/path/to/classes.yaml
```

When `classes_yaml` is empty the node falls back to the built-in
COCO 80 list (matches the default `yolov8s_h8l.hef`). Point it at
`oiv7_classes.yaml` when running `yolov8n-oiv7.hef`.

## Getting `yolov8s_h8l.hef`

Prebuilt in the Hailo Model Zoo. On the Pi 5:

```bash
bash scripts/download_yolov8s_hef.sh
```

## Getting `yolov8n-oiv7.hef`

**There is no prebuilt OIV7 HEF in the Hailo Model Zoo** as of
v2.18 — all public Hailo-8L YOLOs are COCO-trained. You compile it
once on an x86 dev box and copy the resulting `.hef` to the Pi 5.

### Prerequisites on the x86 host

- Linux x86_64 (Ubuntu 22.04 / Debian 12 tested by Hailo)
- Hailo Dataflow Compiler installed (Hailo Developer Zone
  download; free sign-up)
- `pip install ultralytics hailo_model_zoo`
- ~64-256 representative JPEG/PNG calibration images in
  `scripts/calib/` (or pass `--calib-dir`). The Open Images V7
  validation split is ideal; any mixed-class indoor/outdoor set
  works. Calibration drives 8-bit quantization accuracy.

### Compile

```bash
# On the x86 host, inside the star_perception package:
cd star-ros2/src/star_perception
./scripts/compile_yolov8n_oiv7_hef.sh --calib-dir /path/to/calib/images
```

The script:

1. `yolo export model=yolov8n-oiv7.pt format=onnx imgsz=640 opset=11 simplify=True`
   — Ultralytics auto-downloads `yolov8n-oiv7.pt` (AGPL-3.0, 3.2M
   parameters) on first use.
2. `hailomz compile --ckpt yolov8n-oiv7.onnx --calib-path ... --yaml models/yolov8n-oiv7.yaml --classes 601 --hw-arch hailo8l`
   — produces `yolov8n-oiv7.hef`. ~15-30 minutes on current hardware.
3. Tells you where to copy the HEF on the Pi 5.

### On the Pi 5

```bash
scp x86host:yolov8n-oiv7.hef /workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models/
ros2 launch star_perception perception.launch.py \
  hef_path:=/workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models/yolov8n-oiv7.hef \
  classes_yaml:=/workspaces/STAR/star-ros2/install/star_perception/share/star_perception/models/oiv7_classes.yaml
```

No rebuild needed — the HEF and YAML are pure data files in the
install share directory. Restart the perception launch to pick
them up.

### Expected OIV7 performance

- mAP: ~18 across 600 classes (vs COCO YOLOv8s's 44.9 across 80).
  Per-class accuracy on doors, stairs, handrails, wheelchairs is
  "good enough to tag the detection" — **not** the measurement
  source.
- Latency / FPS: similar to YOLOv8n / COCO (same backbone); a rough
  benchmark comparable to COCO's ~30 FPS on Hailo-8L is expected.

### Division of labour

YOLO tells us **what is in the frame** (class label + confidence).
The geometric pipeline (LiDAR doorway width, RANSAC wall/jamb plane
fitter, StereoSGBM disparity, IMU floor frame) measures **how large
/ how steep / how tall** the object is. ADA numbers (32-inch door
clearance, 4.76-deg ramp slope, 0.5-inch threshold height, etc.)
come from geometry — not from the bounding box extent of a YOLO
detection. Adding new class labels (via OIV7 or a custom HEF) only
improves the gating + semantic tagging, not the measurement
accuracy.
