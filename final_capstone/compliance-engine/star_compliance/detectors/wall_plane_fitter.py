"""
Wall-plane fitter for ADA 307 Protruding Objects.

Given a 3D point cloud of an interior room, iteratively RANSAC-fits
vertical planes and returns each detected wall as a Plane parameter
set. Uses Open3D at runtime; skips gracefully in tests via the
availability guard pattern used elsewhere in the compliance engine.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Iterable

import numpy as np


DEFAULT_DISTANCE_THRESHOLD_M = 0.02
DEFAULT_MIN_INLIERS = 200
DEFAULT_MAX_PLANES = 8
# Cos(80 deg) ~ 0.174 - reject planes whose normal's Z component is
# larger than this (i.e., reject floor and ceiling).
MAX_NORMAL_Z_ABS = 0.174


@dataclass
class WallPlane:
    """Parametric wall plane in the input point cloud's frame."""
    nx: float
    ny: float
    nz: float
    d: float
    inlier_count: int

    def signed_distance(self, points: np.ndarray) -> np.ndarray:
        """Signed distance from each row of `points` to the plane."""
        return points @ np.array([self.nx, self.ny, self.nz]) + self.d

    def perpendicular_distance_abs(self, points: np.ndarray) -> np.ndarray:
        return np.abs(self.signed_distance(points))


def fit_walls(points: np.ndarray,
              max_planes: int = DEFAULT_MAX_PLANES,
              distance_threshold_m: float = DEFAULT_DISTANCE_THRESHOLD_M,
              min_inliers: int = DEFAULT_MIN_INLIERS,
              ransac_iterations: int = 400) -> list[WallPlane]:
    """Detect up to `max_planes` vertical wall planes in the cloud.

    Algorithm:
      1. Copy the input points.
      2. RANSAC-fit one plane, keep it if its normal is nearly
         horizontal (|nz| < MAX_NORMAL_Z_ABS) and its inlier count
         exceeds `min_inliers`.
      3. Remove its inliers and repeat.
      4. Stop when no more planes meet the criteria or `max_planes`
         reached.

    Returns the list of WallPlane in detection order. Empty list if
    Open3D is unavailable at runtime or no wall-like planes are found.
    """
    try:
        import open3d as o3d
    except ImportError:  # pragma: no cover
        return []

    if points.shape[0] < min_inliers:
        return []

    remaining = points.astype(np.float64, copy=True)
    walls: list[WallPlane] = []

    for _ in range(max_planes):
        if remaining.shape[0] < min_inliers:
            break
        pcd = o3d.geometry.PointCloud()
        pcd.points = o3d.utility.Vector3dVector(remaining)
        plane_model, inliers = pcd.segment_plane(
            distance_threshold=distance_threshold_m,
            ransac_n=3,
            num_iterations=ransac_iterations,
        )
        if len(inliers) < min_inliers:
            break

        a, b, c, d = plane_model
        n = np.array([a, b, c], dtype=np.float64)
        norm = float(np.linalg.norm(n))
        if norm < 1e-9:
            break
        n /= norm
        d_norm = float(d) / norm

        # Remove inliers whether or not the plane counts as a wall,
        # so the next iteration doesn't refit the same plane.
        keep_mask = np.ones(remaining.shape[0], dtype=bool)
        keep_mask[inliers] = False
        next_remaining = remaining[keep_mask]

        if abs(n[2]) < MAX_NORMAL_Z_ABS:
            walls.append(WallPlane(
                nx=float(n[0]),
                ny=float(n[1]),
                nz=float(n[2]),
                d=d_norm,
                inlier_count=int(len(inliers)),
            ))
        remaining = next_remaining

    return walls


def nearest_wall(point: np.ndarray,
                 walls: Iterable[WallPlane]) -> tuple[WallPlane | None, float]:
    """Return (closest wall, signed distance) for a single 3D point."""
    best: tuple[WallPlane | None, float] = (None, float("inf"))
    for wall in walls:
        dist = float(wall.perpendicular_distance_abs(point[None, :])[0])
        if dist < abs(best[1]):
            best = (wall, dist)
    return best
