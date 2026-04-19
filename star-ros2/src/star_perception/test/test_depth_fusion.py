"""Unit tests for the depth-fusion geometry helpers."""

from __future__ import annotations

import numpy as np
import pytest

from star_perception.depth_fusion import (
    CameraIntrinsics,
    Detection3D,
    back_project_pixel,
    depth_from_disparity,
    fuse_bbox_to_3d,
    median_disparity_in_bbox,
)


# A simple synthetic camera: 640x480 with fx=fy=500, principal point centred.
_K = np.array([[500.0, 0.0, 320.0],
               [0.0, 500.0, 240.0],
               [0.0, 0.0, 1.0]])
_INTR = CameraIntrinsics.from_k_matrix(_K)
_FOCAL_PX = 500.0
_BASELINE_M = 0.061  # Matches the IMX219-83 stereo rig.


def test_intrinsics_from_k_matrix():
    assert _INTR.fx == 500.0
    assert _INTR.fy == 500.0
    assert _INTR.cx == 320.0
    assert _INTR.cy == 240.0


def test_depth_from_disparity_known_value():
    # Object at 2.5 m: d = f * T / Z = 500 * 0.061 / 2.5 = 12.2 px.
    z = depth_from_disparity(12.2, _FOCAL_PX, _BASELINE_M)
    assert z == pytest.approx(2.5, rel=1e-3)


def test_depth_from_disparity_invalid_returns_nan():
    assert np.isnan(depth_from_disparity(0.0, _FOCAL_PX, _BASELINE_M))
    assert np.isnan(depth_from_disparity(-1.0, _FOCAL_PX, _BASELINE_M))


def test_depth_from_disparity_validates_positive_focal_and_baseline():
    with pytest.raises(ValueError):
        depth_from_disparity(10.0, 0.0, _BASELINE_M)
    with pytest.raises(ValueError):
        depth_from_disparity(10.0, _FOCAL_PX, 0.0)


def test_back_project_centre_pixel_at_known_depth():
    # Pixel = principal point at z=3 m -> (0, 0, 3) in camera frame.
    x, y, z = back_project_pixel(320.0, 240.0, 3.0, _INTR)
    assert (x, y, z) == pytest.approx((0.0, 0.0, 3.0))


def test_back_project_offset_pixel():
    # Move 100 px right of cx at 2 m: X = 100 * 2 / 500 = 0.40 m.
    x, y, _ = back_project_pixel(420.0, 240.0, 2.0, _INTR)
    assert x == pytest.approx(0.40)
    assert y == pytest.approx(0.0)


def test_back_project_rejects_non_positive_depth():
    with pytest.raises(ValueError):
        back_project_pixel(100.0, 100.0, 0.0, _INTR)


def test_median_disparity_uniform_roi():
    disp = np.full((480, 640), 12.2, dtype=np.float32)
    median, count = median_disparity_in_bbox(
        disp, x_min=200, y_min=180, x_max=440, y_max=300,
        border_pct=0.15,
    )
    assert median == pytest.approx(12.2)
    assert count > 1000


def test_median_disparity_ignores_invalid_values():
    disp = np.full((480, 640), 12.2, dtype=np.float32)
    # Salt the ROI with NaN, inf, and zero.
    disp[200:220, 250:270] = np.nan
    disp[220:240, 250:270] = np.inf
    disp[240:260, 250:270] = 0.0
    median, count = median_disparity_in_bbox(
        disp, x_min=200, y_min=180, x_max=440, y_max=300,
        border_pct=0.15,
    )
    assert median == pytest.approx(12.2)
    # Count must exclude the salted samples.
    assert count > 0


def test_median_disparity_empty_roi_returns_nan():
    disp = np.zeros((480, 640), dtype=np.float32)
    median, count = median_disparity_in_bbox(
        disp, x_min=0, y_min=0, x_max=10, y_max=10,
        border_pct=0.15,
    )
    assert np.isnan(median)
    assert count == 0


def test_median_disparity_rejects_invalid_border_pct():
    disp = np.zeros((10, 10), dtype=np.float32)
    with pytest.raises(ValueError):
        median_disparity_in_bbox(disp, 0, 0, 10, 10, border_pct=0.5)
    with pytest.raises(ValueError):
        median_disparity_in_bbox(disp, 0, 0, 10, 10, border_pct=-0.1)


def test_median_disparity_rejects_non_2d():
    with pytest.raises(ValueError):
        median_disparity_in_bbox(
            np.zeros((10, 10, 3), dtype=np.float32),
            0, 0, 10, 10, border_pct=0.0,
        )


def test_fuse_bbox_to_3d_centred_object():
    """
    Synthetic scene: a uniform 12.2 px disparity patch in the centre of
    a 480x640 disparity image. With f=500 and T=0.061 the patch sits at
    Z = 2.5 m. A 200 x 100 px bbox centred on the principal point should
    back-project to (0, 0, 2.5) with size (0.80, 0.40, ~0.05) m.
    """
    disp = np.full((480, 640), 12.2, dtype=np.float32)
    out = fuse_bbox_to_3d(
        disp, _INTR, _FOCAL_PX, _BASELINE_M,
        class_id=0, class_name="person", score=0.9,
        x_min=220, y_min=190, x_max=420, y_max=290,
    )
    assert out is not None
    assert out.cx_m == pytest.approx(0.0, abs=0.02)
    assert out.cy_m == pytest.approx(0.0, abs=0.02)
    assert out.cz_m == pytest.approx(2.5, rel=1e-3)
    assert out.size_x_m == pytest.approx(1.0, rel=0.02)  # 200 * 2.5 / 500
    assert out.size_y_m == pytest.approx(0.5, rel=0.02)  # 100 * 2.5 / 500
    # Uniform disparity -> tiny size_z_m, but clamped to >=0.05.
    assert out.size_z_m >= 0.05


def test_fuse_bbox_to_3d_offset_object():
    """
    A bbox shifted right of centre at known depth must produce a positive
    cx_m equal to back-projection of its centre.
    """
    disp = np.full((480, 640), 12.2, dtype=np.float32)  # 2.5 m everywhere.
    out = fuse_bbox_to_3d(
        disp, _INTR, _FOCAL_PX, _BASELINE_M,
        class_id=56, class_name="chair", score=0.7,
        x_min=420, y_min=190, x_max=620, y_max=290,
    )
    assert out is not None
    # Centre u = 520, dx = 200 px. X = 200 * 2.5 / 500 = 1.00 m.
    assert out.cx_m == pytest.approx(1.0, abs=0.02)
    assert out.cz_m == pytest.approx(2.5, rel=1e-3)


def test_fuse_bbox_to_3d_drops_below_min_samples():
    disp = np.full((480, 640), float("nan"), dtype=np.float32)
    # Tiny patch of valid disparity.
    disp[200:205, 200:205] = 12.2
    out = fuse_bbox_to_3d(
        disp, _INTR, _FOCAL_PX, _BASELINE_M,
        class_id=0, class_name="person", score=0.9,
        x_min=190, y_min=190, x_max=215, y_max=215,
        min_valid_samples=100,
    )
    assert out is None


def test_fuse_bbox_to_3d_drops_too_close():
    # Disparity 200 px -> Z = 0.15 m -- below 0.20 m floor.
    disp = np.full((480, 640), 200.0, dtype=np.float32)
    out = fuse_bbox_to_3d(
        disp, _INTR, _FOCAL_PX, _BASELINE_M,
        class_id=0, class_name="person", score=0.9,
        x_min=200, y_min=180, x_max=440, y_max=300,
    )
    assert out is None


def test_fuse_bbox_to_3d_drops_too_far():
    # Disparity 1 px -> Z = 30.5 m -- well beyond 6 m max.
    disp = np.full((480, 640), 1.0, dtype=np.float32)
    out = fuse_bbox_to_3d(
        disp, _INTR, _FOCAL_PX, _BASELINE_M,
        class_id=0, class_name="person", score=0.9,
        x_min=200, y_min=180, x_max=440, y_max=300,
    )
    assert out is None


def test_detection_3d_dataclass_round_trip():
    d = Detection3D(
        class_id=0, class_name="person", score=0.8,
        cx_m=1.0, cy_m=0.0, cz_m=2.5,
        size_x_m=0.5, size_y_m=1.7, size_z_m=0.3,
        sample_count=200,
    )
    assert d.class_name == "person"
    assert d.cz_m == 2.5
    assert d.sample_count == 200
