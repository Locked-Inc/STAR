"""
ADA 307 Protruding Objects compliance node.

Subscribes to:
  /cloud_map               sensor_msgs/PointCloud2 (RTAB-Map consolidated,
                           1 Hz). Configurable via `input_cloud_topic`;
                           /stereo/points2 also works at higher rate with
                           more CPU cost.
  /imu/data                sensor_msgs/Imu (BNO055, for floor-plane anchor)
  /perception/detections_3d  vision_msgs/Detection3DArray (Hailo YOLO 3D
                           detections in cam0_optical_frame, fused via
                           star_perception.detection_3d_fusion_node).
                           When present, each emitted ProtrudingObject is
                           tagged with the nearest YOLO class label, and
                           transient classes (person, chair, backpack,
                           ...) are reported with flagged_violation=false.
                           The node still works without YOLO -- all
                           protrusions are then treated as unclassified
                           and presumed-fixed.

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

try:
    from vision_msgs.msg import Detection3DArray
    HAS_VISION_MSGS = True
except ImportError:  # pragma: no cover
    Detection3DArray = None
    HAS_VISION_MSGS = False

try:
    import tf2_ros
    from tf2_ros import Buffer, TransformListener
    HAS_TF2 = True
except ImportError:  # pragma: no cover
    tf2_ros = None
    Buffer = None
    TransformListener = None
    HAS_TF2 = False

from star_compliance.detectors.cane_zone_filter import (
    ADA_307_PROTRUSION_LIMIT_M,
    CANE_ZONE_MAX_M,
    CANE_ZONE_MIN_M,
    DEFAULT_CLASS_MATCH_RADIUS_M,
    filter_to_cane_zone,
    is_protrusion,
    is_transient_class,
)
from star_compliance.detectors.wall_plane_fitter import (
    fit_walls,
    nearest_wall,
)
from star_compliance.engines.floor_frame import floor_from_bno055_quaternion


ADA_SECTION = "307"
DEFAULT_SENSOR_HEIGHT_M = 0.25
DEFAULT_MIN_CLUSTER_POINTS = 5
DEFAULT_YOLO_CACHE_SECONDS = 1.0
MAP_FRAME = "map"


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
        self.declare_parameter(
            "class_match_radius_m", DEFAULT_CLASS_MATCH_RADIUS_M)
        self.declare_parameter("yolo_cache_seconds", DEFAULT_YOLO_CACHE_SECONDS)

        self._latest_imu: Imu | None = None
        self._latest_cloud: PointCloud2 | None = None
        self._last_process_sec: float = 0.0
        # Each entry: (received_sec, x_map, y_map, z_map, class_label, score).
        self._yolo_cache: list[tuple[float, float, float, float, str, float]] = []

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

        # Optional YOLO 3D detection feed. Subscribed when both
        # vision_msgs and tf2 are available; otherwise the node falls
        # back to unclassified protrusions.
        if HAS_VISION_MSGS and HAS_TF2:
            self._tf_buffer: Buffer | None = Buffer()
            self._tf_listener = TransformListener(self._tf_buffer, self)
            self._sub_yolo = self.create_subscription(
                Detection3DArray,
                "/perception/detections_3d",
                self._on_yolo,
                qos_sensor,
            )
        else:  # pragma: no cover - integration test path
            self._tf_buffer = None
            self._tf_listener = None
            self._sub_yolo = None

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

    def _on_yolo(self, msg) -> None:
        """Cache YOLO 3D detections in map-frame for later matching."""
        if self._tf_buffer is None:
            return
        try:
            tf = self._tf_buffer.lookup_transform(
                MAP_FRAME, msg.header.frame_id,
                rclpy.time.Time(),
                timeout=rclpy.duration.Duration(seconds=0.05),
            )
        except Exception as exc:  # pragma: no cover - depends on TF tree
            self.get_logger().warn(
                f"YOLO TF lookup failed ({msg.header.frame_id} -> "
                f"{MAP_FRAME}): {exc}",
                throttle_duration_sec=10.0,
            )
            return

        now = self.get_clock().now().nanoseconds * 1e-9
        for d in msg.detections:
            if not d.results:
                continue
            class_label = d.results[0].hypothesis.class_id
            score = float(d.results[0].hypothesis.score)
            x_map, y_map, z_map = self._transform_point(
                d.bbox.center.position.x,
                d.bbox.center.position.y,
                d.bbox.center.position.z,
                tf,
            )
            self._yolo_cache.append(
                (now, x_map, y_map, z_map, class_label, score)
            )
        self._evict_stale_yolo(now)

    def _evict_stale_yolo(self, now_sec: float) -> None:
        cache_window = float(self.get_parameter("yolo_cache_seconds").value)
        cutoff = now_sec - cache_window
        self._yolo_cache = [
            entry for entry in self._yolo_cache if entry[0] >= cutoff
        ]

    @staticmethod
    def _transform_point(x: float, y: float, z: float, tf
                         ) -> tuple[float, float, float]:
        """Apply a TransformStamped to a point. Pure linear algebra."""
        t = tf.transform.translation
        q = tf.transform.rotation
        # Rotation matrix from quaternion (q.x, q.y, q.z, q.w).
        xx, yy, zz = q.x * q.x, q.y * q.y, q.z * q.z
        xy, xz, yz = q.x * q.y, q.x * q.z, q.y * q.z
        wx, wy, wz = q.w * q.x, q.w * q.y, q.w * q.z
        rx = (1 - 2 * (yy + zz)) * x + 2 * (xy - wz) * y + 2 * (xz + wy) * z
        ry = 2 * (xy + wz) * x + (1 - 2 * (xx + zz)) * y + 2 * (yz - wx) * z
        rz = 2 * (xz - wy) * x + 2 * (yz + wx) * y + (1 - 2 * (xx + yy)) * z
        return rx + t.x, ry + t.y, rz + t.z

    def _match_yolo(self, x: float, y: float, z: float
                    ) -> tuple[str, float]:
        """Return (class_label, score) of the nearest YOLO detection."""
        if not self._yolo_cache:
            return "", 0.0
        radius = float(self.get_parameter("class_match_radius_m").value)
        radius_sq = radius * radius
        best_d2 = float("inf")
        best_label = ""
        best_score = 0.0
        for _, xm, ym, zm, label, score in self._yolo_cache:
            d2 = (xm - x) ** 2 + (ym - y) ** 2 + (zm - z) ** 2
            if d2 < best_d2 and d2 <= radius_sq:
                best_d2 = d2
                best_label = label
                best_score = score
        return best_label, best_score

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
            x, y, z = float(row[0]), float(row[1]), float(row[2])
            class_label, class_score = self._match_yolo(x, y, z)
            objects.append({
                "x": x,
                "y": y,
                "z": z,
                "distance": float(distance),
                "height_above_floor": float(z - floor_height_m),
                "wall_d": float(wall.d),
                "class_label": class_label,
                "class_confidence": class_score,
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
                p.class_label = o["class_label"]
                p.class_confidence = float(o["class_confidence"])
                # Transient YOLO classes (person, chair, ...) are not
                # ADA 307 violations -- the standard targets *fixed*
                # protrusions. Empty class_label keeps the legacy
                # presumed-fixed behaviour.
                p.flagged_violation = not is_transient_class(
                    o["class_label"])
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
