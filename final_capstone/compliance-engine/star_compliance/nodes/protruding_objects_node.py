"""
ADA 307 Protruding Objects compliance node.

Subscribes to:
  /cloud_map    sensor_msgs/PointCloud2 (RTAB-Map consolidated, 1 Hz)
                 - configurable via the `input_cloud_topic` parameter;
                   /stereo/points2 also works at higher rate with more
                   CPU cost.
  /imu/data     sensor_msgs/Imu (BNO055, for floor-plane anchor)

Publishes:
  /compliance/protruding_objects   star_compliance_msgs/ProtrudingObjectArray

Runtime disable:
  - launch arg `use_ada_307:=false` skips creation of this node.
  - parameter `/star_protruding_objects_node/enabled` can be toggled
    live (e.g., by the CPU safety-net monitor) to short-circuit the
    heavy RANSAC work while leaving the node subscribed.

Target CPU load: <= 70% on Pi 5 with the default config; the safety-
net monitor auto-disables when load exceeds 80% for 1 minute.
"""

from __future__ import annotations

import csv
import os
import time
from pathlib import Path

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from sensor_msgs.msg import PointCloud2, Imu

try:
    from star_compliance_msgs.msg import ProtrudingObject, ProtrudingObjectArray
    HAS_MSGS = True
except ImportError:  # pragma: no cover
    ProtrudingObject = None
    ProtrudingObjectArray = None
    HAS_MSGS = False

from star_compliance.detectors.cane_zone_filter import (
    ADA_307_PROTRUSION_LIMIT_M,
    CANE_ZONE_MAX_M,
    CANE_ZONE_MIN_M,
    filter_to_cane_zone,
    is_protrusion,
)
from star_compliance.detectors.wall_plane_fitter import (
    fit_walls,
    nearest_wall,
)
from star_compliance.engines.floor_frame import floor_from_bno055_quaternion


ADA_SECTION = "307"
DEFAULT_SENSOR_HEIGHT_M = 0.25
DEFAULT_MIN_CLUSTER_POINTS = 5


VALIDATION_CSV = Path(
    os.environ.get(
        "STAR_PROTRUSION_CSV",
        str(Path(__file__).resolve().parents[3] / "extras" / "protrusion_log.csv"),
    )
)


class ProtrudingObjectsNode(Node):

    def __init__(self) -> None:
        super().__init__("star_protruding_objects_node")

        self.declare_parameter("enabled", True)
        self.declare_parameter("input_cloud_topic", "/cloud_map")
        self.declare_parameter("sensor_height_m", DEFAULT_SENSOR_HEIGHT_M)
        self.declare_parameter("min_cluster_points", DEFAULT_MIN_CLUSTER_POINTS)

        self._latest_imu: Imu | None = None
        self._latest_cloud: PointCloud2 | None = None
        self._last_process_sec: float = 0.0

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=2,
        )

        self._sub_cloud = self.create_subscription(
            PointCloud2,
            self.get_parameter("input_cloud_topic").value,
            self._on_cloud,
            qos_sensor,
        )
        self._sub_imu = self.create_subscription(
            Imu, "/imu/data", self._on_imu, qos_sensor,
        )

        if HAS_MSGS:
            self._pub = self.create_publisher(
                ProtrudingObjectArray, "/compliance/protruding_objects", 10
            )
        else:
            self._pub = None
            self.get_logger().warn(
                "star_compliance_msgs unavailable; publishing disabled."
            )

        self._ensure_csv_header()
        # Timer: limit cloud processing to 1 Hz to cap CPU.
        self._timer = self.create_timer(1.0, self._process_if_ready)

        self.get_logger().info(
            f"star_protruding_objects_node ready. ADA {ADA_SECTION}. "
            f"Cane zone {CANE_ZONE_MIN_M:.3f} to {CANE_ZONE_MAX_M:.3f} m. "
            f"Protrusion threshold {ADA_307_PROTRUSION_LIMIT_M:.3f} m."
        )

    # ------------------------------------------------------------------

    def _on_cloud(self, msg: PointCloud2) -> None:
        self._latest_cloud = msg

    def _on_imu(self, msg: Imu) -> None:
        self._latest_imu = msg

    def _process_if_ready(self) -> None:
        if not self.get_parameter("enabled").value:
            return
        if self._latest_cloud is None or self._latest_imu is None:
            return
        now = self.get_clock().now().nanoseconds * 1e-9
        # Rate-limit to the timer period; the timer already gates
        # at 1 Hz but this guards against callback overruns.
        if now - self._last_process_sec < 0.9:
            return
        self._last_process_sec = now
        self._process_cloud(self._latest_cloud)

    def _process_cloud(self, cloud: PointCloud2) -> None:
        xyz = self._cloud_to_xyz(cloud)
        if xyz.size == 0:
            return

        floor = floor_from_bno055_quaternion(
            self._latest_imu.orientation.x,
            self._latest_imu.orientation.y,
            self._latest_imu.orientation.z,
            self._latest_imu.orientation.w,
            sensor_height_m=self.get_parameter("sensor_height_m").value,
            now_sec=time.time(),
        )
        floor_height_m = (-floor.d / floor.nz) if abs(floor.nz) > 1e-6 else 0.0

        band = filter_to_cane_zone(xyz, floor_height_m)
        if band.shape[0] < self.get_parameter("min_cluster_points").value:
            return

        walls = fit_walls(band)
        if not walls:
            return

        objects = []
        for row in band:
            wall, distance = nearest_wall(row, walls)
            if wall is None:
                continue
            if not is_protrusion(distance,
                                 float(row[2] - floor_height_m),
                                 point_count=1,
                                 min_points=1):
                continue
            objects.append({
                "x": float(row[0]),
                "y": float(row[1]),
                "z": float(row[2]),
                "distance": float(distance),
                "height_above_floor": float(row[2] - floor_height_m),
                "wall_d": float(wall.d),
            })

        if not objects:
            return
        self._emit(objects)

    # ------------------------------------------------------------------

    def _emit(self, objects: list[dict]) -> None:
        if self._pub is not None and HAS_MSGS:
            arr = ProtrudingObjectArray()
            arr.header.stamp = self.get_clock().now().to_msg()
            arr.header.frame_id = "map"
            for o in objects:
                p = ProtrudingObject()
                p.header = arr.header
                p.object_pose_map_frame.header = arr.header
                p.object_pose_map_frame.pose.position.x = o["x"]
                p.object_pose_map_frame.pose.position.y = o["y"]
                p.object_pose_map_frame.pose.position.z = o["z"]
                p.object_pose_map_frame.pose.orientation.w = 1.0
                p.protrusion_depth_m = o["distance"]
                p.height_above_floor_m = o["height_above_floor"]
                p.cluster_extent_m = 0.0
                p.cluster_point_count = 1
                p.wall_plane_distance_m = o["wall_d"]
                p.flagged_violation = True
                p.ada_section = ADA_SECTION
                p.cloud_snippet_path = ""
                arr.objects.append(p)
            self._pub.publish(arr)
        self.get_logger().info(
            f"ADA 307: {len(objects)} candidate protrusion(s) flagged."
        )
        self._append_csv(objects)

    def _append_csv(self, objects: list[dict]) -> None:
        now = time.gmtime()
        with VALIDATION_CSV.open("a", newline="") as f:
            writer = csv.writer(f)
            for o in objects:
                writer.writerow([
                    f"PROT-{int(time.time() * 1000)}",
                    time.strftime("%Y-%m-%d", now),
                    time.strftime("%H:%M:%S", now),
                    "AUTO",
                    "live-run",
                    f"{o['x']:.3f}",
                    f"{o['y']:.3f}",
                    f"{o['z']:.3f}",
                    f"{o['distance']:.3f}",
                    f"{o['height_above_floor']:.3f}",
                    "yes",
                    ADA_SECTION,
                ])

    def _ensure_csv_header(self) -> None:
        VALIDATION_CSV.parent.mkdir(parents=True, exist_ok=True)
        if not VALIDATION_CSV.exists() or VALIDATION_CSV.stat().st_size == 0:
            with VALIDATION_CSV.open("w", newline="") as f:
                csv.writer(f).writerow([
                    "run_id", "date", "time", "operator", "location",
                    "x_m", "y_m", "z_m",
                    "protrusion_depth_m", "height_above_floor_m",
                    "flagged", "ada_section",
                ])

    @staticmethod
    def _cloud_to_xyz(cloud: PointCloud2) -> np.ndarray:
        try:
            from sensor_msgs_py import point_cloud2
        except ImportError:  # pragma: no cover
            return np.empty((0, 3))
        points = list(point_cloud2.read_points(
            cloud, field_names=("x", "y", "z"), skip_nans=True,
        ))
        if not points:
            return np.empty((0, 3))
        return np.asarray(points).reshape(-1, 3)


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = ProtrudingObjectsNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
