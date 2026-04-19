"""
Cane-detectable zone filter for ADA 307 Protruding Objects.

ADA 2010 Standards section 307 defines the "cane-detectable zone" as
heights between 27 inches (686 mm) and 80 inches (2032 mm) above the
walking surface. Objects protruding more than 4 inches (102 mm) into
circulation paths within this zone are ADA violations because a person
using a long cane cannot detect them before contact.

This filter takes a 3D point cloud and returns only the points whose
Z value (after subtracting the floor plane) is inside the cane zone.
Pure numpy - no Open3D dependency - so it unit-tests cleanly.
"""

from __future__ import annotations

import numpy as np


INCH_TO_M = 0.0254
CANE_ZONE_MIN_M = 27.0 * INCH_TO_M    # 0.6858
CANE_ZONE_MAX_M = 80.0 * INCH_TO_M    # 2.032
ADA_307_PROTRUSION_LIMIT_M = 4.0 * INCH_TO_M   # 0.1016


def filter_to_cane_zone(points: np.ndarray,
                        floor_height_m: float,
                        zone_min_m: float = CANE_ZONE_MIN_M,
                        zone_max_m: float = CANE_ZONE_MAX_M) -> np.ndarray:
    """Return the subset of `points` whose Z is in the cane zone.

    Parameters
    ----------
    points : (N, 3) ndarray
        Point cloud in a frame where +Z is up.
    floor_height_m : float
        Floor plane Z value in the same frame; ADA cane-zone heights
        are measured relative to this.
    """
    if points.size == 0:
        return points
    z_above = points[:, 2] - floor_height_m
    mask = (z_above >= zone_min_m) & (z_above <= zone_max_m)
    return points[mask]


def is_protrusion(distance_from_wall_m: float,
                  height_above_floor_m: float,
                  point_count: int,
                  min_points: int = 5,
                  threshold_m: float = ADA_307_PROTRUSION_LIMIT_M) -> bool:
    """Classify a cluster as an ADA 307 protrusion.

    True when the cluster (1) sticks more than `threshold_m` out from
    the wall plane, (2) sits in the cane-detectable zone, and (3) has
    enough points to rule out sensor noise.
    """
    if point_count < min_points:
        return False
    if distance_from_wall_m <= threshold_m:
        return False
    if not (CANE_ZONE_MIN_M <= height_above_floor_m <= CANE_ZONE_MAX_M):
        return False
    return True
