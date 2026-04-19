"""Unit tests for the medial-axis corridor analysis."""

from __future__ import annotations

import numpy as np
import pytest

from star_compliance.detectors.corridor_medial_axis import (
    ADA_403_5_MIN_WIDTH_M,
    compute_corridor_clearance,
    scan_min_width_along_line,
)

try:
    from skimage.morphology import medial_axis  # noqa: F401
    HAS_SKIMAGE = True
except ImportError:
    HAS_SKIMAGE = False


def _corridor_grid(width_cells: int, length_cells: int = 80) -> np.ndarray:
    """Build an occupancy grid: two rows of walls, free space between."""
    grid = np.full((width_cells + 4, length_cells), 100, dtype=np.int8)
    grid[2:2 + width_cells, :] = 0   # free band
    return grid


def test_ada_403_5_threshold_constant():
    assert abs(ADA_403_5_MIN_WIDTH_M - 0.9144) < 1e-6


@pytest.mark.skipif(not HAS_SKIMAGE, reason="scikit-image not installed")
def test_wide_corridor_does_not_violate():
    # 40 cells at 0.05 m = 2.0 m wide corridor, well above 36 in.
    grid = _corridor_grid(width_cells=40)
    result = compute_corridor_clearance(grid, resolution_m=0.05)
    assert not result.violates_403_5()


@pytest.mark.skipif(not HAS_SKIMAGE, reason="scikit-image not installed")
def test_narrow_corridor_flags_violation():
    # 12 cells at 0.05 m = 0.60 m wide - below the 0.9144 m threshold.
    grid = _corridor_grid(width_cells=12)
    result = compute_corridor_clearance(grid, resolution_m=0.05)
    assert result.violates_403_5()
    assert 0.3 < result.min_clearance_m < 0.9


def test_scan_min_width_returns_nan_when_no_points():
    result = scan_min_width_along_line(np.empty((0, 2)), heading_rad=0.0)
    assert np.isnan(result)


def test_scan_min_width_measures_corridor_width():
    # Synthetic points: left wall at y=+0.5, right wall at y=-0.5,
    # heading straight ahead (0 rad).
    rng = np.random.default_rng(0)
    xs = np.linspace(0.1, 1.0, 40)
    left = np.column_stack([xs, np.full_like(xs, +0.5)])
    right = np.column_stack([xs, np.full_like(xs, -0.5)])
    pts = np.vstack([left, right])
    width = scan_min_width_along_line(pts, heading_rad=0.0, band_m=0.6)
    assert abs(width - 1.0) < 0.05
