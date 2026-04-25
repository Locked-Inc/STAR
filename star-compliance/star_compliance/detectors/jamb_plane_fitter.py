"""
Stereo-based jamb plane fitting for ADA door clear-width measurement.

Given a 3D point cloud (typically cropped to a LiDAR-detected doorway
bounding box) and a known floor plane, this fitter:

1. Extracts the handle-height band (85-100 cm above the floor, ADA's
   typical door-handle height).
2. RANSAC-fits two parallel vertical planes (the left and right jambs).
3. Reports the perpendicular distance between them at handle height.

Open3D is imported lazily so this module can be loaded without it;
callers get a clear RuntimeError at the first fit call on a machine
where the library is missing. Tests that exercise the fit path skip
gracefully when Open3D is unavailable.

Reference: Quintana et al. 2018 (Automation in Construction) for 3D
door-plane segmentation; Arduengo et al. 2021 (Intelligent Service
Robotics, arXiv:1902.09051) for the RANSAC-then-geometry recipe in
mobile-robot door detection.
"""

from __future__ import annotations

import math
from dataclasses import dataclass

import numpy as np


# ADA 2010 Standards 404.2.3 thresholds
ADA_CLEAR_WIDTH_MIN_M = 0.8128   # 32 inches

# Door-handle detection band above the floor plane. AOSI / ADA 404.2.7
# permits 34-48 in mounting height; we measure width in the narrower
# 33-40 in band to stay comfortably below typical handle locations and
# above waist-hinge hardware.
HANDLE_HEIGHT_MIN_M = 0.85
HANDLE_HEIGHT_MAX_M = 1.00

# RANSAC plane-fit defaults
PLANE_DISTANCE_THRESHOLD_M = 0.02
PLANE_RANSAC_ITERATIONS = 400
PLANE_MIN_INLIERS = 50

# How close a plane's normal must be to horizontal (perpendicular to
# the gravity axis) to count as a vertical wall / jamb. cos(80 deg) =
# 0.174 ~ 10 deg tolerance about the horizontal plane.
VERTICAL_NORMAL_Z_TOLERANCE = 0.174


@dataclass
class VerticalPlane:
    """A wall plane expected to be vertical in the map frame.

    n . p + d = 0, with n = (nx, ny, nz) a unit normal whose z-component
    is close to zero (horizontal normal => vertical wall).
    """
    nx: float
    ny: float
    nz: float
    d: float
    inlier_count: int

    def as_array(self) -> np.ndarray:
        return np.array([self.nx, self.ny, self.nz, self.d])

    def signed_distance(self, p: np.ndarray) -> np.ndarray:
        """Signed perpendicular distance from each row of `p` to the plane."""
        return p @ np.array([self.nx, self.ny, self.nz]) + self.d

    def perpendicular_distance(self, other: "VerticalPlane") -> float:
        """Distance between two (approximately) parallel vertical planes.

        RANSAC can return opposite signs of the same physical plane;
        align normals via their dot product before differencing.
        """
        dot = self.nx * other.nx + self.ny * other.ny + self.nz * other.nz
        if dot >= 0.0:
            return abs(self.d - other.d)
        return abs(self.d + other.d)


@dataclass
class JambMeasurement:
    """Result of fitting the two jambs and measuring between them."""
    frame_width_m: float
    left_jamb: VerticalPlane
    right_jamb: VerticalPlane
    points_used: int
    floor_height_m: float


def extract_handle_height_band(points: np.ndarray,
                               floor_height_m: float,
                               band_min_m: float = HANDLE_HEIGHT_MIN_M,
                               band_max_m: float = HANDLE_HEIGHT_MAX_M) -> np.ndarray:
    """Select points whose Z is within the handle-height band above the floor.

    Points are expected in the same frame as the floor. `floor_height_m`
    is the map-frame Z of the floor plane - subtracted here so the
    returned band is absolute Z in that frame.
    """
    if points.size == 0:
        return points
    z = points[:, 2]
    mask = (z >= floor_height_m + band_min_m) & (z <= floor_height_m + band_max_m)
    return points[mask]


def _fit_one_vertical_plane(points: np.ndarray,
                            used_mask: np.ndarray | None = None
                            ) -> VerticalPlane | None:
    """Fit one vertical plane via Open3D RANSAC.

    Returns None if Open3D reports insufficient support or the plane's
    normal is not horizontal enough.
    """
    if points.shape[0] < PLANE_MIN_INLIERS:
        return None
    try:
        import open3d as o3d
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError(
            "open3d is required for jamb plane fitting. "
            "pip install open3d on the Pi 5 or development machine."
        ) from exc

    if used_mask is None:
        subset = points
    else:
        subset = points[~used_mask]
    if subset.shape[0] < PLANE_MIN_INLIERS:
        return None

    pcd = o3d.geometry.PointCloud()
    pcd.points = o3d.utility.Vector3dVector(subset.astype(np.float64))
    plane_model, inliers = pcd.segment_plane(
        distance_threshold=PLANE_DISTANCE_THRESHOLD_M,
        ransac_n=3,
        num_iterations=PLANE_RANSAC_ITERATIONS,
    )
    if len(inliers) < PLANE_MIN_INLIERS:
        return None

    a, b, c, d = plane_model
    n = np.array([a, b, c], dtype=np.float64)
    norm = float(np.linalg.norm(n))
    if norm < 1e-9:
        return None
    n /= norm
    d_norm = float(d) / norm

    if abs(n[2]) > VERTICAL_NORMAL_Z_TOLERANCE:
        # Not vertical enough; probably fit the floor or a ramp.
        return None

    return VerticalPlane(
        nx=float(n[0]),
        ny=float(n[1]),
        nz=float(n[2]),
        d=d_norm,
        inlier_count=len(inliers),
    )


def fit_jambs(cloud: np.ndarray,
              floor_height_m: float,
              band_min_m: float = HANDLE_HEIGHT_MIN_M,
              band_max_m: float = HANDLE_HEIGHT_MAX_M
              ) -> JambMeasurement | None:
    """Fit two parallel vertical jamb planes and measure between them.

    Parameters
    ----------
    cloud : (N, 3) ndarray
        Point cloud cropped to the LiDAR-detected doorway bounding box,
        in a frame whose +Z is up (map or base_link).
    floor_height_m : float
        Floor plane Z in the same frame.

    Returns
    -------
    JambMeasurement or None if the fit fails.
    """
    band = extract_handle_height_band(cloud, floor_height_m, band_min_m, band_max_m)
    if band.shape[0] < 2 * PLANE_MIN_INLIERS:
        return None

    # Fit the first jamb on the full band.
    first = _fit_one_vertical_plane(band)
    if first is None:
        return None

    # Remove its inliers and fit the second jamb on what remains.
    distances = np.abs(band @ np.array([first.nx, first.ny, first.nz]) + first.d)
    first_inliers_mask = distances < PLANE_DISTANCE_THRESHOLD_M
    remaining = band[~first_inliers_mask]
    second = _fit_one_vertical_plane(remaining)
    if second is None:
        return None

    # Guard: the two planes must be roughly parallel. Compare normals.
    dot = first.nx * second.nx + first.ny * second.ny + first.nz * second.nz
    if abs(dot) < 0.9:  # cos(25 deg) ~ 0.9
        return None

    # Order left / right by the sign of d (assumes the robot's X axis
    # goes forward through the doorway). The caller can re-order if the
    # cloud is in an unusual frame.
    if first.d <= second.d:
        left, right = first, second
    else:
        left, right = second, first

    width = left.perpendicular_distance(right)
    return JambMeasurement(
        frame_width_m=width,
        left_jamb=left,
        right_jamb=right,
        points_used=band.shape[0],
        floor_height_m=floor_height_m,
    )
