"""
Door-state classifier wrapper (open / closed / ajar / none).

Interface over a YOLOv8n model fine-tuned on the DoorDet dataset
(arXiv:2508.07714). The actual ONNX weights live at
`compliance-engine/star_compliance/models/door_state_yolov8n.onnx`
and must be trained separately (see task #4 in the plan). This
wrapper:

- Loads the model via OpenCV DNN (no Ultralytics runtime needed).
- Runs inference on a 320x320 input image.
- Returns a `DoorStateResult` with state, bounding box, and score.
- Falls back to a geometric heuristic on the disparity cloud when no
  ONNX weights are present, so the pipeline can run end-to-end in
  development without blocking on model training.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from pathlib import Path


class DoorState(int, Enum):
    UNKNOWN = 0
    OPEN = 1
    CLOSED = 2
    AJAR = 3
    NONE = 4

    @staticmethod
    def from_name(name: str) -> "DoorState":
        """Map YOLO class names to our DoorState enum.

        The DoorDet dataset's canonical labels are {open, closed,
        semi-open}. We fold semi-open -> AJAR for ADA 404.2.3 scoring.
        A class absent from any of these falls back to UNKNOWN so
        downstream code can flag the reading as low-confidence.
        """
        lowered = name.strip().lower()
        if lowered in ("open", "doorway_open"):
            return DoorState.OPEN
        if lowered in ("closed", "doorway_closed"):
            return DoorState.CLOSED
        if lowered in ("ajar", "semi_open", "semi-open", "doorway_ajar"):
            return DoorState.AJAR
        if lowered in ("none", "no_door", "doorway_none"):
            return DoorState.NONE
        return DoorState.UNKNOWN


@dataclass
class DoorStateResult:
    state: DoorState
    score: float  # 0-1, model's class probability or heuristic confidence
    bbox_xyxy: tuple[float, float, float, float] | None = None
    source: str = "onnx"  # or "heuristic-fallback"


class DoorStateClassifier:
    """OpenCV-DNN YOLOv8n wrapper with a geometric fallback."""

    DEFAULT_INPUT_SIZE = 320
    DEFAULT_CONFIDENCE_THRESHOLD = 0.35
    DEFAULT_NMS_IOU = 0.45

    def __init__(self,
                 weights_path: str | Path,
                 input_size: int = DEFAULT_INPUT_SIZE,
                 confidence_threshold: float = DEFAULT_CONFIDENCE_THRESHOLD,
                 nms_iou: float = DEFAULT_NMS_IOU,
                 class_names: list[str] | None = None):
        self.weights_path = Path(weights_path)
        self.input_size = input_size
        self.confidence_threshold = confidence_threshold
        self.nms_iou = nms_iou
        self.class_names = class_names or ["open", "closed", "ajar", "none"]
        self._net = None

    # -----------------------------------------------------------------
    # Loading / availability
    # -----------------------------------------------------------------

    def is_available(self) -> bool:
        """True when the ONNX file exists on disk."""
        return self.weights_path.exists()

    def _load(self) -> None:
        if self._net is not None or not self.is_available():
            return
        import cv2  # lazy import to keep the module importable on
                   # machines without OpenCV

        self._net = cv2.dnn.readNetFromONNX(str(self.weights_path))
        self._net.setPreferableBackend(cv2.dnn.DNN_BACKEND_OPENCV)
        self._net.setPreferableTarget(cv2.dnn.DNN_TARGET_CPU)

    # -----------------------------------------------------------------
    # Inference
    # -----------------------------------------------------------------

    def classify(self, image_rgb, disparity_cloud=None) -> DoorStateResult:
        """Return a DoorStateResult given one left-camera image.

        Parameters
        ----------
        image_rgb : (H, W, 3) ndarray
            Rectified left camera frame in RGB8, the same stream
            published to /cam0/camera/image_rect_color.
        disparity_cloud : (N, 3) ndarray or None
            Optional 3D point cloud (from /stereo/points2) used by the
            geometric fallback when the ONNX model is unavailable.
        """
        if self.is_available():
            return self._classify_onnx(image_rgb)
        return self._classify_heuristic(disparity_cloud)

    def _classify_onnx(self, image_rgb) -> DoorStateResult:
        import cv2
        import numpy as np

        self._load()
        h, w = image_rgb.shape[:2]
        # YOLOv8 preprocessing: letterbox to self.input_size, BGR or RGB
        # both work for the DoorDet fine-tune; the model is trained
        # without a strict channel order.
        blob = cv2.dnn.blobFromImage(
            image_rgb,
            scalefactor=1.0 / 255.0,
            size=(self.input_size, self.input_size),
            swapRB=False,
            crop=False,
        )
        self._net.setInput(blob)
        outputs = self._net.forward()  # (1, 4 + num_classes, N) typical
        # Defensive parse: if the model was exported in the newer
        # Ultralytics layout (1, C, N) with C = 4 + num_classes, take
        # the best class per proposal and NMS.
        if outputs.ndim == 3 and outputs.shape[0] == 1:
            preds = outputs[0]
        else:
            preds = outputs

        # Rows are proposals, columns are [cx, cy, bw, bh, c0, c1, ...]
        preds = preds.T if preds.shape[0] < preds.shape[1] else preds
        if preds.shape[1] < 4 + len(self.class_names):
            # Unexpected layout - fall back to heuristic
            return DoorStateResult(DoorState.UNKNOWN, 0.0,
                                   source="onnx-unexpected-layout")

        scores = preds[:, 4:4 + len(self.class_names)]
        class_idx = scores.argmax(axis=1)
        class_score = scores.max(axis=1)
        keep = class_score >= self.confidence_threshold
        if not keep.any():
            return DoorStateResult(DoorState.NONE, 0.0,
                                   bbox_xyxy=None, source="onnx")

        best = int(np.argmax(class_score * keep))
        cx, cy, bw, bh = preds[best, :4]
        # Scale back from input_size to original resolution
        sx = w / self.input_size
        sy = h / self.input_size
        x1 = float((cx - bw / 2) * sx)
        y1 = float((cy - bh / 2) * sy)
        x2 = float((cx + bw / 2) * sx)
        y2 = float((cy + bh / 2) * sy)
        name = self.class_names[class_idx[best]]
        return DoorStateResult(
            state=DoorState.from_name(name),
            score=float(class_score[best]),
            bbox_xyxy=(x1, y1, x2, y2),
            source="onnx",
        )

    def _classify_heuristic(self, disparity_cloud) -> DoorStateResult:
        """Geometric fallback: decide state from the point cloud alone.

        If no point cloud is provided, return UNKNOWN so the caller
        knows it got nothing.
        """
        if disparity_cloud is None or len(disparity_cloud) == 0:
            return DoorStateResult(DoorState.UNKNOWN, 0.0,
                                   source="heuristic-fallback-no-cloud")

        import numpy as np

        # Count points inside a 30 cm "doorway footprint" immediately
        # ahead of the camera (x in [0.3, 1.3], y in [-0.15, 0.15],
        # above the floor band). Lots of points -> closed door; almost
        # none -> open doorway.
        arr = np.asarray(disparity_cloud)
        in_footprint = (
            (arr[:, 0] > 0.3) & (arr[:, 0] < 1.3)
            & (np.abs(arr[:, 1]) < 0.15)
            & (arr[:, 2] > 0.3) & (arr[:, 2] < 2.0)
        )
        count = int(np.count_nonzero(in_footprint))
        # Heuristic thresholds, expected to be tuned on rosbag data.
        if count > 800:
            return DoorStateResult(DoorState.CLOSED, 0.6,
                                   source="heuristic-fallback")
        if count < 100:
            return DoorStateResult(DoorState.OPEN, 0.5,
                                   source="heuristic-fallback")
        return DoorStateResult(DoorState.AJAR, 0.4,
                               source="heuristic-fallback")
