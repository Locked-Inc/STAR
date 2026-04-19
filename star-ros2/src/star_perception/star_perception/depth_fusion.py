"""
Pure-Python geometry helpers for fusing a 2D bounding box with a
disparity image to produce a 3D detection in the camera optical frame.

Kept separate from the ROS2 node so the math is unit-testable without
rclpy / cv_bridge / stereo_msgs.

Conventions
-----------
- Image pixel coordinates: u (column) right, v (row) down, origin at
  the top-left of the image.
- Camera intrinsics matrix K (3x3, row-major):
      [[fx,  0, cx],
       [ 0, fy, cy],
       [ 0,  0,  1]].
- Camera optical frame (REP-103): x right, y down, z forward (into the
  scene). All 3D outputs are in this frame.
- Disparity (in pixels) -> depth (metres):
      Z = f * baseline_m / disparity_px
  where f is the rectified focal length in pixels and baseline_m is the
  stereo baseline in metres (both supplied by stereo_image_proc).
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

import numpy as np


MIN_VALID_SAMPLES_DEFAULT = 30
DISPARITY_BORDER_PCT_DEFAULT = 0.15
MAX_DEPTH_M_DEFAULT = 6.0
MIN_DEPTH_M_DEFAULT = 0.20


@dataclass(frozen=True)
class CameraIntrinsics:
    """Pinhole-camera intrinsics, in pixels for fx/fy/cx/cy."""

    fx: float
    fy: float
    cx: float
    cy: float

    @classmethod
    def from_k_matrix(cls, k_row_major: np.ndarray) -> "CameraIntrinsics":
        """Construct from the row-major K matrix in sensor_msgs/CameraInfo."""
        k = np.asarray(k_row_major, dtype=np.float64).reshape(3, 3)
        return cls(fx=float(k[0, 0]), fy=float(k[1, 1]),
                   cx=float(k[0, 2]), cy=float(k[1, 2]))


@dataclass(frozen=True)
class Detection3D:
    """A 3D detection in the camera optical frame, axis-aligned bbox."""

    class_id: int
    class_name: str
    score: float
    cx_m: float       # bbox centre X in metres (camera optical frame).
    cy_m: float       # bbox centre Y in metres.
    cz_m: float       # bbox centre Z in metres (depth).
    size_x_m: float   # bbox extent in X (metres).
    size_y_m: float   # bbox extent in Y (metres).
    size_z_m: float   # bbox extent in Z (metres) -- coarse, see notes.
    sample_count: int  # number of valid disparity samples used.


def median_disparity_in_bbox(disparity: np.ndarray,
                             x_min: float, y_min: float,
                             x_max: float, y_max: float,
                             border_pct: float
                             = DISPARITY_BORDER_PCT_DEFAULT
                             ) -> tuple[float, int]:
    """
    Median of valid disparity samples inside the inset 2D bounding box.

    Args:
        disparity:   HxW float32 array of disparities in pixels. NaN, inf,
                     and non-positive values are treated as invalid.
        x_min, y_min, x_max, y_max: bbox in pixel coordinates of the
                     same image. Will be clamped to the image bounds.
        border_pct:  Fraction of bbox width/height to crop off each
                     edge before sampling. Default 0.15 (15 percent).
                     This rejects edge artefacts where stereo matching
                     tends to fail.

    Returns:
        (median_disparity_px, valid_sample_count). If there are no
        valid samples, returns (nan, 0).
    """
    if disparity.ndim != 2:
        raise ValueError("disparity must be a 2D array.")
    if not (0.0 <= border_pct < 0.5):
        raise ValueError("border_pct must be in [0, 0.5).")

    h, w = disparity.shape
    bw = max(0.0, x_max - x_min)
    bh = max(0.0, y_max - y_min)
    inset_x = bw * border_pct
    inset_y = bh * border_pct
    x0 = max(0, int(np.floor(x_min + inset_x)))
    y0 = max(0, int(np.floor(y_min + inset_y)))
    x1 = min(w, int(np.ceil(x_max - inset_x)))
    y1 = min(h, int(np.ceil(y_max - inset_y)))
    if x1 <= x0 or y1 <= y0:
        return float("nan"), 0

    roi = disparity[y0:y1, x0:x1]
    finite_mask = np.isfinite(roi) & (roi > 0.0)
    valid = roi[finite_mask]
    if valid.size == 0:
        return float("nan"), 0
    return float(np.median(valid)), int(valid.size)


def depth_from_disparity(disparity_px: float,
                         focal_px: float,
                         baseline_m: float) -> float:
    """Z = f * T / d. Returns nan if disparity is non-positive."""
    if not (disparity_px > 0.0):
        return float("nan")
    if not (focal_px > 0.0 and baseline_m > 0.0):
        raise ValueError("focal_px and baseline_m must be positive.")
    return float(focal_px * baseline_m / disparity_px)


def back_project_pixel(u: float, v: float, z_m: float,
                       k: CameraIntrinsics) -> tuple[float, float, float]:
    """Pinhole back-projection of pixel (u, v) at depth z_m into 3D."""
    if not (z_m > 0.0):
        raise ValueError("z_m must be positive.")
    x = (u - k.cx) * z_m / k.fx
    y = (v - k.cy) * z_m / k.fy
    return float(x), float(y), float(z_m)


def fuse_bbox_to_3d(disparity: np.ndarray,
                    intrinsics: CameraIntrinsics,
                    focal_px: float,
                    baseline_m: float,
                    class_id: int,
                    class_name: str,
                    score: float,
                    x_min: float, y_min: float,
                    x_max: float, y_max: float,
                    border_pct: float
                    = DISPARITY_BORDER_PCT_DEFAULT,
                    min_valid_samples: int
                    = MIN_VALID_SAMPLES_DEFAULT,
                    min_depth_m: float = MIN_DEPTH_M_DEFAULT,
                    max_depth_m: float = MAX_DEPTH_M_DEFAULT
                    ) -> Optional[Detection3D]:
    """
    Convert a 2D detection + disparity image into a Detection3D, or
    return None if the depth estimate is rejected.

    The 3D bbox is approximated from the 2D bbox by:
      - centre (cx_m, cy_m, cz_m) from back-projection of bbox centre
        at the median ROI depth;
      - size_x_m, size_y_m from bbox pixel extents scaled by Z/f;
      - size_z_m from the spread (95th - 5th percentile) of the valid
        disparity samples. Coarse but useful for RViz visualisation.

    Returns None when:
      - the bbox has fewer than min_valid_samples valid disparities;
      - the median depth falls outside [min_depth_m, max_depth_m].
    """
    median_d, sample_count = median_disparity_in_bbox(
        disparity, x_min, y_min, x_max, y_max, border_pct,
    )
    if sample_count < min_valid_samples:
        return None
    z_m = depth_from_disparity(median_d, focal_px, baseline_m)
    if not np.isfinite(z_m):
        return None
    if not (min_depth_m <= z_m <= max_depth_m):
        return None

    u_c = 0.5 * (x_min + x_max)
    v_c = 0.5 * (y_min + y_max)
    cx_m, cy_m, cz_m = back_project_pixel(u_c, v_c, z_m, intrinsics)

    size_x_m = (x_max - x_min) * z_m / intrinsics.fx
    size_y_m = (y_max - y_min) * z_m / intrinsics.fy

    # Coarse depth extent from disparity spread: small numbers OK.
    # We re-sample the same ROI to get spread; cheap relative to NPU.
    h, w = disparity.shape
    bw = max(0.0, x_max - x_min)
    bh = max(0.0, y_max - y_min)
    inset_x = bw * border_pct
    inset_y = bh * border_pct
    x0 = max(0, int(np.floor(x_min + inset_x)))
    y0 = max(0, int(np.floor(y_min + inset_y)))
    x1 = min(w, int(np.ceil(x_max - inset_x)))
    y1 = min(h, int(np.ceil(y_max - inset_y)))
    roi = disparity[y0:y1, x0:x1]
    finite_mask = np.isfinite(roi) & (roi > 0.0)
    valid = roi[finite_mask]
    if valid.size >= 4:
        d_low, d_high = np.percentile(valid, [5.0, 95.0])
        z_low = depth_from_disparity(float(d_high), focal_px, baseline_m)
        z_high = depth_from_disparity(float(d_low), focal_px, baseline_m)
        size_z_m = float(max(0.05, z_high - z_low))
    else:
        size_z_m = 0.10  # default 10 cm extent.

    return Detection3D(
        class_id=int(class_id),
        class_name=str(class_name),
        score=float(score),
        cx_m=cx_m, cy_m=cy_m, cz_m=cz_m,
        size_x_m=float(max(0.01, size_x_m)),
        size_y_m=float(max(0.01, size_y_m)),
        size_z_m=size_z_m,
        sample_count=int(sample_count),
    )
