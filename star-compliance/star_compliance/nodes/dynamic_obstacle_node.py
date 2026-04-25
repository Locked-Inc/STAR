"""
DBSCAN dynamic-obstacle clusterer node.

Subscribes to:
  /scan                     sensor_msgs/LaserScan
  /map                      nav_msgs/OccupancyGrid (background subtraction)
  /odom                     nav_msgs/Odometry (transform scan into map frame)
  /perception/detections_3d  vision_msgs/Detection3DArray (Hailo YOLO 3D
                            detections in cam0_optical_frame, optional)

Publishes:
  /perception/dynamic_obstacles   star_compliance_msgs/DynamicObstacleArray

Not ADA-specific; feeds the path-blockage node and future Nav2
behavior-tree "pause for pedestrian" actions.

Fusion behaviour
----------------
When YOLO 3D detections are available, the node fuses them with the
LiDAR DBSCAN clusters:
  * Cluster + matching YOLO  -> source = "yolo+lidar", confidence = 1.0
  * Cluster only             -> source = "lidar"
  * YOLO `person` only       -> source = "yolo", confidence = 0.6
                                (e.g. behind glass, beyond LiDAR)
"""

from __future__ import annotations

import math

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from sensor_msgs.msg import LaserScan
from nav_msgs.msg import OccupancyGrid, Odometry

try:
    from star_compliance_msgs.msg import (
        DynamicObstacle,
        DynamicObstacleArray,
    )
    HAS_MSGS = True
except ImportError:  # pragma: no cover
    DynamicObstacle = None
    DynamicObstacleArray = None
    HAS_MSGS = False

try:
    from vision_msgs.msg import Detection3DArray
    HAS_VISION_MSGS = True
except ImportError:  # pragma: no cover
    Detection3DArray = None
    HAS_VISION_MSGS = False

try:
    from tf2_ros import Buffer, TransformListener
    HAS_TF2 = True
except ImportError:  # pragma: no cover
    Buffer = None
    TransformListener = None
    HAS_TF2 = False

from star_compliance.detectors.cane_zone_filter import (
    DEFAULT_CLASS_MATCH_RADIUS_M,
)
from star_compliance.detectors.obstacle_clusterer import (
    cluster_points,
    subtract_known_map,
)
from star_compliance.detectors.doorway_lidar_detector import polar_to_xy


MAP_FRAME = "map"
PERSON_CLASS = "person"
LIDAR_ONLY_CONFIDENCE = None  # Use whatever DBSCAN reports.
YOLO_ONLY_PERSON_CONFIDENCE = 0.6
YOLO_PLUS_LIDAR_CONFIDENCE = 1.0
DEFAULT_YOLO_CACHE_SECONDS = 1.0


class DynamicObstacleNode(Node):

    def __init__(self) -> None:
        super().__init__("star_dynamic_obstacle_node")

        self.declare_parameter("enabled", True)
        self.declare_parameter("eps_m", 0.15)
        self.declare_parameter("min_samples", 5)
        self.declare_parameter(
            "class_match_radius_m", DEFAULT_CLASS_MATCH_RADIUS_M)
        self.declare_parameter("yolo_cache_seconds", DEFAULT_YOLO_CACHE_SECONDS)

        self._latest_map: OccupancyGrid | None = None
        self._latest_odom: Odometry | None = None
        # Each entry: (received_sec, x_map, y_map, class_label, score).
        self._yolo_cache: list[tuple[float, float, float, str, float]] = []

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=5,
        )
        self._sub_scan = self.create_subscription(
            LaserScan, "/scan", self._on_scan, qos_sensor
        )
        self._sub_map = self.create_subscription(
            OccupancyGrid, "/map", self._on_map, 10
        )
        self._sub_odom = self.create_subscription(
            Odometry, "/odom", self._on_odom, qos_sensor
        )

        if HAS_MSGS:
            self._pub = self.create_publisher(
                DynamicObstacleArray, "/perception/dynamic_obstacles", 10
            )
        else:
            self._pub = None

        if HAS_VISION_MSGS and HAS_TF2:
            self._tf_buffer: Buffer | None = Buffer()
            self._tf_listener = TransformListener(self._tf_buffer, self)
            self._sub_yolo = self.create_subscription(
                Detection3DArray,
                "/perception/detections_3d",
                self._on_yolo,
                qos_sensor,
            )
        else:  # pragma: no cover
            self._tf_buffer = None
            self._tf_listener = None
            self._sub_yolo = None

        self.get_logger().info("star_dynamic_obstacle_node ready.")

    def _on_map(self, msg: OccupancyGrid) -> None:
        self._latest_map = msg

    def _on_odom(self, msg: Odometry) -> None:
        self._latest_odom = msg

    def _on_scan(self, msg: LaserScan) -> None:
        if not self.get_parameter("enabled").value:
            return
        if self._latest_odom is None or self._latest_map is None:
            return

        scan_xy = polar_to_xy(
            msg.ranges,
            angle_min_rad=msg.angle_min,
            angle_increment_rad=msg.angle_increment,
            range_min_m=msg.range_min,
            range_max_m=msg.range_max,
        )
        if scan_xy.size == 0:
            return

        map_xy = self._scan_to_map(scan_xy)
        occ = self._map_as_np()
        origin = (self._latest_map.info.origin.position.x,
                  self._latest_map.info.origin.position.y)
        res = float(self._latest_map.info.resolution)
        dynamic = subtract_known_map(map_xy, occ, origin, res)

        clusters = cluster_points(
            dynamic,
            eps_m=float(self.get_parameter("eps_m").value),
            min_samples=int(self.get_parameter("min_samples").value),
        )
        if not clusters:
            return
        self._emit(clusters)

    def _emit(self, clusters) -> None:
        if self._pub is None or not HAS_MSGS:
            return
        arr = DynamicObstacleArray()
        arr.header.stamp = self.get_clock().now().to_msg()
        arr.header.frame_id = MAP_FRAME

        radius_m = float(self.get_parameter("class_match_radius_m").value)
        matched_yolo_idx: set[int] = set()

        for c in clusters:
            cx, cy = float(c.centroid_xy[0]), float(c.centroid_xy[1])
            yolo_idx, label, score = self._match_yolo(cx, cy, radius_m)
            obs = DynamicObstacle()
            obs.header = arr.header
            obs.cluster_pose_map_frame.header = arr.header
            obs.cluster_pose_map_frame.pose.position.x = cx
            obs.cluster_pose_map_frame.pose.position.y = cy
            obs.cluster_pose_map_frame.pose.orientation.w = 1.0
            obs.cluster_radius_m = float(c.radius_m)
            obs.point_count = int(c.point_count)
            if yolo_idx is not None:
                obs.confidence = YOLO_PLUS_LIDAR_CONFIDENCE
                obs.source = "yolo+lidar"
                obs.class_label = label
                matched_yolo_idx.add(yolo_idx)
            else:
                obs.confidence = float(c.confidence)
                obs.source = "lidar"
                obs.class_label = ""
            arr.obstacles.append(obs)

        # YOLO-only persons (e.g. behind a glass door, beyond LiDAR
        # range) get their own entries.
        for idx, entry in enumerate(self._yolo_cache):
            if idx in matched_yolo_idx:
                continue
            _, x_map, y_map, label, score = entry
            if label != PERSON_CLASS:
                continue
            obs = DynamicObstacle()
            obs.header = arr.header
            obs.cluster_pose_map_frame.header = arr.header
            obs.cluster_pose_map_frame.pose.position.x = x_map
            obs.cluster_pose_map_frame.pose.position.y = y_map
            obs.cluster_pose_map_frame.pose.orientation.w = 1.0
            obs.cluster_radius_m = 0.30  # Nominal person radius.
            obs.point_count = 0
            obs.confidence = YOLO_ONLY_PERSON_CONFIDENCE
            obs.source = "yolo"
            obs.class_label = label
            arr.obstacles.append(obs)

        self._pub.publish(arr)

    # ------------------------------------------------------------------

    def _on_yolo(self, msg) -> None:
        """Cache YOLO 3D detections in map-frame coordinates."""
        if self._tf_buffer is None:
            return
        try:
            tf = self._tf_buffer.lookup_transform(
                MAP_FRAME, msg.header.frame_id,
                rclpy.time.Time(),
                timeout=rclpy.duration.Duration(seconds=0.05),
            )
        except Exception as exc:  # pragma: no cover - TF state dependent
            self.get_logger().warn(
                f"YOLO TF lookup failed: {exc}",
                throttle_duration_sec=10.0,
            )
            return

        now = self.get_clock().now().nanoseconds * 1e-9
        for d in msg.detections:
            if not d.results:
                continue
            label = d.results[0].hypothesis.class_id
            score = float(d.results[0].hypothesis.score)
            x_map, y_map, _ = self._transform_point(
                d.bbox.center.position.x,
                d.bbox.center.position.y,
                d.bbox.center.position.z,
                tf,
            )
            self._yolo_cache.append((now, x_map, y_map, label, score))
        cache_window = float(
            self.get_parameter("yolo_cache_seconds").value)
        cutoff = now - cache_window
        self._yolo_cache = [e for e in self._yolo_cache if e[0] >= cutoff]

    def _match_yolo(self, x: float, y: float, radius_m: float
                    ) -> tuple[int | None, str, float]:
        """Find the nearest YOLO entry within radius_m. 2D match (XY)."""
        if not self._yolo_cache:
            return None, "", 0.0
        radius_sq = radius_m * radius_m
        best_d2 = float("inf")
        best_idx: int | None = None
        best_label = ""
        best_score = 0.0
        for idx, (_, xm, ym, label, score) in enumerate(self._yolo_cache):
            d2 = (xm - x) ** 2 + (ym - y) ** 2
            if d2 < best_d2 and d2 <= radius_sq:
                best_d2 = d2
                best_idx = idx
                best_label = label
                best_score = score
        return best_idx, best_label, best_score

    @staticmethod
    def _transform_point(x: float, y: float, z: float, tf
                         ) -> tuple[float, float, float]:
        """Apply a TransformStamped to a point. Pure linear algebra."""
        t = tf.transform.translation
        q = tf.transform.rotation
        xx, yy, zz = q.x * q.x, q.y * q.y, q.z * q.z
        xy, xz, yz = q.x * q.y, q.x * q.z, q.y * q.z
        wx, wy, wz = q.w * q.x, q.w * q.y, q.w * q.z
        rx = (1 - 2 * (yy + zz)) * x + 2 * (xy - wz) * y + 2 * (xz + wy) * z
        ry = 2 * (xy + wz) * x + (1 - 2 * (xx + zz)) * y + 2 * (yz - wx) * z
        rz = 2 * (xz - wy) * x + 2 * (yz + wx) * y + (1 - 2 * (xx + yy)) * z
        return rx + t.x, ry + t.y, rz + t.z

    # ------------------------------------------------------------------

    def _scan_to_map(self, scan_xy: np.ndarray) -> np.ndarray:
        if self._latest_odom is None:
            return scan_xy
        p = self._latest_odom.pose.pose.position
        q = self._latest_odom.pose.pose.orientation
        siny = 2.0 * (q.w * q.z + q.x * q.y)
        cosy = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
        yaw = math.atan2(siny, cosy)
        c, s = math.cos(yaw), math.sin(yaw)
        rot = np.column_stack([
            c * scan_xy[:, 0] - s * scan_xy[:, 1] + p.x,
            s * scan_xy[:, 0] + c * scan_xy[:, 1] + p.y,
        ])
        return rot

    def _map_as_np(self) -> np.ndarray:
        info = self._latest_map.info
        return np.asarray(self._latest_map.data, dtype=np.int16).reshape(
            info.height, info.width
        )


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = DynamicObstacleNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
