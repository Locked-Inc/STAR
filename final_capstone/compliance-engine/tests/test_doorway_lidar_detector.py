"""Unit tests for the LiDAR doorway candidate detector.

Uses synthetic LaserScan-like polar sweeps - no ROS2, no hardware. All
tests should run under a few hundred milliseconds.
"""

from __future__ import annotations

import math

import numpy as np
import pytest

from star_compliance.detectors.doorway_lidar_detector import (
    DoorwayLidarDetector,
    polar_to_xy,
    ADA_CLEAR_WIDTH_MIN_M,
)


def synthetic_corridor_scan(corridor_width_m: float,
                            corridor_length_m: float = 6.0,
                            num_points: int = 360,
                            noise_std_m: float = 0.01,
                            rng: np.random.Generator | None = None) -> np.ndarray:
    """Build a synthetic 2D LaserScan return array for a straight corridor.

    The robot sits at (0, 0) facing +x. Two parallel walls at
    y = +corridor_width_m/2 and y = -corridor_width_m/2, both running
    from x = 0 to x = corridor_length_m.
    """
    rng = rng or np.random.default_rng(0)
    half = corridor_width_m / 2.0

    # Build walls as dense point strips
    x_points = np.linspace(0.1, corridor_length_m, num_points // 2)
    top_wall = np.column_stack([x_points, np.full_like(x_points, +half)])
    bottom_wall = np.column_stack([x_points, np.full_like(x_points, -half)])
    all_pts = np.vstack([top_wall, bottom_wall])

    # Add gaussian range noise along each point's range vector
    r = np.linalg.norm(all_pts, axis=1)
    theta = np.arctan2(all_pts[:, 1], all_pts[:, 0])
    r_noisy = r + rng.normal(0.0, noise_std_m, size=r.size)
    return np.column_stack([r_noisy * np.cos(theta), r_noisy * np.sin(theta)])


def test_polar_to_xy_drops_invalid():
    ranges = [1.0, float("inf"), -1.0, float("nan"), 2.0]
    xy = polar_to_xy(ranges,
                     angle_min_rad=0.0,
                     angle_increment_rad=math.radians(10),
                     range_min_m=0.1, range_max_m=10.0)
    assert xy.shape == (2, 2)


@pytest.mark.parametrize("width,expect_candidate", [
    (0.80, True),    # 31.5 in - violation candidate
    (0.85, True),    # 33.5 in - still under 1.1 m threshold
    (1.05, True),    # 41.3 in - still under 1.1 m threshold
    (1.30, False),   # 51.2 in - too wide, corridor not a doorway
])
def test_detects_narrow_corridors_as_candidates(width, expect_candidate):
    scan = synthetic_corridor_scan(width, noise_std_m=0.01)
    detector = DoorwayLidarDetector()
    candidates = detector.feed(scan, robot_pose_xy=(0.0, 0.0))
    if expect_candidate:
        assert len(candidates) == 1
        assert abs(candidates[0].corridor_width_m - width) < 0.05
    else:
        assert candidates == []


def test_corridor_width_recovery_accuracy():
    """For a known 0.95 m corridor, the detector should recover width
    to within 3 cm - the RPLiDAR C1 noise floor.
    """
    scan = synthetic_corridor_scan(0.95, noise_std_m=0.015)
    detector = DoorwayLidarDetector()
    candidates = detector.feed(scan)
    assert len(candidates) == 1
    assert abs(candidates[0].corridor_width_m - 0.95) < 0.03


def test_min_separation_policy_prevents_duplicate_flags():
    scan = synthetic_corridor_scan(0.85, noise_std_m=0.01)
    detector = DoorwayLidarDetector(min_separation_m=0.5)

    # First flag at origin
    first = detector.feed(scan, robot_pose_xy=(0.0, 0.0))
    assert len(first) == 1

    # Robot moved 0.2 m - still inside the separation cone, no new flag
    second = detector.feed(scan, robot_pose_xy=(0.2, 0.0))
    assert second == []

    # Robot moved 0.6 m - outside the separation cone, new flag
    third = detector.feed(scan, robot_pose_xy=(0.6, 0.0))
    assert len(third) == 1


def test_empty_scan_returns_no_candidates():
    detector = DoorwayLidarDetector()
    assert detector.feed(np.empty((0, 2))) == []


def test_scan_below_inlier_threshold_returns_empty():
    detector = DoorwayLidarDetector()
    tiny = np.column_stack([np.linspace(0.5, 2.0, 10),
                            np.full(10, 0.4)])
    assert detector.feed(tiny) == []


def test_ada_32_inch_threshold_constant():
    """The detector's constant matches the ADA 2010 404.2.3 number."""
    # 32 inches = 0.8128 m exactly
    assert abs(ADA_CLEAR_WIDTH_MIN_M - 0.8128) < 1e-6
