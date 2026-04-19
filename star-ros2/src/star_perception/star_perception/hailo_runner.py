"""
HailoRT wrapper for YOLOv8 inference on the Hailo-8L NPU.

This module is intentionally ROS2-free so the letterbox / de-letterbox /
NMS-decode math can be unit-tested on any host without an NPU. The
hailort import is lazy and the constructor accepts an injectable
inference backend; tests substitute a fake backend.

The YOLOv8 .hef from the Hailo Model Zoo runs NMS on-chip and emits
per-class detections in normalized [0, 1] coordinates of the 640x640
letterboxed input. This wrapper:

  1. Letterbox-resizes the source RGB image to 640x640 with grey padding,
     remembering the scale and padding offsets.
  2. Runs the HEF (or the injected backend).
  3. Filters detections by score, removes the letterbox padding, and
     scales back to source-image pixel coordinates.
"""

from __future__ import annotations

import argparse
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Optional

import numpy as np


YOLOV8_INPUT_SIZE = 640
LETTERBOX_PAD_VALUE = 114  # YOLOv8 convention (mid-grey).
COCO_CLASSES = (
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train",
    "truck", "boat", "traffic light", "fire hydrant", "stop sign",
    "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep",
    "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella",
    "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard",
    "sports ball", "kite", "baseball bat", "baseball glove", "skateboard",
    "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork",
    "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange",
    "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
    "couch", "potted plant", "bed", "dining table", "toilet", "tv",
    "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave",
    "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase",
    "scissors", "teddy bear", "hair drier", "toothbrush",
)


@dataclass(frozen=True)
class Detection:
    """A single 2D detection in source-image pixel coordinates."""

    class_id: int
    class_name: str
    score: float
    x_min: float
    y_min: float
    x_max: float
    y_max: float

    @property
    def width(self) -> float:
        return self.x_max - self.x_min

    @property
    def height(self) -> float:
        return self.y_max - self.y_min

    @property
    def cx(self) -> float:
        return 0.5 * (self.x_min + self.x_max)

    @property
    def cy(self) -> float:
        return 0.5 * (self.y_min + self.y_max)


@dataclass(frozen=True)
class LetterboxParams:
    """Geometry of a letterbox transform from source HxW to target SxS."""

    src_h: int
    src_w: int
    target: int
    scale: float
    pad_x: int
    pad_y: int


def compute_letterbox(src_h: int, src_w: int,
                      target: int = YOLOV8_INPUT_SIZE) -> LetterboxParams:
    """Compute scale + padding to fit src into a square target canvas."""
    if src_h <= 0 or src_w <= 0:
        raise ValueError("Source dimensions must be positive.")
    scale = float(target) / float(max(src_h, src_w))
    new_h = int(round(src_h * scale))
    new_w = int(round(src_w * scale))
    pad_x = (target - new_w) // 2
    pad_y = (target - new_h) // 2
    return LetterboxParams(
        src_h=src_h, src_w=src_w, target=target,
        scale=scale, pad_x=pad_x, pad_y=pad_y,
    )


def letterbox_image(rgb: np.ndarray,
                    params: LetterboxParams) -> np.ndarray:
    """
    Resize and pad rgb into params.target x params.target uint8 RGB.

    Uses nearest-neighbour resize via numpy (avoids cv2 dependency in
    the unit tests). Production callers may swap this for cv2.resize
    with INTER_LINEAR for better quality, at the cost of a runtime dep.
    """
    if rgb.ndim != 3 or rgb.shape[2] != 3:
        raise ValueError("Expected HxWx3 RGB image.")
    if rgb.dtype != np.uint8:
        raise ValueError("Expected uint8 RGB image.")
    src_h, src_w = rgb.shape[:2]
    if src_h != params.src_h or src_w != params.src_w:
        raise ValueError("Image shape does not match letterbox params.")

    new_h = int(round(src_h * params.scale))
    new_w = int(round(src_w * params.scale))
    # Nearest-neighbour resize via index lookup (no cv2 dep).
    row_idx = (np.arange(new_h) / params.scale).astype(np.int32)
    col_idx = (np.arange(new_w) / params.scale).astype(np.int32)
    row_idx = np.clip(row_idx, 0, src_h - 1)
    col_idx = np.clip(col_idx, 0, src_w - 1)
    resized = rgb[row_idx][:, col_idx]

    canvas = np.full(
        (params.target, params.target, 3),
        LETTERBOX_PAD_VALUE,
        dtype=np.uint8,
    )
    canvas[params.pad_y:params.pad_y + new_h,
           params.pad_x:params.pad_x + new_w] = resized
    return canvas


def deletterbox_bbox(x_min: float, y_min: float,
                     x_max: float, y_max: float,
                     params: LetterboxParams) -> tuple[float, float,
                                                      float, float]:
    """
    Map a bbox in [0,1] coords of the letterboxed canvas back to the
    source image's pixel coordinates.

    Inputs are normalized to params.target (i.e. multiply by target to
    get canvas pixels). Outputs are clamped to [0, src_w-1] / [0, src_h-1].
    """
    t = float(params.target)
    cx_min = x_min * t - params.pad_x
    cx_max = x_max * t - params.pad_x
    cy_min = y_min * t - params.pad_y
    cy_max = y_max * t - params.pad_y

    sx_min = cx_min / params.scale
    sx_max = cx_max / params.scale
    sy_min = cy_min / params.scale
    sy_max = cy_max / params.scale

    sx_min = float(np.clip(sx_min, 0.0, params.src_w - 1))
    sx_max = float(np.clip(sx_max, 0.0, params.src_w - 1))
    sy_min = float(np.clip(sy_min, 0.0, params.src_h - 1))
    sy_max = float(np.clip(sy_max, 0.0, params.src_h - 1))
    return sx_min, sy_min, sx_max, sy_max


# Type of an inference backend: takes uint8 (1, H, W, 3) or (H, W, 3)
# and returns a list of (class_id, score, x_min_n, y_min_n, x_max_n,
# y_max_n) tuples in normalized [0, 1] canvas coordinates. This shape
# matches the post-NMS output of the Hailo Model Zoo YOLOv8 .hef.
InferenceBackend = Callable[[np.ndarray],
                            list[tuple[int, float, float, float,
                                       float, float]]]


class HailoYoloRunner:
    """
    Pre + post-processing wrapper around a Hailo YOLOv8 .hef.

    The actual NPU call is delegated to a backend so this class can be
    constructed in unit tests with a fake. The default constructor
    builds the real HailoRT backend from an .hef path; the static
    `with_backend` constructor accepts an injected callable.
    """

    def __init__(self,
                 hef_path: str | Path,
                 score_threshold: float = 0.35,
                 input_size: int = YOLOV8_INPUT_SIZE,
                 class_names: tuple[str, ...] = COCO_CLASSES,
                 backend: Optional[InferenceBackend] = None) -> None:
        if input_size <= 0:
            raise ValueError("input_size must be positive.")
        if not (0.0 < score_threshold < 1.0):
            raise ValueError("score_threshold must be in (0, 1).")
        self._input_size = int(input_size)
        self._score_threshold = float(score_threshold)
        self._class_names = tuple(class_names)
        self._hef_path = Path(hef_path)
        self._backend = backend if backend is not None else \
            self._build_default_backend(self._hef_path, self._input_size)

    @classmethod
    def with_backend(cls, backend: InferenceBackend,
                     score_threshold: float = 0.35,
                     input_size: int = YOLOV8_INPUT_SIZE,
                     class_names: tuple[str, ...] = COCO_CLASSES
                     ) -> "HailoYoloRunner":
        """Construct a runner with a pre-built backend (test seam)."""
        return cls(
            hef_path=Path("/dev/null"),
            score_threshold=score_threshold,
            input_size=input_size,
            class_names=class_names,
            backend=backend,
        )

    def infer(self, rgb_image: np.ndarray) -> list[Detection]:
        """
        Run inference on an HxWx3 uint8 RGB image.

        Returns detections in source-image pixel coordinates, filtered
        by `score_threshold`, sorted by score descending.
        """
        if rgb_image.ndim != 3 or rgb_image.shape[2] != 3:
            raise ValueError("Expected HxWx3 RGB image.")
        if rgb_image.dtype != np.uint8:
            raise ValueError("Expected uint8 RGB image.")

        params = compute_letterbox(rgb_image.shape[0], rgb_image.shape[1],
                                   self._input_size)
        canvas = letterbox_image(rgb_image, params)
        raw = self._backend(canvas)

        detections: list[Detection] = []
        for class_id, score, xn0, yn0, xn1, yn1 in raw:
            if score < self._score_threshold:
                continue
            x_min, y_min, x_max, y_max = deletterbox_bbox(
                xn0, yn0, xn1, yn1, params,
            )
            if x_max <= x_min or y_max <= y_min:
                continue
            name = self._class_names[class_id] \
                if 0 <= class_id < len(self._class_names) \
                else f"class_{class_id}"
            detections.append(Detection(
                class_id=int(class_id),
                class_name=name,
                score=float(score),
                x_min=x_min, y_min=y_min,
                x_max=x_max, y_max=y_max,
            ))
        detections.sort(key=lambda d: d.score, reverse=True)
        return detections

    @staticmethod
    def _build_default_backend(hef_path: Path,
                               input_size: int) -> InferenceBackend:
        """
        Lazily construct the HailoRT backend.

        Imports are inside the function so that test environments
        without hailort installed can still import this module.
        """
        try:
            import hailo_platform as hp
        except ImportError as exc:  # pragma: no cover - hardware path
            raise RuntimeError(
                "hailo_platform is not installed. On the Pi 5, run "
                "`sudo apt install hailo-all` and reboot.") from exc

        if not hef_path.exists():
            raise FileNotFoundError(
                f"HEF not found at {hef_path}. Run "
                "scripts/download_yolov8s_hef.sh on the Pi 5.")

        hef = hp.HEF(str(hef_path))
        target = hp.VDevice()
        configure_params = hp.ConfigureParams.create_from_hef(
            hef=hef, interface=hp.HailoStreamInterface.PCIe,
        )
        network_group = target.configure(hef, configure_params)[0]
        ng_params = network_group.create_params()
        input_vstreams_params = hp.InputVStreamParams.make(
            network_group, format_type=hp.FormatType.UINT8,
        )
        output_vstreams_params = hp.OutputVStreamParams.make(
            network_group, format_type=hp.FormatType.FLOAT32,
        )
        input_info = hef.get_input_vstream_infos()[0]

        def backend(canvas: np.ndarray) -> list[tuple[int, float, float,
                                                      float, float, float]]:
            with network_group.activate(ng_params):
                with hp.InferVStreams(network_group,
                                      input_vstreams_params,
                                      output_vstreams_params) as pipeline:
                    feed = {input_info.name: np.expand_dims(canvas, axis=0)}
                    outputs = pipeline.infer(feed)
            return _decode_hailo_yolov8_output(outputs, input_size)

        return backend


def _decode_hailo_yolov8_output(outputs: dict, input_size: int
                                ) -> list[tuple[int, float, float,
                                                float, float, float]]:
    """
    Decode the post-NMS output of the Hailo Model Zoo YOLOv8 .hef.

    The HEF emits per-class detection arrays. The shape convention used
    by the Hailo zoo's YOLOv8 NMS post-processor is one array per class,
    each of shape (max_dets_per_class, 5) holding (y_min, x_min, y_max,
    x_max, score) in normalized [0, 1] coordinates. We re-emit as
    (class_id, score, x_min, y_min, x_max, y_max).
    """
    decoded: list[tuple[int, float, float, float, float, float]] = []
    if not outputs:
        return decoded
    name = next(iter(outputs))
    arr = outputs[name]
    if arr.ndim == 4 and arr.shape[0] == 1:
        arr = arr[0]
    # arr shape now: (num_classes, max_dets, 5) or list-of-arrays.
    if isinstance(arr, list):
        per_class = arr
    else:
        per_class = list(arr)
    for class_id, dets in enumerate(per_class):
        if dets is None or len(dets) == 0:
            continue
        for det in dets:
            if len(det) < 5:
                continue
            y_min, x_min, y_max, x_max, score = (
                float(det[0]), float(det[1]), float(det[2]),
                float(det[3]), float(det[4]),
            )
            if score <= 0.0:
                continue
            decoded.append(
                (int(class_id), score, x_min, y_min, x_max, y_max)
            )
    return decoded


def _cli() -> None:  # pragma: no cover - manual smoke test
    """Smoke-test entrypoint: load HEF, infer on an image, print FPS."""
    parser = argparse.ArgumentParser(description="Hailo YOLO smoke test.")
    parser.add_argument("--hef", required=True, help="Path to .hef file.")
    parser.add_argument("--image", required=True, help="Path to RGB image.")
    parser.add_argument("--iters", type=int, default=50,
                        help="Inference iterations for FPS.")
    parser.add_argument("--score", type=float, default=0.35)
    args = parser.parse_args()

    try:
        import cv2
    except ImportError as exc:
        raise SystemExit("opencv-python is required for the CLI.") from exc

    bgr = cv2.imread(args.image)
    if bgr is None:
        raise SystemExit(f"Could not read image {args.image}.")
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)

    runner = HailoYoloRunner(args.hef, score_threshold=args.score)
    detections = runner.infer(rgb)
    print(f"First-pass detections: {len(detections)}")
    for d in detections[:10]:
        print(f"  {d.class_name:>14} {d.score:.2f} "
              f"[{d.x_min:.0f},{d.y_min:.0f},"
              f"{d.x_max:.0f},{d.y_max:.0f}]")

    t0 = time.perf_counter()
    for _ in range(args.iters):
        runner.infer(rgb)
    elapsed = time.perf_counter() - t0
    fps = args.iters / elapsed if elapsed > 0 else 0.0
    print(f"FPS over {args.iters} iters: {fps:.1f}")


if __name__ == "__main__":  # pragma: no cover
    _cli()
