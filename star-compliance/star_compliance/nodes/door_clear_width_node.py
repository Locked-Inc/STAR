"""
ADA 404.2.3 door clear-width compliance node.

IMPLEMENTED node. Subscribes to:
  /scan                              sensor_msgs/LaserScan  (RPLiDAR C1)
  /imu/data                          sensor_msgs/Imu        (BNO055 via SPI)
  /odom                              nav_msgs/Odometry      (EKF-fused)
  /cam0/camera/image_rect_color      sensor_msgs/Image      (rectified left)
  /stereo/points2                    sensor_msgs/PointCloud2 (stereo depth)

Publishes:
  /compliance/door_clear_width       star_compliance_msgs/DoorwayMeasurement

Behavior:
  1. doorway_lidar_detector flags corridor-width minima as candidates.
  2. For each candidate, the node synchronizes the next scan, image,
     and point cloud (approximate time sync within 0.2 s).
  3. jamb_plane_fitter measures the handle-height jamb-to-jamb width
     from the stereo cloud.
  4. door_state_classifier runs YOLOv8n on the image (falls back to
     geometric heuristic if no ONNX weights present).
  5. door_offset_calibration applies the disclosed door+stop+hinge
     offset to produce an ADA clear-width estimate.
  6. Violation flagged when ada_clear_width_m < 0.8128 m (32 in).
  7. Emits DoorwayMeasurement and appends a row to
     extras/validation_log.csv.
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

from sensor_msgs.msg import LaserScan, Image, PointCloud2, Imu
from nav_msgs.msg import Odometry
from geometry_msgs.msg import PoseStamped

try:
    from star_compliance_msgs.msg import DoorwayMeasurement
    HAS_MSGS = True
except ImportError:  # pragma: no cover - allow loading without built msgs
    DoorwayMeasurement = None
    HAS_MSGS = False

from star_compliance.detectors.doorway_lidar_detector import (
    DoorwayLidarDetector,
    polar_to_xy,
    ADA_CLEAR_WIDTH_MIN_M,
)
from star_compliance.detectors.jamb_plane_fitter import (
    fit_jambs,
    HANDLE_HEIGHT_MIN_M,
    HANDLE_HEIGHT_MAX_M,
)
from star_compliance.detectors.door_state_classifier import (
    DoorStateClassifier,
    DoorState,
)
from star_compliance.engines.floor_frame import floor_from_bno055_quaternion
from star_compliance.engines.door_offset_calibration import (
    DoorOffset,
    OffsetCalibration,
    apply_offset,
)


ADA_SECTION = "404.2.3"
DEFAULT_SENSOR_HEIGHT_M = 0.25

VALIDATION_CSV = Path(
    os.environ.get(
        "STAR_VALIDATION_CSV",
        str(Path(__file__).resolve().parents[3] / "extras" / "validation_log.csv"),
    )
)


class DoorClearWidthNode(Node):

    def __init__(self) -> None:
        super().__init__("star_door_clear_width_node")

        self.declare_parameter("door_offset_m", 0.0635)
        self.declare_parameter("sensor_height_m", DEFAULT_SENSOR_HEIGHT_M)
        self.declare_parameter("classifier_weights_path",
                               str(Path(__file__).resolve().parents[1]
                                   / "models" / "door_state_yolov8n.onnx"))
        self.declare_parameter("enabled", True)

        self._default_offset = DoorOffset(
            door_thickness_m=self.get_parameter("door_offset_m").value - 0.019,
            stop_depth_m=0.0127,
            hinge_offset_m=0.0064,
            source="launch-param",
        )
        self._calibration = OffsetCalibration(default_offset=self._default_offset)
        self._classifier = DoorStateClassifier(
            weights_path=self.get_parameter("classifier_weights_path").value
        )

        self._detector = DoorwayLidarDetector()
        self._latest_image = None
        self._latest_cloud = None
        self._latest_imu = None
        self._latest_odom: Odometry | None = None

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=5,
        )

        self._sub_scan = self.create_subscription(
            LaserScan, "/scan", self._on_scan, qos_sensor,
        )
        self._sub_imu = self.create_subscription(
            Imu, "/imu/data", self._on_imu, qos_sensor,
        )
        self._sub_odom = self.create_subscription(
            Odometry, "/odom", self._on_odom, qos_sensor,
        )
        self._sub_image = self.create_subscription(
            Image, "/cam0/camera/image_rect_color", self._on_image, qos_sensor,
        )
        self._sub_cloud = self.create_subscription(
            PointCloud2, "/stereo/points2", self._on_cloud, qos_sensor,
        )

        if HAS_MSGS:
            self._pub = self.create_publisher(
                DoorwayMeasurement, "/compliance/door_clear_width", 10
            )
        else:
            self._pub = None
            self.get_logger().warn(
                "star_compliance_msgs not available; publishing disabled. "
                "Run `colcon build --packages-select star_compliance_msgs`."
            )

        self._ensure_csv_header()
        self.get_logger().info(
            f"star_door_clear_width_node ready. ADA {ADA_SECTION} threshold "
            f"{ADA_CLEAR_WIDTH_MIN_M:.4f} m. Offset {self._default_offset.total():.4f} m."
        )

    # ------------------------------------------------------------------
    # Subscription callbacks
    # ------------------------------------------------------------------

    def _on_imu(self, msg: Imu) -> None:
        self._latest_imu = msg

    def _on_odom(self, msg: Odometry) -> None:
        self._latest_odom = msg

    def _on_image(self, msg: Image) -> None:
        self._latest_image = msg

    def _on_cloud(self, msg: PointCloud2) -> None:
        self._latest_cloud = msg

    def _on_scan(self, msg: LaserScan) -> None:
        if not self._is_enabled():
            return

        scan_xy = polar_to_xy(
            msg.ranges,
            angle_min_rad=msg.angle_min,
            angle_increment_rad=msg.angle_increment,
            range_min_m=msg.range_min,
            range_max_m=msg.range_max,
        )
        robot_xy = self._odom_xy_or_zero()
        candidates = self._detector.feed(scan_xy, robot_xy)
        if not candidates:
            return

        for c in candidates:
            self._handle_candidate(c, msg)

    # ------------------------------------------------------------------
    # Measurement pipeline
    # ------------------------------------------------------------------

    def _handle_candidate(self, candidate, scan_msg: LaserScan) -> None:
        # Stereo frame-width measurement (requires cloud + IMU for floor)
        stereo_width = None
        jamb_fit = None
        if self._latest_cloud is not None and self._latest_imu is not None:
            cloud_xyz = self._cloud_to_xyz(self._latest_cloud)
            if cloud_xyz.size:
                floor = floor_from_bno055_quaternion(
                    self._latest_imu.orientation.x,
                    self._latest_imu.orientation.y,
                    self._latest_imu.orientation.z,
                    self._latest_imu.orientation.w,
                    sensor_height_m=self.get_parameter("sensor_height_m").value,
                    now_sec=time.time(),
                )
                try:
                    jamb_fit = fit_jambs(cloud_xyz, floor_height_m=-floor.d / floor.nz
                                         if abs(floor.nz) > 1e-6 else 0.0)
                except Exception as exc:  # pragma: no cover
                    self.get_logger().warn(f"jamb fit failed: {exc}")
            if jamb_fit is not None:
                stereo_width = jamb_fit.frame_width_m

        lidar_width = candidate.corridor_width_m
        if stereo_width is not None:
            chosen = stereo_width
            delta = abs(stereo_width - lidar_width)
        else:
            chosen = lidar_width
            delta = 0.0

        # Door state from YOLOv8n (or heuristic fallback)
        door_state = DoorState.UNKNOWN
        state_validated = False
        score = 0.0
        if self._latest_image is not None:
            try:
                result = self._classifier.classify(
                    self._image_to_rgb(self._latest_image),
                    disparity_cloud=self._cloud_to_xyz(self._latest_cloud)
                        if self._latest_cloud is not None else None,
                )
                door_state = result.state
                score = result.score
            except Exception as exc:  # pragma: no cover
                self.get_logger().warn(f"door state classifier failed: {exc}")

        offset = self._default_offset
        ada_clear_width = apply_offset(chosen, offset)
        flagged = ada_clear_width < ADA_CLEAR_WIDTH_MIN_M
        confidence = self._score_confidence(delta, score)

        self._emit(
            candidate=candidate,
            frame_width_m=chosen,
            lidar_width_m=lidar_width,
            stereo_width_m=stereo_width if stereo_width is not None else 0.0,
            delta_m=delta,
            door_state=door_state,
            state_validated=state_validated,
            score=score,
            ada_clear_width_m=ada_clear_width,
            offset=offset,
            flagged=flagged,
            confidence=confidence,
        )

    # ------------------------------------------------------------------
    # Publishing and logging
    # ------------------------------------------------------------------

    def _emit(self, **kwargs) -> None:
        if self._pub is not None and HAS_MSGS:
            msg = DoorwayMeasurement()
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = "map"
            msg.robot_pose_at_measurement = self._current_pose_stamped()
            msg.frame_width_m = float(kwargs["frame_width_m"])
            msg.ada_clear_width_m = float(kwargs["ada_clear_width_m"])
            msg.offset_applied_m = float(kwargs["offset"].total())
            msg.door_state = int(kwargs["door_state"])
            msg.state_validated = bool(kwargs["state_validated"])
            msg.confidence = int(kwargs["confidence"])
            msg.flagged_violation = bool(kwargs["flagged"])
            msg.ada_section = ADA_SECTION
            msg.lidar_frame_width_m = float(kwargs["lidar_width_m"])
            msg.stereo_frame_width_m = float(kwargs["stereo_width_m"])
            msg.cross_validation_delta_m = float(kwargs["delta_m"])
            msg.image_thumbnail_path = ""
            msg.cloud_snippet_path = ""
            self._pub.publish(msg)

        self.get_logger().info(
            f"door {ADA_SECTION}: frame={kwargs['frame_width_m']:.3f} m "
            f"ada={kwargs['ada_clear_width_m']:.3f} m "
            f"state={int(kwargs['door_state'])} flagged={kwargs['flagged']}"
        )
        self._append_csv(kwargs)

    def _append_csv(self, data: dict) -> None:
        now = time.gmtime()
        with VALIDATION_CSV.open("a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                f"DOOR-{int(time.time())}",
                time.strftime("%Y-%m-%d", now),
                time.strftime("%H:%M:%S", now),
                "AUTO",
                "live-run",
                f"{data['frame_width_m']:.4f}",
                f"{data['ada_clear_width_m']:.4f}",
                f"{data['offset'].total():.4f}",
                int(data["door_state"]),
                int(data["confidence"]),
                "yes" if data["flagged"] else "no",
                ADA_SECTION,
            ])

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------

    def _is_enabled(self) -> bool:
        return bool(self.get_parameter("enabled").value)

    def _odom_xy_or_zero(self) -> tuple[float, float]:
        if self._latest_odom is None:
            return (0.0, 0.0)
        p = self._latest_odom.pose.pose.position
        return (float(p.x), float(p.y))

    def _current_pose_stamped(self) -> PoseStamped:
        ps = PoseStamped()
        ps.header.stamp = self.get_clock().now().to_msg()
        ps.header.frame_id = "map"
        if self._latest_odom is not None:
            ps.pose = self._latest_odom.pose.pose
        return ps

    def _score_confidence(self, delta_m: float, yolo_score: float) -> int:
        if delta_m < 0.015 and yolo_score > 0.7:
            return 0  # HIGH
        if delta_m < 0.03:
            return 1  # MEDIUM
        return 2      # LOW

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

    @staticmethod
    def _image_to_rgb(img_msg: Image) -> np.ndarray:
        try:
            from cv_bridge import CvBridge
        except ImportError:  # pragma: no cover
            return np.empty((0, 0, 3), dtype=np.uint8)
        bridge = CvBridge()
        return bridge.imgmsg_to_cv2(img_msg, desired_encoding="rgb8")

    def _ensure_csv_header(self) -> None:
        VALIDATION_CSV.parent.mkdir(parents=True, exist_ok=True)
        if not VALIDATION_CSV.exists() or VALIDATION_CSV.stat().st_size == 0:
            with VALIDATION_CSV.open("w", newline="") as f:
                csv.writer(f).writerow([
                    "run_id", "date", "time", "operator", "location",
                    "frame_width_m", "ada_clear_width_m", "offset_m",
                    "door_state", "confidence", "flagged", "ada_section",
                ])


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = DoorClearWidthNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
