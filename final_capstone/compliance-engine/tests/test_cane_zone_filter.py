"""Unit tests for the ADA 307 cane-detectable-zone filter."""

from __future__ import annotations

import numpy as np

from star_compliance.detectors.cane_zone_filter import (
    ADA_307_PROTRUSION_LIMIT_M,
    CANE_ZONE_MAX_M,
    CANE_ZONE_MIN_M,
    filter_to_cane_zone,
    is_protrusion,
)


def test_cane_zone_constants_match_ada():
    # 27 inches = 0.6858 m exactly; 80 inches = 2.032 m exactly
    assert abs(CANE_ZONE_MIN_M - 0.6858) < 1e-6
    assert abs(CANE_ZONE_MAX_M - 2.032) < 1e-6
    # 4 inches = 0.1016 m exactly
    assert abs(ADA_307_PROTRUSION_LIMIT_M - 0.1016) < 1e-6


def test_filter_empty_cloud_returns_empty():
    band = filter_to_cane_zone(np.empty((0, 3)), floor_height_m=0.0)
    assert band.shape == (0, 3)


def test_filter_keeps_only_cane_zone_points():
    # Z values above a 0 m floor
    points = np.array([
        [0.0, 0.0, 0.50],    # below 27 in - below cane zone
        [0.0, 0.0, 0.70],    # just in zone (27.5 in)
        [0.0, 0.0, 1.00],    # middle of zone
        [0.0, 0.0, 2.00],    # still in zone (78.7 in)
        [0.0, 0.0, 2.10],    # above 80 in - above cane zone
    ])
    band = filter_to_cane_zone(points, floor_height_m=0.0)
    zs = sorted(band[:, 2].tolist())
    assert zs == [0.70, 1.00, 2.00]


def test_filter_honors_nonzero_floor():
    # Floor at 0.2 m, so cane zone is [0.8858, 2.232] absolute Z
    points = np.array([
        [0.0, 0.0, 0.80],   # below adjusted zone
        [0.0, 0.0, 1.00],   # in adjusted zone
        [0.0, 0.0, 2.30],   # above adjusted zone
    ])
    band = filter_to_cane_zone(points, floor_height_m=0.2)
    zs = band[:, 2].tolist()
    assert zs == [1.00]


def test_is_protrusion_below_threshold_returns_false():
    assert not is_protrusion(distance_from_wall_m=0.05,
                             height_above_floor_m=1.0,
                             point_count=10)


def test_is_protrusion_above_threshold_in_zone_returns_true():
    assert is_protrusion(distance_from_wall_m=0.15,
                         height_above_floor_m=1.0,
                         point_count=10)


def test_is_protrusion_outside_cane_zone_returns_false():
    # A 6-inch protrusion at knee height (0.45 m) is NOT an ADA 307
    # violation because ADA 307 only covers the 27-80 inch band.
    assert not is_protrusion(distance_from_wall_m=0.15,
                             height_above_floor_m=0.45,
                             point_count=10)


def test_is_protrusion_insufficient_points_returns_false():
    assert not is_protrusion(distance_from_wall_m=0.15,
                             height_above_floor_m=1.0,
                             point_count=2)
