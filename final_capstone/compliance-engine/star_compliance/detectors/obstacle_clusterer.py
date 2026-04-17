"""
DBSCAN dynamic-obstacle clusterer for /scan background subtraction.

Projects LiDAR scan points into the map frame, subtracts any that
coincide with occupied cells in the slam_toolbox map, and clusters
the remaining (dynamic) points via sklearn DBSCAN.

Feeds the path-blockage compliance node and (eventually) a Nav2
behavior-tree pedestrian-pause action.

Pure numpy + sklearn; no ROS2 dependency. The caller maps LaserScan
+ OccupancyGrid into this module's inputs.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


DEFAULT_EPS_M = 0.15
DEFAULT_MIN_SAMPLES = 5
DEFAULT_MAP_MATCH_TOLERANCE_M = 0.10


@dataclass
class ObstacleCluster:
    """Summary of one DBSCAN cluster."""
    centroid_xy: tuple[float, float]
    radius_m: float
    point_count: int
    confidence: float   # normalized density, 0-1


def subtract_known_map(points_map_frame: np.ndarray,
                       occupancy: np.ndarray,
                       origin_xy: tuple[float, float],
                       resolution_m: float,
                       tolerance_m: float = DEFAULT_MAP_MATCH_TOLERANCE_M
                       ) -> np.ndarray:
    """Drop scan points that match an occupied cell in the map.

    Parameters
    ----------
    points_map_frame : (N, 2) ndarray
        Scan points already transformed into the map frame.
    occupancy : (H, W) ndarray
        OccupancyGrid data (-1 unknown, 0 free, >=50 occupied).
    origin_xy : (2,) tuple
        Map origin (bottom-left cell center) in world coordinates.
    resolution_m : float
        Map resolution (meters per cell).
    tolerance_m : float
        How close a scan point must be to an occupied cell's center
        to count as a match. Defaults to 10 cm, matching slam_toolbox
        noise.
    """
    if points_map_frame.size == 0:
        return points_map_frame
    tol_cells = max(1, int(np.ceil(tolerance_m / resolution_m)))

    # Discretize scan points to cell indices
    col = ((points_map_frame[:, 0] - origin_xy[0]) / resolution_m).astype(np.int64)
    row = ((points_map_frame[:, 1] - origin_xy[1]) / resolution_m).astype(np.int64)
    h, w = occupancy.shape
    valid = (col >= 0) & (col < w) & (row >= 0) & (row < h)

    keep = np.ones(points_map_frame.shape[0], dtype=bool)
    for idx, (r, c) in enumerate(zip(row, col)):
        if not valid[idx]:
            continue
        # Check a small neighborhood around (r, c)
        r0, r1 = max(0, r - tol_cells), min(h, r + tol_cells + 1)
        c0, c1 = max(0, c - tol_cells), min(w, c + tol_cells + 1)
        if np.any(occupancy[r0:r1, c0:c1] >= 50):
            keep[idx] = False
    return points_map_frame[keep]


def cluster_points(points_map_frame: np.ndarray,
                   eps_m: float = DEFAULT_EPS_M,
                   min_samples: int = DEFAULT_MIN_SAMPLES
                   ) -> list[ObstacleCluster]:
    """Cluster the given XY scan points via DBSCAN.

    Returns ObstacleCluster instances in descending size order. An
    empty input returns an empty list immediately.
    """
    if points_map_frame.shape[0] < min_samples:
        return []

    try:
        from sklearn.cluster import DBSCAN
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError(
            "scikit-learn is required for the obstacle clusterer."
        ) from exc

    labels = DBSCAN(eps=eps_m, min_samples=min_samples).fit_predict(points_map_frame)
    clusters: list[ObstacleCluster] = []
    for label in sorted(set(labels.tolist())):
        if label < 0:
            continue  # noise
        mask = labels == label
        pts = points_map_frame[mask]
        centroid = pts.mean(axis=0)
        radius = float(np.max(np.linalg.norm(pts - centroid, axis=1)))
        n_points = int(mask.sum())
        density = n_points / (1.0 + np.pi * radius * radius)
        confidence = float(min(1.0, density / 200.0))
        clusters.append(ObstacleCluster(
            centroid_xy=(float(centroid[0]), float(centroid[1])),
            radius_m=radius,
            point_count=n_points,
            confidence=confidence,
        ))
    clusters.sort(key=lambda c: c.point_count, reverse=True)
    return clusters
