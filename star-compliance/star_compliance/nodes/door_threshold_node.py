"""
ADA 404.2.5 door-threshold presence-detection node.

Subscribes to:
  /imu/data                          sensor_msgs/Imu (BNO055, 200 Hz)
  /odom                              nav_msgs/Odometry
  /compliance/door_clear_width       star_compliance_msgs/DoorwayMeasurement

Publishes:
  /compliance/door_threshold         star_compliance_msgs/ThresholdMeasurement

Behavior:
  When a DoorwayMeasurement lands, start a 2 s "threshold watch." The
  ImuJoltDetector feeds every IMU sample during the watch; a jolt of
  magnitude > 2.0 m/s^2 sustained for 25 ms fires a ThresholdMeasurement
  with detected=True. Absence of jolt is ADA-compliant.

STAR reports presence only; precise height is not measured. The
disclosure is mandatory on the PDF audit report.
"""

from __future__ import annotations

import csv
import os
import time
from pathlib import Path

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from sensor_msgs.msg import Imu
from nav_msgs.msg import Odometry
from geometry_msgs.msg import PoseStamped

try:
    from star_compliance_msgs.msg import (
        DoorwayMeasurement,
        ThresholdMeasurement,
    )
    HAS_MSGS = True
except ImportError:  # pragma: no cover
    DoorwayMeasurement = None
    ThresholdMeasurement = None
    HAS_MSGS = False

from star_compliance.detectors.imu_jolt_detector import ImuJoltDetector


ADA_SECTION = "404.2.5"
DISCLOSURE = (
    "threshold presence detected via IMU jolt; precise height not measured "
    "by this version of STAR; CASp follow-up with a depth gauge required."
)
DETECTION_METHOD = "bno055_jolt_odometry"

VALIDATION_CSV = Path(
    os.environ.get(
        "STAR_THRESHOLD_CSV",
        str(Path(__file__).resolve().parents[3] / "extras" / "threshold_log.csv"),
    )
)


class DoorThresholdNode(Node):

    def __init__(self) -> None:
        super().__init__("star_door_threshold_node")

        self.declare_parameter("threshold_ms2", 2.0)
        self.declare_parameter("watch_duration_sec", 2.0)
        self.declare_parameter("enabled", True)

        self._detector = ImuJoltDetector(
            threshold_ms2=self.get_parameter("threshold_ms2").value,
            watch_duration_sec=self.get_parameter("watch_duration_sec").value,
        )
        self._latest_odom: Odometry | None = None

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=5,
        )

        self._sub_imu = self.create_subscription(
            Imu, "/imu/data", self._on_imu, qos_sensor
        )
        self._sub_odom = self.create_subscription(
            Odometry, "/odom", self._on_odom, qos_sensor
        )
        if HAS_MSGS:
            self._sub_door = self.create_subscription(
                DoorwayMeasurement,
                "/compliance/door_clear_width",
                self._on_doorway, 10,
            )
            self._pub = self.create_publisher(
                ThresholdMeasurement, "/compliance/door_threshold", 10
            )
        else:
            self._sub_door = None
            self._pub = None
            self.get_logger().warn(
                "star_compliance_msgs missing; node degraded to IMU-only logging."
            )

        self._ensure_csv_header()
        self.get_logger().info(
            f"star_door_threshold_node ready. ADA {ADA_SECTION}. "
            f"Threshold {self._detector.threshold_ms2} m/s^2; "
            f"watch {self._detector.watch_duration_sec} s."
        )

    def _on_odom(self, msg: Odometry) -> None:
        self._latest_odom = msg

    def _on_doorway(self, _msg) -> None:  # pragma: no cover - requires msgs
        if not self.get_parameter("enabled").value:
            return
        self._detector.start_watch(self.get_clock().now().nanoseconds * 1e-9)
        self.get_logger().info("threshold watch armed (2 s)")

    def _on_imu(self, msg: Imu) -> None:
        if not self.get_parameter("enabled").value:
            return
        t = self._stamp_to_sec(msg.header.stamp)
        if not self._detector.in_watch_window(t):
            return
        odom_xy = self._odom_xy_or_zero()
        velocity = self._odom_velocity_or_zero()
        event = self._detector.feed(
            t_sec=t,
            qx=msg.orientation.x, qy=msg.orientation.y,
            qz=msg.orientation.z, qw=msg.orientation.w,
            ax=msg.linear_acceleration.x,
            ay=msg.linear_acceleration.y,
            az=msg.linear_acceleration.z,
            robot_xy=odom_xy,
            velocity_mps=velocity,
        )
        if event is None:
            return
        self.get_logger().info(
            f"threshold jolt detected at ({event.robot_x_m:.2f}, "
            f"{event.robot_y_m:.2f}) magnitude={event.jolt_magnitude_ms2:.2f} m/s^2"
        )
        self._emit(event)

    def _emit(self, event) -> None:
        if self._pub is not None and HAS_MSGS:
            msg = ThresholdMeasurement()
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = "map"
            msg.step_pose_map_frame = self._pose_from_xy(
                event.robot_x_m, event.robot_y_m
            )
            msg.detected = True
            msg.jolt_magnitude_ms2 = float(event.jolt_magnitude_ms2)
            msg.traversal_velocity_mps = float(event.traversal_velocity_mps)
            # Rough height estimate: magnitude / (2 * velocity^2) but
            # capped to a plausible range. Presence only, not certified.
            if event.traversal_velocity_mps > 0.05:
                msg.estimated_step_height_m = min(
                    0.05,
                    event.jolt_magnitude_ms2 / (80.0 + 1.0 / event.traversal_velocity_mps),
                )
            else:
                msg.estimated_step_height_m = 0.0
            msg.flagged_violation = True
            msg.ada_section = ADA_SECTION
            msg.detection_method = DETECTION_METHOD
            msg.disclosure = DISCLOSURE
            self._pub.publish(msg)
        self._append_csv(event)

    def _append_csv(self, event) -> None:
        now = time.gmtime()
        with VALIDATION_CSV.open("a", newline="") as f:
            csv.writer(f).writerow([
                f"THR-{int(time.time())}",
                time.strftime("%Y-%m-%d", now),
                time.strftime("%H:%M:%S", now),
                "AUTO",
                "live-run",
                f"{event.robot_x_m:.3f}",
                f"{event.robot_y_m:.3f}",
                f"{event.jolt_magnitude_ms2:.2f}",
                f"{event.traversal_velocity_mps:.2f}",
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
                    "jolt_magnitude_ms2",
                    "traversal_velocity_mps",
                    "flagged", "ada_section",
                ])

    # ------------------------------------------------------------------

    @staticmethod
    def _stamp_to_sec(stamp) -> float:
        return float(stamp.sec) + float(stamp.nanosec) * 1e-9

    def _odom_xy_or_zero(self) -> tuple[float, float]:
        if self._latest_odom is None:
            return (0.0, 0.0)
        p = self._latest_odom.pose.pose.position
        return (float(p.x), float(p.y))

    def _odom_velocity_or_zero(self) -> float:
        if self._latest_odom is None:
            return 0.0
        v = self._latest_odom.twist.twist.linear
        return float((v.x * v.x + v.y * v.y) ** 0.5)

    def _pose_from_xy(self, x: float, y: float) -> PoseStamped:
        ps = PoseStamped()
        ps.header.stamp = self.get_clock().now().to_msg()
        ps.header.frame_id = "map"
        ps.pose.position.x = float(x)
        ps.pose.position.y = float(y)
        ps.pose.orientation.w = 1.0
        return ps


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = DoorThresholdNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
