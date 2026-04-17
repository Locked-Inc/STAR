"""
Floor-plane tracking for the STAR compliance engine.

Maintains a running estimate of the floor plane in the robot's map
frame and exposes a simple API for querying "how high is this point
above the floor?" - needed by the jamb-plane fitter (handle-height
band) and the ADA 307 protruding-objects check (27-80 in cane zone).

Inputs:

- The BNO055 orientation quaternion (robot pitch / roll) from the
  chassis IMU.
- Optionally, LiDAR returns immediately in front of the robot for a
  direct floor RANSAC.

Output:

- `FloorEstimate` with the plane normal (should be close to +z in map
  frame), the plane offset d in map frame, and an age in seconds.

Pure numpy; no Open3D or ROS2 dependency so it unit-tests cleanly.
"""

from __future__ import annotations

import math
from dataclasses import dataclass


# Cos threshold for "the floor normal is vertical enough to trust." A
# misaligned BNO055 or a robot on a steep ramp breaks this, which is
# intended - we prefer to fail loudly rather than silently misreport
# heights above the floor.
VERTICAL_NORMAL_Z_MIN = 0.95   # about cos(18 deg)


@dataclass
class FloorEstimate:
    """Floor plane in the map frame.

    Plane equation: nx x + ny y + nz z + d = 0, with (nx, ny, nz) unit.
    For a level robot in a level building, (nx, ny, nz) ~= (0, 0, 1)
    and d ~= -h_robot_above_floor.
    """
    nx: float
    ny: float
    nz: float
    d: float
    age_sec: float

    def height_above_floor(self, x: float, y: float, z: float) -> float:
        """Perpendicular distance from (x, y, z) to the floor plane.

        Returns a signed value; positive means the point is above the
        floor (same side as the normal), negative means below.
        """
        return self.nx * x + self.ny * y + self.nz * z + self.d

    def is_vertical_enough(self) -> bool:
        return abs(self.nz) >= VERTICAL_NORMAL_Z_MIN


def floor_from_bno055_quaternion(qx: float, qy: float, qz: float, qw: float,
                                 sensor_height_m: float,
                                 now_sec: float) -> FloorEstimate:
    """Derive a floor plane from the chassis BNO055 orientation.

    Assumes the robot base_link is oriented so its +Z points up when
    the floor is level. The BNO055 reports an absolute orientation
    quaternion; we take its inverse rotation of the world +Z vector to
    find how +Z projects into the current base_link frame. On a level
    floor this collapses to (0, 0, 1); on a ramp the vector tilts.

    The floor is then the plane passing through (0, 0, -sensor_height_m)
    with that normal.
    """
    # Rotate map-frame +Z (0, 0, 1) into the current base_link frame.
    # For unit quaternion q = (qx, qy, qz, qw), the rotation of v is:
    #   v' = v + 2*qw*(qvec x v) + 2*(qvec x (qvec x v))
    # We need the inverse rotation, which is the conjugate: (-qx,-qy,-qz,qw)
    cx, cy, cz = -qx, -qy, -qz
    vx, vy, vz = 0.0, 0.0, 1.0

    # t = 2 * (cross(cvec, v))
    tx = 2.0 * (cy * vz - cz * vy)
    ty = 2.0 * (cz * vx - cx * vz)
    tz = 2.0 * (cx * vy - cy * vx)

    # v' = v + qw*t + cross(cvec, t)
    nx = vx + qw * tx + (cy * tz - cz * ty)
    ny = vy + qw * ty + (cz * tx - cx * tz)
    nz = vz + qw * tz + (cx * ty - cy * tx)

    norm = math.sqrt(nx * nx + ny * ny + nz * nz)
    if norm < 1e-9:
        raise ValueError("degenerate quaternion produced a zero normal")
    nx /= norm
    ny /= norm
    nz /= norm

    # Plane through (0, 0, -sensor_height_m) with normal (nx, ny, nz):
    # nx*0 + ny*0 + nz*(-sensor_height_m) + d = 0  =>  d = nz*sensor_height_m
    d = nz * sensor_height_m
    return FloorEstimate(nx=nx, ny=ny, nz=nz, d=d, age_sec=now_sec)


def floor_from_lidar_points(points, sensor_height_m: float,
                            now_sec: float,
                            distance_threshold_m: float = 0.02,
                            iterations: int = 200):
    """Fit a floor plane via RANSAC on LiDAR returns near the floor.

    `points` is an (N, 3) ndarray. Only points with Z near the expected
    floor height (within 1.5x sensor_height_m, to catch floor points
    even when the robot is slightly tilted) are considered.

    Uses the shared RANSAC helper from `engines.plane_segmentation` to
    avoid duplicating logic. Open3D is required at runtime; if absent,
    falls back to `floor_from_bno055_quaternion` via the caller.
    """
    import numpy as np

    if points.size == 0:
        return None
    # Prefilter to points reasonably close to the sensor-height floor
    # estimate. +0.25 m buffer tolerates ramps and small tilts.
    mask = np.abs(points[:, 2] + sensor_height_m) < 0.25 + 1.5 * abs(sensor_height_m)
    subset = points[mask]
    if subset.shape[0] < 50:
        return None

    from star_compliance.engines.plane_segmentation import fit_plane_ransac
    fit = fit_plane_ransac(subset,
                           distance_threshold=distance_threshold_m,
                           num_iterations=iterations)
    nx, ny, nz = fit.normal.tolist()
    return FloorEstimate(nx=nx, ny=ny, nz=nz, d=fit.d, age_sec=now_sec)
