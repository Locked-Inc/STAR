"""
ADA 403.5 accessible-path-width / path-blockage compliance node.

Subscribes to:
  /map       nav_msgs/OccupancyGrid (slam_toolbox, 0.5 Hz)
  /scan      sensor_msgs/LaserScan
  /odom      nav_msgs/Odometry

Publishes:
  /compliance/path_blockage   star_compliance_msgs/PathBlockage

Behavior:
  1. When /map updates, run the corridor_medial_axis analysis to
     compute the baseline free width along every corridor segment.
  2. On each /scan, compute an instantaneous clear-width along the
     robot's current heading.
  3. Flag a blockage when live-width < 36 inches AND
     map-width - live-width > 0.15 m for 3 consecutive scans.
"""

from __future__ import annotations

import csv
import math
import os
import time
from collections import deque
from pathlib import Path

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from sensor_msgs.msg import LaserScan
from nav_msgs.msg import OccupancyGrid, Odometry
from geometry_msgs.msg import PoseStamped

try:
    from star_compliance_msgs.msg import PathBlockage
    HAS_MSGS = True
except ImportError:  # pragma: no cover
    PathBlockage = None
    HAS_MSGS = False

from star_compliance.detectors.corridor_medial_axis import (
    ADA_403_5_MIN_WIDTH_M,
    compute_corridor_clearance,
    scan_min_width_along_line,
)
from star_compliance.detectors.doorway_lidar_detector import polar_to_xy


ADA_SECTION = "403.5"
BLOCKAGE_DELTA_M = 0.15
BLOCKAGE_SUSTAIN_FRAMES = 3

VALIDATION_CSV = Path(
    os.environ.get(
        "STAR_BLOCKAGE_CSV",
        str(Path(__file__).resolve().parents[3] / "extras" / "blockage_log.csv"),
    )
)


class PathBlockageNode(Node):

    def __init__(self) -> None:
        super().__init__("star_path_blockage_node")

        self.declare_parameter("enabled", True)

        self._map_clearance = None   # CorridorClearance
        self._map_resolution = 0.05
        self._map_origin = (0.0, 0.0)
        self._history: deque[tuple[float, float]] = deque(maxlen=BLOCKAGE_SUSTAIN_FRAMES)
        self._latest_odom: Odometry | None = None

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=5,
        )

        self._sub_map = self.create_subscription(
            OccupancyGrid, "/map", self._on_map, 10
        )
        self._sub_scan = self.create_subscription(
            LaserScan, "/scan", self._on_scan, qos_sensor
        )
        self._sub_odom = self.create_subscription(
            Odometry, "/odom", self._on_odom, qos_sensor
        )

        if HAS_MSGS:
            self._pub = self.create_publisher(
                PathBlockage, "/compliance/path_blockage", 10
            )
        else:
            self._pub = None

        self._ensure_csv_header()
        self.get_logger().info(
            f"star_path_blockage_node ready. ADA {ADA_SECTION}. "
            f"min width {ADA_403_5_MIN_WIDTH_M:.4f} m."
        )

    # ------------------------------------------------------------------

    def _on_map(self, msg: OccupancyGrid) -> None:
        w, h = msg.info.width, msg.info.height
        if w == 0 or h == 0:
            return
        grid = np.asarray(msg.data, dtype=np.int16).reshape(h, w)
        self._map_resolution = float(msg.info.resolution)
        self._map_origin = (float(msg.info.origin.position.x),
                            float(msg.info.origin.position.y))
        try:
            self._map_clearance = compute_corridor_clearance(
                grid, resolution_m=self._map_resolution
            )
        except Exception as exc:  # pragma: no cover
            self.get_logger().warn(f"medial-axis failed: {exc}")
            self._map_clearance = None

    def _on_odom(self, msg: Odometry) -> None:
        self._latest_odom = msg

    def _on_scan(self, msg: LaserScan) -> None:
        if not self.get_parameter("enabled").value:
            return
        if self._latest_odom is None:
            return
        if self._map_clearance is None:
            return

        scan_xy = polar_to_xy(
            msg.ranges,
            angle_min_rad=msg.angle_min,
            angle_increment_rad=msg.angle_increment,
            range_min_m=msg.range_min,
            range_max_m=msg.range_max,
        )
        heading = self._heading_from_odom()
        live = scan_min_width_along_line(scan_xy, heading_rad=heading, band_m=0.25)
        if not math.isfinite(live):
            return

        baseline = self._baseline_clearance_at_robot()
        if baseline is None:
            return
        delta = baseline - live
        self._history.append((baseline, live))

        if (live < ADA_403_5_MIN_WIDTH_M
                and delta > BLOCKAGE_DELTA_M
                and self._sustained_blockage()):
            self._emit(baseline, live, delta)
            self._history.clear()

    # ------------------------------------------------------------------

    def _sustained_blockage(self) -> bool:
        if len(self._history) < BLOCKAGE_SUSTAIN_FRAMES:
            return False
        return all(
            (live < ADA_403_5_MIN_WIDTH_M and baseline - live > BLOCKAGE_DELTA_M)
            for baseline, live in self._history
        )

    def _baseline_clearance_at_robot(self) -> float | None:
        if self._map_clearance is None or self._latest_odom is None:
            return None
        origin_x, origin_y = self._map_origin
        res = self._map_resolution
        rx = self._latest_odom.pose.pose.position.x
        ry = self._latest_odom.pose.pose.position.y
        col = int((rx - origin_x) / res)
        row = int((ry - origin_y) / res)
        clearance = self._map_clearance.clearance_m
        h, w = clearance.shape
        if not (0 <= row < h and 0 <= col < w):
            return None
        # Search a small neighborhood for the nearest skeleton cell.
        for radius in range(0, 5):
            r0, r1 = max(0, row - radius), min(h, row + radius + 1)
            c0, c1 = max(0, col - radius), min(w, col + radius + 1)
            patch = clearance[r0:r1, c0:c1]
            if patch.size == 0:
                continue
            mx = float(np.max(patch))
            if mx > 0.0:
                return mx
        return None

    def _heading_from_odom(self) -> float:
        if self._latest_odom is None:
            return 0.0
        q = self._latest_odom.pose.pose.orientation
        # 2D yaw from quaternion
        siny = 2.0 * (q.w * q.z + q.x * q.y)
        cosy = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
        return math.atan2(siny, cosy)

    # ------------------------------------------------------------------

    def _emit(self, baseline_m: float, live_m: float, delta_m: float) -> None:
        if self._latest_odom is None:
            return
        if self._pub is not None and HAS_MSGS:
            msg = PathBlockage()
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = "map"
            ps = PoseStamped()
            ps.header = msg.header
            ps.pose = self._latest_odom.pose.pose
            msg.blockage_pose_map_frame = ps
            msg.map_free_width_m = float(baseline_m)
            msg.live_free_width_m = float(live_m)
            msg.blockage_delta_m = float(delta_m)
            msg.flagged_violation = True
            msg.ada_section = ADA_SECTION
            msg.cloud_snippet_path = ""
            self._pub.publish(msg)
        self.get_logger().info(
            f"ADA 403.5 path blockage: baseline={baseline_m:.2f} m "
            f"live={live_m:.2f} m delta={delta_m:.2f} m"
        )
        self._append_csv(baseline_m, live_m, delta_m)

    def _append_csv(self, baseline_m: float, live_m: float, delta_m: float) -> None:
        now = time.gmtime()
        p = self._latest_odom.pose.pose.position
        with VALIDATION_CSV.open("a", newline="") as f:
            csv.writer(f).writerow([
                f"BLK-{int(time.time())}",
                time.strftime("%Y-%m-%d", now),
                time.strftime("%H:%M:%S", now),
                "AUTO",
                "live-run",
                f"{p.x:.3f}",
                f"{p.y:.3f}",
                f"{baseline_m:.3f}",
                f"{live_m:.3f}",
                f"{delta_m:.3f}",
                "yes",
                ADA_SECTION,
            ])

    def _ensure_csv_header(self) -> None:
        VALIDATION_CSV.parent.mkdir(parents=True, exist_ok=True)
        if not VALIDATION_CSV.exists() or VALIDATION_CSV.stat().st_size == 0:
            with VALIDATION_CSV.open("w", newline="") as f:
                csv.writer(f).writerow([
                    "run_id", "date", "time", "operator", "location",
                    "x_m", "y_m",
                    "map_free_width_m", "live_free_width_m", "delta_m",
                    "flagged", "ada_section",
                ])


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = PathBlockageNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
