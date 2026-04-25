"""
YOLO 3D detection fusion behaviour tests.

Verifies that the protruding-objects and dynamic-obstacle nodes,
when fed a synthetic Detection3DArray, correctly tag their outputs
with COCO class labels and gate ADA 307 flagging on the
TRANSIENT_CLASSES set.
"""

from __future__ import annotations

from types import SimpleNamespace
from unittest.mock import MagicMock

import pytest

from tests.ros_mocks import install_ros_mocks

install_ros_mocks()


def _make_identity_transform():
    """A TransformStamped with translation 0 and identity quaternion."""
    tf = SimpleNamespace()
    tf.transform = SimpleNamespace()
    tf.transform.translation = SimpleNamespace(x=0.0, y=0.0, z=0.0)
    tf.transform.rotation = SimpleNamespace(x=0.0, y=0.0, z=0.0, w=1.0)
    return tf


def _make_detection_3d_msg(detections):
    """Build a duck-typed Detection3DArray from (label, score, x, y, z)."""
    msg = SimpleNamespace()
    msg.header = SimpleNamespace()
    msg.header.stamp = SimpleNamespace(sec=0, nanosec=0)
    msg.header.frame_id = "cam0_optical_frame"
    msg.detections = []
    for label, score, x, y, z in detections:
        d = SimpleNamespace()
        d.bbox = SimpleNamespace()
        d.bbox.center = SimpleNamespace()
        d.bbox.center.position = SimpleNamespace(x=x, y=y, z=z)
        hyp = SimpleNamespace()
        hyp.hypothesis = SimpleNamespace(class_id=label, score=score)
        d.results = [hyp]
        msg.detections.append(d)
    return msg


# ---------------------------------------------------------------------------
# protruding_objects_node YOLO matching
# ---------------------------------------------------------------------------


def _yolo_into_protruding_node(node, detections):
    node._tf_buffer.transform_to_return = _make_identity_transform()
    node.find_subscription("/perception/detections_3d").callback(
        _make_detection_3d_msg(detections)
    )


def test_protruding_node_caches_yolo_in_map_frame():
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    node = ProtrudingObjectsNode()
    _yolo_into_protruding_node(node, [
        ("chair", 0.85, 1.0, 2.0, 1.2),
        ("person", 0.92, 0.0, 1.0, 1.1),
    ])
    assert len(node._yolo_cache) == 2
    # Cache entries are (received_sec, x, y, z, label, score).
    labels = sorted(entry[4] for entry in node._yolo_cache)
    assert labels == ["chair", "person"]


def test_protruding_node_match_yolo_finds_nearest_within_radius():
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    node = ProtrudingObjectsNode()
    _yolo_into_protruding_node(node, [
        ("chair", 0.85, 1.00, 2.00, 1.20),
        ("person", 0.92, 5.00, 5.00, 1.10),
    ])
    label, score = node._match_yolo(1.05, 2.05, 1.20)
    assert label == "chair"
    assert score == pytest.approx(0.85)


def test_protruding_node_match_yolo_returns_empty_outside_radius():
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    node = ProtrudingObjectsNode()
    _yolo_into_protruding_node(node, [("chair", 0.85, 0.0, 0.0, 0.0)])
    # 5 m away is far outside the default 0.30 m radius.
    label, score = node._match_yolo(5.0, 5.0, 5.0)
    assert label == ""
    assert score == 0.0


def test_protruding_node_match_yolo_empty_cache():
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    node = ProtrudingObjectsNode()
    label, score = node._match_yolo(0.0, 0.0, 0.0)
    assert label == ""
    assert score == 0.0


def test_protruding_node_transform_point_identity():
    """An identity transform must leave coordinates unchanged."""
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    tf = _make_identity_transform()
    x, y, z = ProtrudingObjectsNode._transform_point(1.5, -0.5, 2.0, tf)
    assert (x, y, z) == pytest.approx((1.5, -0.5, 2.0))


def test_protruding_node_transform_point_translation():
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    tf = _make_identity_transform()
    tf.transform.translation = SimpleNamespace(x=10.0, y=-2.0, z=0.5)
    x, y, z = ProtrudingObjectsNode._transform_point(1.0, 1.0, 1.0, tf)
    assert (x, y, z) == pytest.approx((11.0, -1.0, 1.5))


# ---------------------------------------------------------------------------
# dynamic_obstacle_node YOLO matching
# ---------------------------------------------------------------------------


def _yolo_into_dynamic_node(node, detections):
    node._tf_buffer.transform_to_return = _make_identity_transform()
    node.find_subscription("/perception/detections_3d").callback(
        _make_detection_3d_msg(detections)
    )


def test_dynamic_obstacle_node_caches_yolo():
    from star_compliance.nodes.dynamic_obstacle_node import DynamicObstacleNode

    node = DynamicObstacleNode()
    _yolo_into_dynamic_node(node, [
        ("person", 0.95, 1.0, 1.0, 1.0),
        ("chair", 0.40, 2.0, 0.0, 1.2),
    ])
    assert len(node._yolo_cache) == 2


def test_dynamic_obstacle_node_match_yolo_2d_radius():
    """Match operates in XY (the obstacle plane); Z is ignored."""
    from star_compliance.nodes.dynamic_obstacle_node import DynamicObstacleNode

    node = DynamicObstacleNode()
    _yolo_into_dynamic_node(node, [("person", 0.95, 1.0, 1.0, 1.5)])
    idx, label, score = node._match_yolo(1.05, 1.05, radius_m=0.30)
    assert idx == 0
    assert label == "person"
    assert score == pytest.approx(0.95)


def test_dynamic_obstacle_node_no_match_outside_radius():
    from star_compliance.nodes.dynamic_obstacle_node import DynamicObstacleNode

    node = DynamicObstacleNode()
    _yolo_into_dynamic_node(node, [("person", 0.95, 0.0, 0.0, 0.0)])
    idx, label, score = node._match_yolo(5.0, 5.0, radius_m=0.30)
    assert idx is None
    assert label == ""
    assert score == 0.0


def test_dynamic_obstacle_node_transform_point_identity():
    from star_compliance.nodes.dynamic_obstacle_node import DynamicObstacleNode

    tf = _make_identity_transform()
    x, y, z = DynamicObstacleNode._transform_point(2.0, 3.0, 4.0, tf)
    assert (x, y, z) == pytest.approx((2.0, 3.0, 4.0))


# ---------------------------------------------------------------------------
# Cross-node: TRANSIENT_CLASSES gating produces the right flagged value
# ---------------------------------------------------------------------------


def test_transient_class_clears_violation_flag():
    """The protruding-objects node must un-flag transient classes."""
    from star_compliance.detectors.cane_zone_filter import is_transient_class

    # Simulate the gating expression used in protruding_objects_node._emit:
    for label, expected in (
        ("person", False),
        ("chair", False),
        ("backpack", False),
        ("fire hydrant", True),
        ("tv", True),
        ("", True),  # unclassified -> presumed-fixed -> flag
    ):
        assert (not is_transient_class(label)) is expected
