"""
Corridor medial-axis analysis for ADA 403.5 accessible-path-width.

Given an OccupancyGrid (slam_toolbox 2D), compute the medial-axis
transform of the free-space region and return the local clearance
(2x distance transform) at every centerline pixel. The minimum
clearance along each connected segment is the ADA accessible path
width.

ADA 2010 Standards 403.5 requires >= 36 inches (0.9144 m) of
continuous clear width along accessible routes.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


INCH_TO_M = 0.0254
ADA_403_5_MIN_WIDTH_M = 36.0 * INCH_TO_M


@dataclass
class CorridorClearance:
    """Per-pixel clearance result from the medial-axis transform."""
    skeleton_mask: np.ndarray      # bool, shape (H, W)
    clearance_m: np.ndarray        # float32, shape (H, W), 0 on non-skel pixels
    min_clearance_m: float         # smallest non-zero clearance on the skeleton
    min_clearance_xy: tuple[int, int]  # pixel coords of the minimum

    def violates_403_5(self,
                       threshold_m: float = ADA_403_5_MIN_WIDTH_M) -> bool:
        return 0.0 < self.min_clearance_m < threshold_m


def compute_corridor_clearance(occupancy: np.ndarray,
                               resolution_m: float,
                               free_threshold: int = 50) -> CorridorClearance:
    """Run medial-axis + distance-transform on a slam_toolbox map.

    Parameters
    ----------
    occupancy : (H, W) ndarray
        OccupancyGrid data. Standard ROS encoding: -1 = unknown,
        0 = free, 100 = occupied. Integers between 1 and 99 are
        probabilities; values >= `free_threshold` are treated as
        occupied for the corridor computation.
    resolution_m : float
        Map resolution (meters per cell).
    free_threshold : int
        Minimum occupancy probability to treat a cell as obstacle.
    """
    try:
        from skimage.morphology import medial_axis
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError(
            "scikit-image is required for corridor clearance analysis. "
            "pip install scikit-image in the compliance-engine env."
        ) from exc

    free_mask = (occupancy >= 0) & (occupancy < free_threshold)
    skeleton, distance = medial_axis(free_mask, return_distance=True)
    clearance_px = np.where(skeleton, 2.0 * distance, 0.0)
    clearance_m = (clearance_px * resolution_m).astype(np.float32)

    nonzero = clearance_m[skeleton]
    if nonzero.size == 0:
        return CorridorClearance(
            skeleton_mask=skeleton,
            clearance_m=clearance_m,
            min_clearance_m=0.0,
            min_clearance_xy=(0, 0),
        )
    min_idx = int(np.argmin(nonzero))
    min_clearance_m = float(nonzero[min_idx])
    y_coords, x_coords = np.where(skeleton)
    min_xy = (int(x_coords[min_idx]), int(y_coords[min_idx]))

    return CorridorClearance(
        skeleton_mask=skeleton,
        clearance_m=clearance_m,
        min_clearance_m=min_clearance_m,
        min_clearance_xy=min_xy,
    )


def scan_min_width_along_line(scan_xy: np.ndarray,
                              heading_rad: float,
                              band_m: float = 0.25) -> float:
    """Rough live clearance from a single /scan, along the robot heading.

    Projects the scan onto the axis perpendicular to `heading_rad` and
    returns the absolute minimum |y| on both sides (left max + right
    max) within a forward band. Used by the path-blockage node as the
    instantaneous width for comparison against the SLAM map's baseline.
    """
    if scan_xy.size == 0:
        return float("nan")
    # Rotate so heading aligns with +x
    c = np.cos(-heading_rad)
    s = np.sin(-heading_rad)
    rot = np.column_stack([
        c * scan_xy[:, 0] - s * scan_xy[:, 1],
        s * scan_xy[:, 0] + c * scan_xy[:, 1],
    ])
    in_band = np.abs(rot[:, 0]) < band_m
    if not in_band.any():
        return float("nan")
    y = rot[in_band, 1]
    left = y[y > 0.0]
    right = y[y < 0.0]
    if left.size == 0 or right.size == 0:
        return float("nan")
    return float(np.min(left) - np.max(right))
