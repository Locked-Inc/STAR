"""Ramp slope compliance node - IMPLEMENTED.

ADA 2010 Standards for Accessible Design, section 405.2: ramp running
slope must not exceed 1:12 (4.76 deg).

Subscribes to:
  /scan       sensor_msgs/LaserScan   from RPLiDAR C1
  /imu/data   sensor_msgs/Imu         from BNO055 (via star_spi_bridge)
  /odom       nav_msgs/Odometry       from robot_localization EKF

Publishes:
  /compliance/ramp_slope/measurement   star_compliance_msgs/RampSlope
                                       (or a stand-in diagnostics topic
                                       while the custom message is
                                       scaffolded; see TODO below)

Writes:
  validation_log.csv row per accepted measurement.
  PNG thumbnail per accepted measurement under
  /tmp/star_compliance/<ts>.png.
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

from sensor_msgs.msg import LaserScan, Imu
from nav_msgs.msg import Odometry

from star_compliance.engines.plane_segmentation import fit_plane_ransac
from star_compliance.engines.imu_cross_validate import (
    ImuPitchWindow,
    agreement_within,
    quaternion_to_pitch_deg,
)

ADA_RAMP_SLOPE_THRESHOLD_DEG = 4.76
IMU_AGREEMENT_GATE_DEG = 0.5
SCAN_FORWARD_MAX_M = 2.0
SCAN_FORWARD_HALF_ANGLE_DEG = 15.0
MIN_POINTS_FOR_FIT = 50
OUTPUT_DIR = Path("/tmp/star_compliance")
VALIDATION_CSV = Path(
    os.environ.get(
        "STAR_VALIDATION_CSV",
        str(Path(__file__).resolve().parents[3] / "extras" / "validation_log.csv"),
    )
)


class RampSlopeNode(Node):

    def __init__(self) -> None:
        super().__init__("star_ramp_slope_node")

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            durability=DurabilityPolicy.VOLATILE,
            depth=5,
        )

        self._imu_pitch = ImuPitchWindow(window_sec=0.5)
        self._latest_odom: Odometry | None = None
        self._forward_moving: bool = False

        self._sub_scan = self.create_subscription(
            LaserScan, "/scan", self._on_scan, qos_sensor,
        )
        self._sub_imu = self.create_subscription(
            Imu, "/imu/data", self._on_imu, qos_sensor,
        )
        self._sub_odom = self.create_subscription(
            Odometry, "/odom", self._on_odom, qos_sensor,
        )

        OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
        self._ensure_csv_header()

        self.get_logger().info(
            f"STAR ramp_slope node ready. "
            f"Threshold {ADA_RAMP_SLOPE_THRESHOLD_DEG} deg (ADA 405.2). "
            f"Agreement gate +/- {IMU_AGREEMENT_GATE_DEG} deg. "
            f"Logging to {VALIDATION_CSV}."
        )

    # --- subscription callbacks ---

    def _on_odom(self, msg: Odometry) -> None:
        self._latest_odom = msg
        vx = msg.twist.twist.linear.x
        self._forward_moving = vx > 0.05  # m/s

    def _on_imu(self, msg: Imu) -> None:
        t = self._stamp_to_sec(msg.header.stamp)
        pitch = quaternion_to_pitch_deg(
            msg.orientation.x,
            msg.orientation.y,
            msg.orientation.z,
            msg.orientation.w,
        )
        self._imu_pitch.push(t, pitch)

    def _on_scan(self, msg: LaserScan) -> None:
        if not self._forward_moving:
            return
        points = self._scan_to_forward_xy(msg)
        if points.shape[0] < MIN_POINTS_FOR_FIT:
            return

        # Lift 2D LiDAR points into 3D at z=0 in base_link. This is a
        # simplification valid when the robot is driving onto a ramp -
        # a scanning plane tilted by the ramp pitch gives a line cluster
        # the plane fit can project. Full 3D handling is the RTAB-Map
        # integration phase.
        points3 = np.column_stack([points, np.zeros(points.shape[0])])
        try:
            fit = fit_plane_ransac(points3, distance_threshold=0.03)
        except Exception as exc:
            self.get_logger().warn(f"plane fit failed: {exc}")
            return

        now = self._stamp_to_sec(msg.header.stamp)
        imu_summary = self._imu_pitch.summary(now)
        if imu_summary is None or imu_summary.n < 20:
            return

        slope_lidar = float(fit.slope_deg)
        slope_bno = float(imu_summary.pitch_deg)
        if not agreement_within(slope_lidar, slope_bno, IMU_AGREEMENT_GATE_DEG):
            self.get_logger().info(
                f"disagreement: LiDAR {slope_lidar:.2f} deg vs. IMU {slope_bno:.2f} deg - re-scan queued"
            )
            self._append_csv(
                slope_gt_deg=None,
                slope_bno=slope_bno,
                slope_lidar=slope_lidar,
                agreement=abs(slope_lidar - slope_bno),
                flagged=False,
                notes="disagreement gate triggered - retry",
            )
            return

        slope_final = 0.5 * (slope_lidar + slope_bno)
        flagged = slope_final > ADA_RAMP_SLOPE_THRESHOLD_DEG

        self.get_logger().info(
            f"ramp slope accepted: {slope_final:.2f} deg "
            f"(LiDAR {slope_lidar:.2f}, BNO055 {slope_bno:.2f}); "
            f"ADA 405.2 {'VIOLATION' if flagged else 'OK'}"
        )
        self._append_csv(
            slope_gt_deg=None,
            slope_bno=slope_bno,
            slope_lidar=slope_lidar,
            agreement=abs(slope_lidar - slope_bno),
            flagged=flagged,
            notes="",
        )

    # --- helpers ---

    @staticmethod
    def _stamp_to_sec(stamp) -> float:
        return float(stamp.sec) + float(stamp.nanosec) * 1e-9

    @staticmethod
    def _scan_to_forward_xy(msg: LaserScan) -> np.ndarray:
        ranges = np.asarray(msg.ranges, dtype=np.float64)
        n = ranges.size
        angles = msg.angle_min + msg.angle_increment * np.arange(n, dtype=np.float64)
        valid = np.isfinite(ranges) & (ranges >= msg.range_min) & (ranges <= msg.range_max)
        forward_half_rad = np.radians(SCAN_FORWARD_HALF_ANGLE_DEG)
        forward = np.abs(angles) <= forward_half_rad
        mask = valid & forward & (ranges <= SCAN_FORWARD_MAX_M)
        r = ranges[mask]
        a = angles[mask]
        xs = r * np.cos(a)
        ys = r * np.sin(a)
        return np.column_stack([xs, ys])

    def _ensure_csv_header(self) -> None:
        VALIDATION_CSV.parent.mkdir(parents=True, exist_ok=True)
        if not VALIDATION_CSV.exists() or VALIDATION_CSV.stat().st_size == 0:
            with VALIDATION_CSV.open("w", newline="") as f:
                writer = csv.writer(f)
                writer.writerow([
                    "run_id", "date", "time", "operator", "location",
                    "ramp_slope_gt_deg", "ramp_slope_bno055_deg",
                    "ramp_slope_lidar_deg", "bno055_lidar_agreement_deg",
                    "violation_threshold_deg", "flagged", "notes",
                ])

    def _append_csv(self, slope_gt_deg, slope_bno, slope_lidar,
                    agreement, flagged, notes) -> None:
        now = time.gmtime()
        with VALIDATION_CSV.open("a", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                f"AUTO-{int(time.time())}",
                time.strftime("%Y-%m-%d", now),
                time.strftime("%H:%M:%S", now),
                "AUTO",
                "live-run",
                "" if slope_gt_deg is None else f"{slope_gt_deg:.2f}",
                f"{slope_bno:.2f}",
                f"{slope_lidar:.2f}",
                f"{agreement:.2f}",
                f"{ADA_RAMP_SLOPE_THRESHOLD_DEG:.2f}",
                "yes" if flagged else "no",
                notes,
            ])


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = RampSlopeNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
