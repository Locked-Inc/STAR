# Hailo .hef models

This directory holds pre-compiled Hailo Executable Format (`.hef`) models
for the Hailo-8L NPU on the Raspberry Pi AI HAT+ 13 TOPS.

The `.hef` binaries are NOT committed to git. Run the install script on
the Pi 5 to download them from the Hailo Model Zoo:

```
./scripts/download_yolov8s_hef.sh
```

## Files placed here at install time

- `yolov8s_h8l.hef` -- YOLOv8s trained on COCO, compiled for Hailo-8L.
  640x640 input, 80 classes, ~30 FPS on Hailo-8L.

## Choosing a different model

Edit `scripts/download_yolov8s_hef.sh` to point at a different Hailo
Model Zoo URL. YOLOv6n is a smaller fallback if power or thermal
budget is tight; YOLOv11s and YOLOv8m are reasonable upgrades when
the Hailo-8L is otherwise idle.
