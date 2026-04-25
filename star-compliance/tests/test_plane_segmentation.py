"""Unit tests for the shared plane-segmentation helper.

These do not require ROS2 and can run on any machine with open3d +
numpy installed. They validate the geometric correctness of the RANSAC
wrapper against synthetic point clouds with known slope.
"""

from __future__ import annotations

import math

import numpy as np
import pytest

try:
    import open3d  # noqa: F401
    HAS_OPEN3D = True
except ImportError:
    HAS_OPEN3D = False

from star_compliance.engines.plane_segmentation import fit_plane_ransac


def synthetic_plane(slope_deg: float, n: int = 400, noise: float = 0.005) -> np.ndarray:
    """Generate a random point cloud lying on a plane inclined by
    `slope_deg` from horizontal, rotated about the y-axis."""
    rng = np.random.default_rng(42)
    xs = rng.uniform(-1.0, 1.0, size=n)
    ys = rng.uniform(-1.0, 1.0, size=n)
    slope = math.radians(slope_deg)
    zs = xs * math.tan(slope)
    zs += rng.normal(0.0, noise, size=n)
    return np.column_stack([xs, ys, zs])


@pytest.mark.skipif(not HAS_OPEN3D, reason="open3d not installed")
@pytest.mark.parametrize("slope_deg", [0.0, 2.0, 4.76, 6.0, 10.0])
def test_fit_plane_recovers_slope(slope_deg):
    pts = synthetic_plane(slope_deg)
    fit = fit_plane_ransac(pts)
    assert abs(fit.slope_deg - slope_deg) < 0.6, (
        f"expected ~{slope_deg}, got {fit.slope_deg}"
    )
