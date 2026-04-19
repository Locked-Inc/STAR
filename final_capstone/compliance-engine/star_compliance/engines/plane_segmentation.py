"""Shared plane-segmentation utilities for ramp-related ADA checks.

Uses Open3D RANSAC. Callable without ROS so it can be unit-tested on
saved point clouds.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np


@dataclass
class PlaneFit:
    """Result of a single RANSAC plane fit.

    Attributes
    ----------
    normal : np.ndarray shape (3,)
        Unit surface normal in the frame the input points were in.
    d : float
        Plane equation offset so that n . p + d = 0 for any p on the plane.
    inlier_count : int
        Number of points accepted as inliers by RANSAC.
    slope_deg : float
        Angle between the plane's normal and the z-axis (gravity), in
        degrees. Zero for a horizontal floor; up to ~90 for a vertical
        wall.
    """

    normal: np.ndarray
    d: float
    inlier_count: int
    slope_deg: float


def fit_plane_ransac(points: np.ndarray,
                     distance_threshold: float = 0.02,
                     ransac_n: int = 3,
                     num_iterations: int = 500) -> PlaneFit:
    """Fit a plane to `points` (Nx3) using Open3D RANSAC.

    Parameters are passed straight through to Open3D. The function does
    not import Open3D at module import time so this file can still be
    imported in an environment where Open3D is unavailable (e.g., lint
    CI on a headless image). The call raises an informative error if
    Open3D is missing at runtime.
    """
    try:
        import open3d as o3d
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError(
            "open3d is required for fit_plane_ransac. "
            "Add open3d to the compliance-engine environment."
        ) from exc

    if points.shape[0] < ransac_n:
        raise ValueError(f"need at least {ransac_n} points, got {points.shape[0]}")

    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(points.astype(np.float64))

    plane, inliers = pcd.segment_plane(
        distance_threshold=distance_threshold,
        ransac_n=ransac_n,
        num_iterations=num_iterations,
    )
    a, b, c, d = plane
    normal = np.array([a, b, c], dtype=np.float64)
    n_norm = float(np.linalg.norm(normal))
    if n_norm == 0.0:
        raise RuntimeError("degenerate plane fit returned zero normal")
    normal /= n_norm

    # slope is angle between normal and gravity axis (+z). If normal
    # points "down" (negative z), flip it so slope is always in [0, 90].
    if normal[2] < 0:
        normal = -normal
        d = -d
    slope_rad = math.acos(max(-1.0, min(1.0, normal[2])))
    slope_deg = math.degrees(slope_rad)

    return PlaneFit(
        normal=normal,
        d=float(d),
        inlier_count=len(inliers),
        slope_deg=slope_deg,
    )
