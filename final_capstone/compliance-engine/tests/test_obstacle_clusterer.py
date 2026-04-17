"""Unit tests for the DBSCAN obstacle clusterer."""

from __future__ import annotations

import numpy as np
import pytest

from star_compliance.detectors.obstacle_clusterer import (
    cluster_points,
    subtract_known_map,
)

try:
    import sklearn  # noqa: F401
    HAS_SKLEARN = True
except ImportError:
    HAS_SKLEARN = False


def test_subtract_known_map_drops_wall_points():
    # 10x10 grid with a wall column at x index 5
    occupancy = np.zeros((10, 10), dtype=np.int8)
    occupancy[:, 5] = 100

    # Scan points: one at the wall (x=0.5, y=0.1) and one off-wall
    points = np.array([
        [0.55, 0.05],   # wall at col 5
        [0.20, 0.05],   # free space
    ])
    result = subtract_known_map(points,
                                occupancy,
                                origin_xy=(0.0, 0.0),
                                resolution_m=0.1,
                                tolerance_m=0.1)
    # Only the free-space point survives
    assert result.shape == (1, 2)
    assert abs(result[0, 0] - 0.20) < 1e-9


def test_subtract_empty_points_returns_empty():
    occupancy = np.zeros((5, 5), dtype=np.int8)
    result = subtract_known_map(np.empty((0, 2)), occupancy,
                                origin_xy=(0.0, 0.0),
                                resolution_m=0.1)
    assert result.shape == (0, 2)


@pytest.mark.skipif(not HAS_SKLEARN, reason="scikit-learn not installed")
def test_cluster_points_finds_two_distinct_clusters():
    rng = np.random.default_rng(3)
    cluster_a = rng.normal([0.0, 0.0], 0.05, size=(40, 2))
    cluster_b = rng.normal([2.0, 2.0], 0.05, size=40).reshape(-1, 2)
    points = np.vstack([cluster_a, cluster_b])
    clusters = cluster_points(points, eps_m=0.2, min_samples=5)
    assert len(clusters) == 2


@pytest.mark.skipif(not HAS_SKLEARN, reason="scikit-learn not installed")
def test_cluster_points_returns_empty_for_too_few_points():
    points = np.array([[0.0, 0.0], [0.1, 0.1]])
    clusters = cluster_points(points, min_samples=5)
    assert clusters == []


@pytest.mark.skipif(not HAS_SKLEARN, reason="scikit-learn not installed")
def test_cluster_points_sorted_by_size_descending():
    rng = np.random.default_rng(9)
    big = rng.normal([0.0, 0.0], 0.03, size=(50, 2))
    small = rng.normal([5.0, 5.0], 0.03, size=(10, 2))
    points = np.vstack([small, big])
    clusters = cluster_points(points, eps_m=0.15, min_samples=5)
    assert len(clusters) == 2
    assert clusters[0].point_count >= clusters[1].point_count
