"""
LiDAR-based doorway candidate detection.

Given a rolling window of 2D LaserScan returns, this detector fits the
two nearest parallel wall segments on each side of the robot's path and
reports local minima in the corridor width as doorway candidates.

It is the first stage of the ADA 404.2.3 door-clear-width pipeline. The
stereo jamb-plane fitter consumes its output to perform the precise
handle-height measurement.

The detector is designed to run without rclpy so it can be unit-tested
with synthetic LaserScan-like inputs. A thin ROS2 adapter in
`nodes/door_clear_width_node.py` wires it to the live `/scan` topic.

Reference: Rusu 2009/2010 (TUM / PCL origin thesis) for the 2D-laser
corridor-width approach. STAR adapts it to the RPLiDAR C1 (single-plane,
10 Hz, 12 m) with a rolling RANSAC window.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Iterable

import numpy as np


# ADA 2010 Standards 404.2.3 thresholds
ADA_CLEAR_WIDTH_MIN_M = 0.8128   # 32 inches
ADA_DEEP_OPENING_MIN_M = 0.9144  # 36 inches (for openings > 24 in deep)

# Detection policy constants
DOORWAY_CANDIDATE_MAX_M = 1.1    # flag corridor widths below this as candidates
DOORWAY_MIN_SEPARATION_M = 0.5   # min robot travel between consecutive flags
WALL_LINE_RANSAC_ITERATIONS = 80
WALL_LINE_INLIER_THRESHOLD_M = 0.05
WALL_LINE_MIN_INLIERS = 20
FORWARD_SCAN_FAN_DEG = 90.0      # consider returns within +/- 45 deg of forward
FORWARD_SCAN_MAX_RANGE_M = 4.0   # doorways are near; ignore long returns


@dataclass
class WallLine:
    """A 2D line y = m x + b in the robot base_link frame.

    Represented as (a, b, c) with a x + b y + c = 0 for numerical
    stability with vertical walls (the common case for a robot in a
    corridor, where the walls are along the robot's forward axis).
    """
    a: float
    b: float
    c: float
    inlier_count: int

    def signed_distance(self, x: float, y: float) -> float:
        """Signed perpendicular distance from (x, y) to the line."""
        return self.a * x + self.b * y + self.c

    def perpendicular_distance(self, other: "WallLine") -> float:
        """Distance between two (approximately) parallel lines.

        RANSAC can return either (a, b, c) or (-a, -b, -c) for the same
        physical line, so we align the normals via their dot product
        before differencing. Works because each WallLine is unit-normal.
        """
        dot = self.a * other.a + self.b * other.b
        if dot >= 0.0:
            return abs(self.c - other.c)
        return abs(self.c + other.c)


@dataclass
class DoorwayCandidate:
    """One local minimum in the corridor width."""
    centroid_x_m: float
    centroid_y_m: float
    corridor_width_m: float
    left_wall: WallLine
    right_wall: WallLine
    score: float = field(default=0.0)  # narrower => higher score

    def as_dict(self) -> dict:
        return {
            "centroid_x_m": self.centroid_x_m,
            "centroid_y_m": self.centroid_y_m,
            "corridor_width_m": self.corridor_width_m,
            "left_a": self.left_wall.a,
            "left_b": self.left_wall.b,
            "left_c": self.left_wall.c,
            "right_a": self.right_wall.a,
            "right_b": self.right_wall.b,
            "right_c": self.right_wall.c,
            "score": self.score,
        }


def polar_to_xy(ranges: Iterable[float],
                angle_min_rad: float,
                angle_increment_rad: float,
                range_min_m: float,
                range_max_m: float) -> np.ndarray:
    """Convert a LaserScan-like polar sweep into (N, 2) XY.

    Out-of-range and non-finite returns are dropped. The output is in
    the sensor frame (same as the LaserScan's `frame_id`).
    """
    r = np.asarray(ranges, dtype=np.float64)
    n = r.size
    angles = angle_min_rad + angle_increment_rad * np.arange(n, dtype=np.float64)
    valid = np.isfinite(r) & (r >= range_min_m) & (r <= range_max_m)
    r = r[valid]
    a = angles[valid]
    return np.column_stack([r * np.cos(a), r * np.sin(a)])


def _forward_points(xy: np.ndarray,
                    fan_deg: float = FORWARD_SCAN_FAN_DEG,
                    max_range_m: float = FORWARD_SCAN_MAX_RANGE_M) -> np.ndarray:
    """Subset the scan to points ahead of the robot within a fan."""
    if xy.size == 0:
        return xy
    half_rad = math.radians(fan_deg / 2.0)
    r = np.linalg.norm(xy, axis=1)
    bearing = np.arctan2(xy[:, 1], xy[:, 0])
    mask = (np.abs(bearing) <= half_rad) & (r <= max_range_m)
    return xy[mask]


def _fit_line_ransac(points: np.ndarray,
                     iterations: int = WALL_LINE_RANSAC_ITERATIONS,
                     threshold_m: float = WALL_LINE_INLIER_THRESHOLD_M,
                     rng: np.random.Generator | None = None) -> WallLine | None:
    """Fit a 2D line to points via RANSAC; return None if it fails.

    Uses the standard (a, b, c) with a x + b y + c = 0 and
    a^2 + b^2 = 1 so `signed_distance` is geometrically meaningful.
    """
    if points.shape[0] < WALL_LINE_MIN_INLIERS:
        return None
    rng = rng or np.random.default_rng(0)
    best: WallLine | None = None
    n = points.shape[0]

    for _ in range(iterations):
        i, j = rng.choice(n, size=2, replace=False)
        p1 = points[i]
        p2 = points[j]
        dx = p2[0] - p1[0]
        dy = p2[1] - p1[1]
        norm = math.hypot(dx, dy)
        if norm < 1e-6:
            continue
        a = -dy / norm
        b = dx / norm
        c = -(a * p1[0] + b * p1[1])

        distances = np.abs(a * points[:, 0] + b * points[:, 1] + c)
        inliers = int(np.sum(distances < threshold_m))
        if inliers >= WALL_LINE_MIN_INLIERS and (best is None or inliers > best.inlier_count):
            best = WallLine(a=a, b=b, c=c, inlier_count=inliers)
    return best


def _split_left_right(points: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Split forward scan into left (y > 0) and right (y < 0) subsets."""
    left = points[points[:, 1] > 0.0]
    right = points[points[:, 1] < 0.0]
    return left, right


class DoorwayLidarDetector:
    """Streaming LiDAR doorway candidate detector.

    Usage:

        detector = DoorwayLidarDetector()
        for scan in live_scans:
            candidates = detector.feed(scan_xy, robot_pose_xy)
            for c in candidates:
                stereo_pipeline.measure(c)
    """

    def __init__(self,
                 candidate_max_m: float = DOORWAY_CANDIDATE_MAX_M,
                 min_separation_m: float = DOORWAY_MIN_SEPARATION_M,
                 rng_seed: int = 0):
        self.candidate_max_m = candidate_max_m
        self.min_separation_m = min_separation_m
        self._rng = np.random.default_rng(rng_seed)
        self._last_flag_xy: tuple[float, float] | None = None

    def feed(self,
             scan_xy: np.ndarray,
             robot_pose_xy: tuple[float, float] = (0.0, 0.0)) -> list[DoorwayCandidate]:
        """Feed one scan and return any new doorway candidates.

        Parameters
        ----------
        scan_xy : (N, 2) ndarray
            LaserScan returns already in the robot's base_link frame.
        robot_pose_xy : (2,) tuple
            The robot's (x, y) pose in the map frame at this scan time.
            Used only to enforce the min_separation_m policy between
            consecutive flags.
        """
        forward = _forward_points(scan_xy)
        if forward.shape[0] < 2 * WALL_LINE_MIN_INLIERS:
            return []

        left, right = _split_left_right(forward)
        left_line = _fit_line_ransac(left, rng=self._rng)
        right_line = _fit_line_ransac(right, rng=self._rng)
        if left_line is None or right_line is None:
            return []

        width = left_line.perpendicular_distance(right_line)
        if width >= self.candidate_max_m:
            return []

        # Enforce min separation between flags
        if self._last_flag_xy is not None:
            dx = robot_pose_xy[0] - self._last_flag_xy[0]
            dy = robot_pose_xy[1] - self._last_flag_xy[1]
            if math.hypot(dx, dy) < self.min_separation_m:
                return []

        # Approximate doorway centroid as the midpoint of left and right
        # wall intercepts at the robot's forward axis (x=0).
        # A point on a line (a, b, c) closest to origin is (-a c, -b c).
        left_near = (-left_line.a * left_line.c, -left_line.b * left_line.c)
        right_near = (-right_line.a * right_line.c, -right_line.b * right_line.c)
        cx = 0.5 * (left_near[0] + right_near[0])
        cy = 0.5 * (left_near[1] + right_near[1])

        score = self.candidate_max_m - width  # narrower => higher
        candidate = DoorwayCandidate(
            centroid_x_m=cx,
            centroid_y_m=cy,
            corridor_width_m=width,
            left_wall=left_line,
            right_wall=right_line,
            score=score,
        )

        self._last_flag_xy = tuple(robot_pose_xy)
        return [candidate]
