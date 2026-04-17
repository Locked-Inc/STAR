"""Unit tests for the stereo jamb plane fitter.

Uses synthetic 3D point clouds - no ROS2, no hardware. Skips the
RANSAC-exercising tests when Open3D is unavailable (e.g. CI machines
without a GPU).
"""

from __future__ import annotations

import math

import numpy as np
import pytest

from star_compliance.detectors.jamb_plane_fitter import (
    VerticalPlane,
    JambMeasurement,
    fit_jambs,
    extract_handle_height_band,
    HANDLE_HEIGHT_MIN_M,
    HANDLE_HEIGHT_MAX_M,
    ADA_CLEAR_WIDTH_MIN_M,
)

try:
    import open3d  # noqa: F401
    HAS_OPEN3D = True
except ImportError:
    HAS_OPEN3D = False


def synthetic_doorway_cloud(frame_width_m: float,
                            floor_height_m: float = 0.0,
                            wall_height_m: float = 2.1,
                            wall_depth_m: float = 0.3,
                            noise_std_m: float = 0.005,
                            density: int = 40,
                            rng: np.random.Generator | None = None) -> np.ndarray:
    """Synthesize a dense point cloud of two parallel vertical jambs.

    Points lie on two planes at y = +/- frame_width_m/2 spanning a
    rectangle in (x, z) from (0, floor) to (wall_depth_m, wall_height_m).
    Gaussian noise is added along the y-axis (perpendicular to the
    walls) to simulate stereo depth uncertainty.
    """
    rng = rng or np.random.default_rng(1)
    half = frame_width_m / 2.0

    xs = np.linspace(0.0, wall_depth_m, density)
    zs = np.linspace(floor_height_m + 0.05,
                     floor_height_m + wall_height_m,
                     density)
    X, Z = np.meshgrid(xs, zs, indexing="xy")
    X = X.ravel()
    Z = Z.ravel()

    # Top / right jamb at y = +half
    top = np.column_stack([X, np.full_like(X, +half), Z])
    # Bottom / left jamb at y = -half
    bot = np.column_stack([X, np.full_like(X, -half), Z])

    cloud = np.vstack([top, bot])
    cloud[:, 1] += rng.normal(0.0, noise_std_m, size=cloud.shape[0])
    return cloud


def test_extract_handle_height_band_keeps_correct_zs():
    points = np.array([
        [0.0, 0.0, 0.5],    # below band
        [0.0, 0.0, 0.85],   # on band edge
        [0.0, 0.0, 0.95],   # in band
        [0.0, 0.0, 1.00],   # on band edge
        [0.0, 0.0, 1.30],   # above band
    ])
    band = extract_handle_height_band(points, floor_height_m=0.0)
    zs = sorted(band[:, 2].tolist())
    assert zs == [0.85, 0.95, 1.00]


def test_extract_handle_height_band_honors_floor_offset():
    points = np.array([
        [0.0, 0.0, 1.0],    # 1 m above a 0.15 m floor => 0.85 above floor (band edge)
        [0.0, 0.0, 1.10],   # 0.95 above floor - in band
        [0.0, 0.0, 1.20],   # 1.05 above floor - ABOVE the 1.00 max band edge
        [0.0, 0.0, 2.00],   # 1.85 above floor - well above band
    ])
    band = extract_handle_height_band(points, floor_height_m=0.15)
    zs = sorted(band[:, 2].tolist())
    assert zs == [1.0, 1.10]


def test_vertical_plane_perpendicular_distance_aligned():
    # Two planes y = +0.5 and y = -0.5, both with normal +y
    a = VerticalPlane(nx=0, ny=1, nz=0, d=-0.5, inlier_count=100)
    b = VerticalPlane(nx=0, ny=1, nz=0, d=+0.5, inlier_count=100)
    assert abs(a.perpendicular_distance(b) - 1.0) < 1e-9


def test_vertical_plane_perpendicular_distance_flipped_normals():
    # Same two planes, but RANSAC returned the second with flipped sign
    a = VerticalPlane(nx=0, ny=1, nz=0, d=-0.5, inlier_count=100)
    b = VerticalPlane(nx=0, ny=-1, nz=0, d=-0.5, inlier_count=100)
    assert abs(a.perpendicular_distance(b) - 1.0) < 1e-9


@pytest.mark.skipif(not HAS_OPEN3D, reason="open3d not installed")
@pytest.mark.parametrize("width_m", [0.80, 0.85, 0.95, 1.05])
def test_fit_jambs_recovers_frame_width(width_m):
    cloud = synthetic_doorway_cloud(width_m, noise_std_m=0.005)
    result = fit_jambs(cloud, floor_height_m=0.0)
    assert result is not None
    assert abs(result.frame_width_m - width_m) < 0.02


@pytest.mark.skipif(not HAS_OPEN3D, reason="open3d not installed")
def test_fit_jambs_returns_none_on_empty_cloud():
    result = fit_jambs(np.empty((0, 3)), floor_height_m=0.0)
    assert result is None


@pytest.mark.skipif(not HAS_OPEN3D, reason="open3d not installed")
def test_fit_jambs_returns_none_when_only_one_wall():
    # Create a cloud with only the right jamb
    rng = np.random.default_rng(0)
    xs = np.linspace(0.0, 0.3, 40)
    zs = np.linspace(0.05, 2.0, 40)
    X, Z = np.meshgrid(xs, zs, indexing="xy")
    wall = np.column_stack([X.ravel(),
                            np.full(X.size, 0.5),
                            Z.ravel()])
    wall[:, 1] += rng.normal(0.0, 0.005, size=wall.shape[0])
    result = fit_jambs(wall, floor_height_m=0.0)
    assert result is None


def test_ada_32_inch_threshold_constant():
    assert abs(ADA_CLEAR_WIDTH_MIN_M - 0.8128) < 1e-6


def test_handle_height_band_constants():
    # 85-100 cm range covers most ADA handle mounting heights.
    assert HANDLE_HEIGHT_MIN_M == 0.85
    assert HANDLE_HEIGHT_MAX_M == 1.00
    assert HANDLE_HEIGHT_MAX_M > HANDLE_HEIGHT_MIN_M
