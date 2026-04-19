"""Unit tests for the Hailo runner pre/post-processing math."""

from __future__ import annotations

import numpy as np
import pytest

from star_perception.hailo_runner import (
    COCO_CLASSES,
    Detection,
    HailoYoloRunner,
    LETTERBOX_PAD_VALUE,
    YOLOV8_INPUT_SIZE,
    compute_letterbox,
    deletterbox_bbox,
    letterbox_image,
)


# ---------------------------------------------------------------------------
# Letterbox geometry
# ---------------------------------------------------------------------------


def test_letterbox_landscape_geometry():
    p = compute_letterbox(src_h=480, src_w=640, target=640)
    assert p.scale == pytest.approx(1.0)
    assert p.pad_x == 0
    assert p.pad_y == (640 - 480) // 2

def test_letterbox_portrait_geometry():
    p = compute_letterbox(src_h=800, src_w=600, target=640)
    assert p.scale == pytest.approx(640.0 / 800.0)
    new_w = int(round(600 * p.scale))
    assert p.pad_x == (640 - new_w) // 2
    assert p.pad_y == 0

def test_letterbox_square_geometry():
    p = compute_letterbox(src_h=512, src_w=512, target=640)
    assert p.scale == pytest.approx(640.0 / 512.0)
    assert p.pad_x == 0
    assert p.pad_y == 0


def test_letterbox_image_pads_with_grey():
    src = np.full((100, 200, 3), 30, dtype=np.uint8)
    p = compute_letterbox(100, 200, 640)
    canvas = letterbox_image(src, p)
    assert canvas.shape == (640, 640, 3)
    assert canvas.dtype == np.uint8
    # Corner pixel must be the letterbox padding value.
    assert int(canvas[0, 0, 0]) == LETTERBOX_PAD_VALUE
    # Centre pixel of resized image content must be the source colour.
    cy = p.pad_y + (int(round(100 * p.scale)) // 2)
    cx = p.pad_x + (int(round(200 * p.scale)) // 2)
    assert int(canvas[cy, cx, 0]) == 30


def test_letterbox_image_rejects_wrong_dtype():
    bad = np.zeros((100, 100, 3), dtype=np.float32)
    p = compute_letterbox(100, 100, 640)
    with pytest.raises(ValueError):
        letterbox_image(bad, p)


def test_letterbox_image_rejects_shape_mismatch():
    src = np.zeros((100, 100, 3), dtype=np.uint8)
    p = compute_letterbox(50, 50, 640)
    with pytest.raises(ValueError):
        letterbox_image(src, p)


# ---------------------------------------------------------------------------
# De-letterbox round-trip
# ---------------------------------------------------------------------------


def test_deletterbox_round_trip_identity():
    """A bbox covering the whole content area must map back to source."""
    p = compute_letterbox(480, 640, 640)
    # In letterboxed canvas, content occupies the central 480 rows.
    t = float(p.target)
    yn0 = p.pad_y / t
    yn1 = (p.pad_y + 480) / t
    x0, y0, x1, y1 = deletterbox_bbox(0.0, yn0, 1.0, yn1, p)
    assert x0 == pytest.approx(0.0)
    assert y0 == pytest.approx(0.0, abs=1e-3)
    assert x1 == pytest.approx(639.0)
    assert y1 == pytest.approx(479.0, abs=1.0)


def test_deletterbox_clamps_to_image_bounds():
    p = compute_letterbox(480, 640, 640)
    # Out-of-bounds normalized values must be clamped, not blow up.
    x0, y0, x1, y1 = deletterbox_bbox(-0.5, -0.5, 1.5, 1.5, p)
    assert x0 == 0.0 and y0 == 0.0
    assert x1 == 639.0 and y1 == 479.0


# ---------------------------------------------------------------------------
# Runner with injected backend
# ---------------------------------------------------------------------------


def _make_backend(detections):
    """Return a backend callable that always emits the given detections."""
    def backend(_canvas):
        return list(detections)
    return backend


def test_runner_filters_by_score_threshold():
    backend = _make_backend([
        (0, 0.50, 0.10, 0.10, 0.20, 0.20),  # person, kept
        (16, 0.20, 0.30, 0.30, 0.40, 0.40),  # dog, dropped
    ])
    runner = HailoYoloRunner.with_backend(backend, score_threshold=0.35)
    rgb = np.zeros((480, 640, 3), dtype=np.uint8)
    out = runner.infer(rgb)
    assert len(out) == 1
    assert out[0].class_name == "person"


def test_runner_sorts_by_score_descending():
    backend = _make_backend([
        (0, 0.50, 0.0, 0.0, 0.1, 0.1),
        (0, 0.90, 0.0, 0.0, 0.1, 0.1),
        (0, 0.70, 0.0, 0.0, 0.1, 0.1),
    ])
    runner = HailoYoloRunner.with_backend(backend, score_threshold=0.10)
    rgb = np.zeros((480, 640, 3), dtype=np.uint8)
    out = runner.infer(rgb)
    scores = [d.score for d in out]
    assert scores == sorted(scores, reverse=True)


def test_runner_drops_zero_area_boxes():
    backend = _make_backend([
        (0, 0.99, 0.50, 0.50, 0.50, 0.50),  # zero area
    ])
    runner = HailoYoloRunner.with_backend(backend, score_threshold=0.10)
    rgb = np.zeros((480, 640, 3), dtype=np.uint8)
    assert runner.infer(rgb) == []


def test_runner_maps_unknown_class_id_to_placeholder():
    backend = _make_backend([
        (999, 0.99, 0.10, 0.10, 0.20, 0.20),
    ])
    runner = HailoYoloRunner.with_backend(backend, score_threshold=0.10)
    rgb = np.zeros((480, 640, 3), dtype=np.uint8)
    out = runner.infer(rgb)
    assert out[0].class_name.startswith("class_")


def test_runner_rejects_invalid_score_threshold():
    backend = _make_backend([])
    with pytest.raises(ValueError):
        HailoYoloRunner.with_backend(backend, score_threshold=0.0)
    with pytest.raises(ValueError):
        HailoYoloRunner.with_backend(backend, score_threshold=1.0)


def test_runner_rejects_non_rgb_image():
    backend = _make_backend([])
    runner = HailoYoloRunner.with_backend(backend, score_threshold=0.10)
    bad_grey = np.zeros((480, 640), dtype=np.uint8)
    with pytest.raises(ValueError):
        runner.infer(bad_grey)


def test_runner_known_geometry_round_trip():
    """
    Place a bbox at the centre 50 percent of the canvas and verify the
    de-letterboxed coordinates land within +/- 1 pixel of the centre
    50 percent of the source 480x640 image.
    """
    backend = _make_backend([
        (0, 0.99, 0.25, 0.25, 0.75, 0.75),
    ])
    runner = HailoYoloRunner.with_backend(backend, score_threshold=0.10)
    rgb = np.zeros((480, 640, 3), dtype=np.uint8)
    [det] = runner.infer(rgb)
    # Source 640 wide: 25% to 75% -> 160 to 480.
    assert det.x_min == pytest.approx(160.0, abs=2.0)
    assert det.x_max == pytest.approx(480.0, abs=2.0)
    # Source 480 tall, with letterbox pad of (640-480)/2 = 80.
    # Canvas y 0.25..0.75 -> pixel 160..480 -> source 80..400.
    assert det.y_min == pytest.approx(80.0, abs=2.0)
    assert det.y_max == pytest.approx(400.0, abs=2.0)


def test_coco_classes_complete():
    """Sanity: 80 COCO classes, person at index 0, chair at 56."""
    assert len(COCO_CLASSES) == 80
    assert COCO_CLASSES[0] == "person"
    assert COCO_CLASSES[56] == "chair"


def test_detection_geometry_helpers():
    d = Detection(
        class_id=0, class_name="person", score=0.8,
        x_min=10.0, y_min=20.0, x_max=30.0, y_max=60.0,
    )
    assert d.width == 20.0
    assert d.height == 40.0
    assert d.cx == 20.0
    assert d.cy == 40.0


def test_yolov8_input_size_constant_is_640():
    """The Hailo zoo YOLOv8 .hef is compiled for 640 x 640."""
    assert YOLOV8_INPUT_SIZE == 640
