"""
Behavioral tests for each compliance node.

Where test_nodes_smoke.py proves the nodes *wire up* correctly, this
file proves they *process messages* correctly: feed synthetic
LaserScan / Imu / PointCloud2 / OccupancyGrid into the real callback
functions and assert that downstream state updates (CSV rows, publish
calls, detector invocations) happen as designed.

Uses the ros_mocks.py harness so no rclpy install is required.
"""

from __future__ import annotations

import math
import os
import tempfile
from pathlib import Path

import numpy as np
import pytest

from tests.ros_mocks import install_ros_mocks

install_ros_mocks()


# ---------------------------------------------------------------------
# Shared synthetic-message builders
# ---------------------------------------------------------------------


def _make_stamp(sec: float):
    stamp = type("S", (), {})()
    stamp.sec = int(sec)
    stamp.nanosec = int((sec - int(sec)) * 1e9)
    return stamp


def _make_header(t_sec: float = 0.0, frame_id: str = "map"):
    h = type("H", (), {})()
    h.stamp = _make_stamp(t_sec)
    h.frame_id = frame_id
    return h


def make_corridor_scan(width_m: float, num_points: int = 360,
                       max_range_m: float = 12.0):
    """Build a LaserScan-shaped duck type representing a narrow corridor.

    Walls at y = +- width_m/2 from x=0 to x=4 m; unknown points return
    max_range.
    """
    half = width_m / 2.0
    ranges = []
    angles = np.linspace(-math.pi, math.pi, num_points, endpoint=False)
    for theta in angles:
        # Ray from origin along theta. Intersect with y = +/- half.
        if abs(math.sin(theta)) < 1e-4:
            ranges.append(max_range_m)
            continue
        for wall_y in (half, -half):
            t = wall_y / math.sin(theta)
            if t <= 0:
                continue
            x = t * math.cos(theta)
            if 0.05 < x < 4.0:
                ranges.append(t)
                break
        else:
            ranges.append(max_range_m)

    msg = type("LaserScan", (), {})()
    msg.ranges = ranges
    msg.angle_min = -math.pi
    msg.angle_max = math.pi
    msg.angle_increment = (2 * math.pi) / num_points
    msg.range_min = 0.15
    msg.range_max = max_range_m
    msg.header = _make_header()
    return msg


def make_odom(x: float = 0.0, y: float = 0.0, vx: float = 0.0,
              yaw_rad: float = 0.0):
    msg = type("Odometry", (), {})()
    msg.header = _make_header()
    msg.pose = type("P", (), {})()
    msg.pose.pose = type("Po", (), {})()
    msg.pose.pose.position = type("Pos", (), {})()
    msg.pose.pose.position.x = float(x)
    msg.pose.pose.position.y = float(y)
    msg.pose.pose.position.z = 0.0
    msg.pose.pose.orientation = type("O", (), {})()
    msg.pose.pose.orientation.x = 0.0
    msg.pose.pose.orientation.y = 0.0
    msg.pose.pose.orientation.z = math.sin(yaw_rad / 2)
    msg.pose.pose.orientation.w = math.cos(yaw_rad / 2)
    msg.twist = type("T", (), {})()
    msg.twist.twist = type("Tw", (), {})()
    msg.twist.twist.linear = type("L", (), {})()
    msg.twist.twist.linear.x = float(vx)
    msg.twist.twist.linear.y = 0.0
    return msg


def make_imu(t_sec: float = 0.0, ax: float = 0.0, ay: float = 0.0,
             az: float = 9.80665, qx=0.0, qy=0.0, qz=0.0, qw=1.0):
    msg = type("Imu", (), {})()
    msg.header = _make_header(t_sec)
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


def make_occupancy_grid(width_cells: int = 100, height_cells: int = 100,
                        resolution_m: float = 0.05,
                        corridor_width_cells: int = 20):
    """OccupancyGrid with a central free corridor flanked by walls."""
    msg = type("OccupancyGrid", (), {})()
    msg.header = _make_header()
    msg.info = type("I", (), {})()
    msg.info.width = width_cells
    msg.info.height = height_cells
    msg.info.resolution = resolution_m
    msg.info.origin = type("Or", (), {})()
    msg.info.origin.position = type("P", (), {})()
    msg.info.origin.position.x = 0.0
    msg.info.origin.position.y = 0.0
    msg.info.origin.position.z = 0.0

    data = np.full((height_cells, width_cells), 100, dtype=np.int8)
    start = (height_cells - corridor_width_cells) // 2
    end = start + corridor_width_cells
    data[start:end, :] = 0
    msg.data = data.ravel().tolist()
    return msg


# ---------------------------------------------------------------------
# door_clear_width_node: feed a narrow-corridor scan, expect CSV write
# ---------------------------------------------------------------------


def test_door_clear_width_narrow_scan_writes_csv(tmp_path, monkeypatch):
    csv_path = tmp_path / "validation_log.csv"
    monkeypatch.setenv("STAR_VALIDATION_CSV", str(csv_path))

    # Must re-import the node so the env var is picked up.
    import importlib

    import star_compliance.nodes.door_clear_width_node as door_mod
    importlib.reload(door_mod)
    DoorClearWidthNode = door_mod.DoorClearWidthNode

    node = DoorClearWidthNode()

    # Deliver a narrow-corridor scan - should trigger a candidate and
    # write a CSV row even without stereo / image data (the node
    # gracefully degrades).
    scan_cb = node.find_subscription("/scan").callback
    odom_cb = node.find_subscription("/odom").callback

    odom_cb(make_odom(x=0.0, y=0.0))
    scan_cb(make_corridor_scan(0.85))   # 33.5 in - still candidate

    # One violation flag, one CSV row appended.
    assert csv_path.exists()
    lines = csv_path.read_text().splitlines()
    assert len(lines) >= 2
    header, row = lines[0], lines[-1]
    cols = row.split(",")
    # frame_width is col index 5 per the node's CSV schema
    frame_width = float(cols[5])
    # The detector's RANSAC should recover ~0.85 within a few cm.
    assert 0.70 < frame_width < 1.00


def test_door_clear_width_wide_scan_does_not_flag(tmp_path, monkeypatch):
    csv_path = tmp_path / "validation_log.csv"
    monkeypatch.setenv("STAR_VALIDATION_CSV", str(csv_path))

    import importlib
    import star_compliance.nodes.door_clear_width_node as door_mod
    importlib.reload(door_mod)

    node = door_mod.DoorClearWidthNode()
    node.find_subscription("/odom").callback(make_odom())
    node.find_subscription("/scan").callback(make_corridor_scan(1.5))

    assert csv_path.exists()
    # Only the header row - no candidate flagged
    assert len(csv_path.read_text().splitlines()) == 1


# ---------------------------------------------------------------------
# door_threshold_node: watch window + jolt
# ---------------------------------------------------------------------


def test_door_threshold_jolt_in_watch_fires_event(tmp_path, monkeypatch):
    csv_path = tmp_path / "threshold_log.csv"
    monkeypatch.setenv("STAR_THRESHOLD_CSV", str(csv_path))

    import importlib
    import star_compliance.nodes.door_threshold_node as th_mod
    importlib.reload(th_mod)
    node = th_mod.DoorThresholdNode()

    # Arm the watch manually (normally triggered by a DoorwayMeasurement).
    import time
    node._detector.start_watch(time.time())
    node.find_subscription("/odom").callback(make_odom(x=2.0, vx=0.3))

    # Feed 20 quiet samples
    imu_cb = node.find_subscription("/imu/data").callback
    for i in range(20):
        imu_cb(make_imu(t_sec=time.time(),
                        az=9.80665))

    # Feed 5 spike samples (> 2.0 m/s^2 above baseline)
    for i in range(5):
        imu_cb(make_imu(t_sec=time.time(),
                        az=9.80665 + 3.0))

    assert csv_path.exists()
    lines = csv_path.read_text().splitlines()
    assert len(lines) >= 2   # header + at least one jolt row


def test_door_threshold_jolt_outside_watch_ignored(tmp_path, monkeypatch):
    csv_path = tmp_path / "threshold_log.csv"
    monkeypatch.setenv("STAR_THRESHOLD_CSV", str(csv_path))

    import importlib
    import star_compliance.nodes.door_threshold_node as th_mod
    importlib.reload(th_mod)
    node = th_mod.DoorThresholdNode()

    # No start_watch -> not armed
    imu_cb = node.find_subscription("/imu/data").callback
    for i in range(25):
        imu_cb(make_imu(t_sec=i * 0.005, az=9.80665 + 5.0))

    # Only the header, no events.
    assert csv_path.exists()
    assert len(csv_path.read_text().splitlines()) == 1


# ---------------------------------------------------------------------
# path_blockage_node: blocked corridor flag, open corridor doesn't
# ---------------------------------------------------------------------


def test_path_blockage_open_corridor_does_not_flag(tmp_path, monkeypatch):
    pytest.importorskip("skimage")
    csv_path = tmp_path / "blockage_log.csv"
    monkeypatch.setenv("STAR_BLOCKAGE_CSV", str(csv_path))

    import importlib
    import star_compliance.nodes.path_blockage_node as pb_mod
    importlib.reload(pb_mod)
    node = pb_mod.PathBlockageNode()

    node.find_subscription("/map").callback(
        make_occupancy_grid(corridor_width_cells=40)   # 2.0 m wide
    )
    node.find_subscription("/odom").callback(make_odom(x=1.0, y=2.5))
    node.find_subscription("/scan").callback(make_corridor_scan(1.95))
    node.find_subscription("/scan").callback(make_corridor_scan(1.95))
    node.find_subscription("/scan").callback(make_corridor_scan(1.95))

    assert csv_path.exists()
    # Just the header - no blockage flagged
    assert len(csv_path.read_text().splitlines()) == 1


# ---------------------------------------------------------------------
# compliance_monitor_node: CPU threshold toggles ADA 307 parameter
# ---------------------------------------------------------------------


def test_compliance_monitor_high_load_throttles_ada_307(monkeypatch):
    import star_compliance.nodes.compliance_monitor_node as mon_mod

    # Patch getloadavg to simulate high load
    monkeypatch.setattr(os, "getloadavg", lambda: (10.0, 0.0, 0.0))

    node = mon_mod.ComplianceMonitorNode()
    node._cpu_count = 4   # so 10 / 4 = 2.5 load ratio -> throttle
    node._tick()
    assert node._throttled_ada_307 is True

    # Now simulate low load and confirm it re-enables
    monkeypatch.setattr(os, "getloadavg", lambda: (0.5, 0.0, 0.0))
    node._tick()
    assert node._throttled_ada_307 is False


# ---------------------------------------------------------------------
# dynamic_obstacle_node: DBSCAN on a scan that's off-map produces clusters
# ---------------------------------------------------------------------


def test_dynamic_obstacle_empty_scan_does_not_crash(tmp_path):
    import importlib
    import star_compliance.nodes.dynamic_obstacle_node as do_mod
    importlib.reload(do_mod)
    node = do_mod.DynamicObstacleNode()

    node.find_subscription("/map").callback(make_occupancy_grid())
    node.find_subscription("/odom").callback(make_odom())
    # Empty scan should short-circuit cleanly
    empty = type("LaserScan", (), {})()
    empty.ranges = []
    empty.angle_min = 0.0
    empty.angle_max = 0.0
    empty.angle_increment = 0.0
    empty.range_min = 0.15
    empty.range_max = 12.0
    empty.header = _make_header()
    # No exception expected
    node.find_subscription("/scan").callback(empty)
