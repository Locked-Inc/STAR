"""
DBSCAN dynamic-obstacle clusterer node.

Subscribes to:
  /scan    sensor_msgs/LaserScan
  /map     nav_msgs/OccupancyGrid (for background subtraction)
  /odom    nav_msgs/Odometry (to transform scan into map frame)

Publishes:
  /perception/dynamic_obstacles   star_compliance_msgs/DynamicObstacleArray

Not ADA-specific; feeds the path-blockage node and future Nav2
behavior-tree "pause for pedestrian" actions.
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

from star_compliance.detectors.obstacle_clusterer import (
    cluster_points,
    subtract_known_map,
)
from star_compliance.detectors.doorway_lidar_detector import polar_to_xy


class DynamicObstacleNode(Node):

    def __init__(self) -> None:
        super().__init__("star_dynamic_obstacle_node")

        self.declare_parameter("enabled", True)
        self.declare_parameter("eps_m", 0.15)
        self.declare_parameter("min_samples", 5)

        self._latest_map: OccupancyGrid | None = None
        self._latest_odom: Odometry | None = None

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
        arr.header.frame_id = "map"
        for c in clusters:
            obs = DynamicObstacle()
            obs.header = arr.header
            obs.cluster_pose_map_frame.header = arr.header
            obs.cluster_pose_map_frame.pose.position.x = c.centroid_xy[0]
            obs.cluster_pose_map_frame.pose.position.y = c.centroid_xy[1]
            obs.cluster_pose_map_frame.pose.orientation.w = 1.0
            obs.cluster_radius_m = float(c.radius_m)
            obs.point_count = int(c.point_count)
            obs.confidence = float(c.confidence)
            arr.obstacles.append(obs)
        self._pub.publish(arr)

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
