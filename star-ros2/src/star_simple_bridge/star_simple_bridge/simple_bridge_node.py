"""Minimal ASCII serial bridge between ROS2 and the RX72N motor controller.

This module is a SLAM MVP replacement for the full star_gateway_bridge
C++ package. It talks a newline-delimited ASCII line protocol to the
RX72N firmware over /dev/ttyACM0:

  ROS2 -> RX72N:  "V <fl> <fr> <bl> <br>\\n"   (m/s per wheel)
  RX72N -> ROS2:  "E <fl> <fr> <bl> <br> <ms>\\n"
                  (signed int16 encoder deltas at MTU1 / MTU2 / TPU1 /
                  TPU2 channels, followed by a uint32 millisecond
                  timestamp)

The robot is a 4-wheel skid-steer. Per-side velocities are derived
from geometry_msgs/Twist using the standard differential-drive
kinematics. Integrated pose is published on /odom/unfiltered; the
robot_localization ekf_filter_node owns the odom->base_link TF, so
this node does NOT broadcast a transform.

Wire-protocol quirk (harness wiring): due to how the encoder cables
are routed, TPU1 physically reads the BL wheel and TPU2 reads BR.
The firmware already places them at positions 3 and 4 of the "E "
line, so from this node's perspective they are labelled fl/fr/bl/br
in transmit order and no swap is required here.
"""

import math
import threading

import rclpy
import serial
from geometry_msgs.msg import Quaternion, Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node

# Serial transport
SERIAL_DEVICE = "/dev/ttyACM0"
SERIAL_BAUD = 115200
SERIAL_READ_TIMEOUT_S = 0.05

# Drivetrain geometry. Sourced from
# star-ros2/src/star_bringup/launch/slam.launch.py:50-52 and
# star_gateway_bridge_node.cpp:79-81 (goBILDA Wasteland chassis,
# 144 mm wheels, 356 mm track width).
WHEEL_RADIUS_M = 0.072
WHEEL_BASE_M = 0.356

# Encoder resolution. RX72N firmware currently reports raw quadrature
# counts at the motor shaft: 341 PPR Hall sensor * 4x quadrature
# decoding = 1364 ticks per motor revolution. NOTE: this does NOT
# include the gearbox reduction; if the firmware later starts
# reporting wheel-side ticks this constant must be updated.
TICKS_PER_REV = 1364

# Signed 16-bit wrap threshold. The firmware sends int16 deltas; any
# absolute jump larger than this is assumed to be a wrap and the
# delta is rotated by +/- 65536.
ENCODER_WRAP_THRESHOLD = 32000
ENCODER_WRAP_RANGE = 65536

# Timers
SERIAL_DRAIN_HZ = 100.0
CMD_VEL_QOS_DEPTH = 10
ODOM_QOS_DEPTH = 10

# Frame IDs (must match ekf.yaml and the URDF).
FRAME_ODOM = "odom"
FRAME_BASE_LINK = "base_link"

# Safety bounds
MIN_DT_S = 0.001  # clamp reported timestamp delta to avoid div-by-zero


def _yaw_to_quaternion(yaw: float) -> Quaternion:
    """Convert a planar yaw (radians) into a geometry_msgs/Quaternion."""
    half = 0.5 * yaw
    quat = Quaternion()
    quat.x = 0.0
    quat.y = 0.0
    quat.z = math.sin(half)
    quat.w = math.cos(half)
    return quat


def _unwrap_int16_delta(new_val: int, old_val: int) -> int:
    """Return a wrap-corrected delta for a signed 16-bit encoder counter."""
    raw = new_val - old_val
    if raw > ENCODER_WRAP_THRESHOLD:
        raw -= ENCODER_WRAP_RANGE
    elif raw < -ENCODER_WRAP_THRESHOLD:
        raw += ENCODER_WRAP_RANGE
    return raw


class SimpleBridgeNode(Node):
    """ROS2 node bridging /cmd_vel and /odom/unfiltered to the RX72N."""

    def __init__(self) -> None:
        super().__init__("star_simple_bridge")

        # Open the serial port first -- if this fails the node should
        # exit rather than silently produce zero odometry.
        try:
            self._port = serial.Serial(
                SERIAL_DEVICE,
                SERIAL_BAUD,
                timeout=SERIAL_READ_TIMEOUT_S,
                write_timeout=SERIAL_READ_TIMEOUT_S,
            )
        except serial.SerialException as exc:
            self.get_logger().error(
                f"Failed to open {SERIAL_DEVICE} @ {SERIAL_BAUD}: {exc}"
            )
            raise

        self.get_logger().info(
            f"Opened {SERIAL_DEVICE} @ {SERIAL_BAUD} "
            f"(wheel_radius={WHEEL_RADIUS_M} m, "
            f"wheel_base={WHEEL_BASE_M} m, "
            f"ticks_per_rev={TICKS_PER_REV})"
        )

        # Odometry integration state.
        self._last_ticks_fl = None
        self._last_ticks_fr = None
        self._last_ticks_bl = None
        self._last_ticks_br = None
        self._last_ts_ms = None
        self._pose_x = 0.0
        self._pose_y = 0.0
        self._pose_theta = 0.0

        # Writes and reads can happen from different callbacks; protect
        # the shared serial handle with a lock.
        self._serial_lock = threading.Lock()

        # Line-buffered parse state for incoming encoder frames.
        self._rx_buffer = bytearray()

        self._cmd_vel_sub = self.create_subscription(
            Twist, "/cmd_vel", self._cmd_vel_cb, CMD_VEL_QOS_DEPTH
        )
        self._odom_pub = self.create_publisher(
            Odometry, "/odom/unfiltered", ODOM_QOS_DEPTH
        )

        self._drain_timer = self.create_timer(
            1.0 / SERIAL_DRAIN_HZ, self._serial_drain_cb
        )

    # -- /cmd_vel -> firmware ----------------------------------------

    def _cmd_vel_cb(self, msg: Twist) -> None:
        """Translate a Twist into per-wheel velocities and transmit."""
        half_base = WHEEL_BASE_M / 2.0
        v_left = msg.linear.x - msg.angular.z * half_base
        v_right = msg.linear.x + msg.angular.z * half_base

        # Skid-steer: front + back wheel on each side get the same
        # commanded linear velocity.
        line = (
            f"V {v_left:.3f} {v_right:.3f} "
            f"{v_left:.3f} {v_right:.3f}\n"
        ).encode("ascii")

        with self._serial_lock:
            try:
                self._port.write(line)
            except (serial.SerialException, serial.SerialTimeoutException) as exc:
                self.get_logger().warn(f"Serial write failed: {exc}")

    # -- firmware -> /odom/unfiltered --------------------------------

    def _serial_drain_cb(self) -> None:
        """Read any pending bytes and dispatch one line at a time."""
        with self._serial_lock:
            try:
                waiting = self._port.in_waiting
                if waiting:
                    self._rx_buffer.extend(self._port.read(waiting))
            except serial.SerialException as exc:
                self.get_logger().warn(f"Serial read failed: {exc}")
                return

        while True:
            idx = self._rx_buffer.find(b"\n")
            if idx < 0:
                break
            raw_line = bytes(self._rx_buffer[:idx])
            del self._rx_buffer[: idx + 1]
            try:
                line = raw_line.decode("ascii").strip()
            except UnicodeDecodeError:
                self.get_logger().warn("Dropped non-ASCII serial line")
                continue
            if not line:
                continue
            self._dispatch_line(line)

    def _dispatch_line(self, line: str) -> None:
        """Route a single decoded ASCII line to the right handler."""
        if line.startswith("E "):
            self._handle_encoder_line(line)
        elif line.startswith("#"):
            self.get_logger().debug(f"fw: {line}")
        else:
            self.get_logger().warn(f"Unknown firmware line: {line!r}")

    def _handle_encoder_line(self, line: str) -> None:
        """Parse an "E fl fr bl br ms" frame and publish odometry."""
        tokens = line.split()
        expected_token_count = 6
        if len(tokens) != expected_token_count:
            self.get_logger().warn(
                f"Malformed encoder line (want {expected_token_count} "
                f"tokens, got {len(tokens)}): {line!r}"
            )
            return
        try:
            ticks_fl = int(tokens[1])
            ticks_fr = int(tokens[2])
            ticks_bl = int(tokens[3])
            ticks_br = int(tokens[4])
            ts_ms = int(tokens[5])
        except ValueError:
            self.get_logger().warn(f"Non-integer encoder tokens: {line!r}")
            return

        # First frame: seed state and skip publication.
        if self._last_ts_ms is None:
            self._last_ticks_fl = ticks_fl
            self._last_ticks_fr = ticks_fr
            self._last_ticks_bl = ticks_bl
            self._last_ticks_br = ticks_br
            self._last_ts_ms = ts_ms
            return

        dticks_fl = _unwrap_int16_delta(ticks_fl, self._last_ticks_fl)
        dticks_fr = _unwrap_int16_delta(ticks_fr, self._last_ticks_fr)
        dticks_bl = _unwrap_int16_delta(ticks_bl, self._last_ticks_bl)
        dticks_br = _unwrap_int16_delta(ticks_br, self._last_ticks_br)

        # Convert per-wheel tick delta to linear distance travelled.
        meters_per_tick = (2.0 * math.pi * WHEEL_RADIUS_M) / TICKS_PER_REV
        d_fl = dticks_fl * meters_per_tick
        d_fr = dticks_fr * meters_per_tick
        d_bl = dticks_bl * meters_per_tick
        d_br = dticks_br * meters_per_tick

        # Skid-steer: average front+back on each side to filter noise
        # and absorb minor per-wheel slip.
        d_left = 0.5 * (d_fl + d_bl)
        d_right = 0.5 * (d_fr + d_br)

        dx_body = 0.5 * (d_left + d_right)
        dtheta = (d_right - d_left) / WHEEL_BASE_M

        # Timestamp-based dt. The firmware emits a uint32 ms counter
        # which will wrap after ~49 days; treat a negative delta as a
        # wrap rather than as an invalid frame.
        raw_dt_ms = ts_ms - self._last_ts_ms
        if raw_dt_ms < 0:
            raw_dt_ms += 1 << 32
        dt_s = max(raw_dt_ms / 1000.0, MIN_DT_S)

        linear_vel = dx_body / dt_s
        angular_vel = dtheta / dt_s

        # Integrate pose in the body frame, then rotate into odom.
        self._pose_theta += dtheta
        self._pose_x += dx_body * math.cos(self._pose_theta)
        self._pose_y += dx_body * math.sin(self._pose_theta)

        self._last_ticks_fl = ticks_fl
        self._last_ticks_fr = ticks_fr
        self._last_ticks_bl = ticks_bl
        self._last_ticks_br = ticks_br
        self._last_ts_ms = ts_ms

        odom = Odometry()
        odom.header.stamp = self.get_clock().now().to_msg()
        odom.header.frame_id = FRAME_ODOM
        odom.child_frame_id = FRAME_BASE_LINK
        odom.pose.pose.position.x = self._pose_x
        odom.pose.pose.position.y = self._pose_y
        odom.pose.pose.position.z = 0.0
        odom.pose.pose.orientation = _yaw_to_quaternion(self._pose_theta)
        odom.twist.twist.linear.x = linear_vel
        odom.twist.twist.linear.y = 0.0
        odom.twist.twist.linear.z = 0.0
        odom.twist.twist.angular.x = 0.0
        odom.twist.twist.angular.y = 0.0
        odom.twist.twist.angular.z = angular_vel
        self._odom_pub.publish(odom)

    # -- shutdown ----------------------------------------------------

    def destroy_node(self) -> bool:
        """Close the serial port on shutdown."""
        try:
            if self._port and self._port.is_open:
                self._port.close()
        except serial.SerialException:
            pass
        return super().destroy_node()


def main(args=None) -> None:
    """ament_python console_scripts entry point."""
    rclpy.init(args=args)
    node = None
    try:
        node = SimpleBridgeNode()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node is not None:
            node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
