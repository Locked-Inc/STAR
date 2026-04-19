"""
Smoke tests: every compliance node imports and constructs cleanly.

Uses the sys.modules-level ROS 2 mocks defined in ros_mocks.py so the
tests run off-robot with no rclpy install. Each test verifies:

1. The node module imports without error.
2. The node class instantiates on top of FakeNode.
3. Every advertised subscription is registered on the expected topic.
4. Every advertised publisher is registered on the expected topic.
5. Every declared launch parameter has a sane default.

These catch typos, wrong signatures, and missing imports that unit tests
for the detectors alone cannot. Launch tests (see test_launch_*.py)
cover the live message-flow side.
"""

from __future__ import annotations

import pytest

from tests.ros_mocks import install_ros_mocks

install_ros_mocks()


# ---------------------------------------------------------------------


def test_door_clear_width_node_wires_topics():
    from star_compliance.nodes.door_clear_width_node import DoorClearWidthNode

    node = DoorClearWidthNode()

    topics = {s.topic for s in node.subscriptions}
    assert "/scan" in topics
    assert "/imu/data" in topics
    assert "/odom" in topics
    assert "/cam0/camera/image_rect_color" in topics
    assert "/stereo/points2" in topics

    pubs = {p.topic for p in node.publishers}
    assert "/compliance/door_clear_width" in pubs

    assert "door_offset_m" in node.parameters
    assert "sensor_height_m" in node.parameters
    assert "enabled" in node.parameters
    assert node.parameters["enabled"] is True


def test_door_threshold_node_wires_topics():
    from star_compliance.nodes.door_threshold_node import DoorThresholdNode

    node = DoorThresholdNode()

    topics = {s.topic for s in node.subscriptions}
    assert "/imu/data" in topics
    assert "/odom" in topics
    assert "/compliance/door_clear_width" in topics

    pubs = {p.topic for p in node.publishers}
    assert "/compliance/door_threshold" in pubs

    assert "threshold_ms2" in node.parameters
    assert "watch_duration_sec" in node.parameters


def test_protruding_objects_node_wires_topics():
    from star_compliance.nodes.protruding_objects_node import ProtrudingObjectsNode

    node = ProtrudingObjectsNode()

    topics = {s.topic for s in node.subscriptions}
    # input_cloud_topic default is /cloud_map
    assert "/cloud_map" in topics
    assert "/imu/data" in topics

    pubs = {p.topic for p in node.publishers}
    assert "/compliance/protruding_objects" in pubs

    assert "enabled" in node.parameters
    assert "input_cloud_topic" in node.parameters
    assert node.parameters["input_cloud_topic"] == "/cloud_map"


def test_path_blockage_node_wires_topics():
    from star_compliance.nodes.path_blockage_node import PathBlockageNode

    node = PathBlockageNode()

    topics = {s.topic for s in node.subscriptions}
    assert "/map" in topics
    assert "/scan" in topics
    assert "/odom" in topics

    pubs = {p.topic for p in node.publishers}
    assert "/compliance/path_blockage" in pubs


def test_dynamic_obstacle_node_wires_topics():
    from star_compliance.nodes.dynamic_obstacle_node import DynamicObstacleNode

    node = DynamicObstacleNode()

    topics = {s.topic for s in node.subscriptions}
    assert "/scan" in topics
    assert "/map" in topics
    assert "/odom" in topics

    pubs = {p.topic for p in node.publishers}
    assert "/perception/dynamic_obstacles" in pubs


def test_compliance_monitor_node_wires_topics():
    from star_compliance.nodes.compliance_monitor_node import ComplianceMonitorNode

    node = ComplianceMonitorNode()

    pubs = {p.topic for p in node.publishers}
    assert "/compliance/monitor/status" in pubs

    assert len(node.timers) >= 1
    assert node._cpu_count >= 1


def test_ramp_slope_node_wires_topics():
    from star_compliance.nodes.ramp_slope_node import RampSlopeNode

    node = RampSlopeNode()

    topics = {s.topic for s in node.subscriptions}
    assert "/scan" in topics
    assert "/imu/data" in topics
    assert "/odom" in topics


# ---------------------------------------------------------------------


def test_door_clear_width_disabled_short_circuits_scan_callback():
    from star_compliance.nodes.door_clear_width_node import DoorClearWidthNode

    node = DoorClearWidthNode()
    node.set_mock_parameter("enabled", False)

    sub = node.find_subscription("/scan")
    fake_scan = _fake_laser_scan(ranges=[1.0] * 360)
    # Should silently return without touching downstream state
    sub.callback(fake_scan)

    # No doorway measurement published
    pub = node.find_publisher("/compliance/door_clear_width")
    assert pub.sent == []


def test_door_threshold_disabled_short_circuits_imu_callback():
    from star_compliance.nodes.door_threshold_node import DoorThresholdNode

    node = DoorThresholdNode()
    node.set_mock_parameter("enabled", False)

    sub = node.find_subscription("/imu/data")
    fake_imu = _fake_imu(ax=0.0, ay=0.0, az=9.8, qw=1.0)
    sub.callback(fake_imu)

    pub = node.find_publisher("/compliance/door_threshold")
    assert pub.sent == []


# ---------------------------------------------------------------------
# Helpers


def _fake_laser_scan(ranges):
    """Build a LaserScan-shaped duck type the callbacks can consume."""
    import math
    msg = type("LaserScan", (), {})()
    msg.ranges = list(ranges)
    msg.angle_min = -math.pi
    msg.angle_max = math.pi
    msg.angle_increment = (2 * math.pi) / max(1, len(ranges))
    msg.range_min = 0.15
    msg.range_max = 12.0
    return msg


def _fake_imu(ax, ay, az, qw=1.0, qx=0.0, qy=0.0, qz=0.0):
    msg = type("Imu", (), {})()
    msg.header = type("H", (), {})()
    msg.header.stamp = type("S", (), {})()
    msg.header.stamp.sec = 0
    msg.header.stamp.nanosec = 0
    msg.orientation = type("O", (), {})()
    msg.orientation.x = qx
    msg.orientation.y = qy
    msg.orientation.z = qz
    msg.orientation.w = qw
    msg.linear_acceleration = type("A", (), {})()
    msg.linear_acceleration.x = ax
    msg.linear_acceleration.y = ay
    msg.linear_acceleration.z = az
    return msg
