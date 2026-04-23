"""Minimal ASCII serial bridge between ROS2 and the RX72N motor controller.

This module is a SLAM MVP replacement for the full star_gateway_bridge
C++ package. It talks a newline-delimited ASCII line protocol to the
RX72N firmware over /dev/ttyACM0:

  ROS2 -> RX72N:  "V <fl> <fr> <bl> <br>\\n"   (m/s per wheel)
  RX72N -> ROS2:  "E <fl> <fr> <bl> <br> <ms>\\n"
                  (signed int16 encoder deltas at MTU1 / MTU2 / TPU1 /
                  TPU2 channels, followed by a uint32 millisecond
                  timestamp)
  RX72N -> ROS2:  "M <d_fl> <d_fr> <d_bl> <d_br>
                     <f_fl> <f_fr> <f_bl> <f_br> <ms>\\n"
                  (int16 per-motor duty in tenths of a percent, then
                  uint8 DRV8263 fault byte per motor, then uint32 ms)
  RX72N -> ROS2:  "I <qw> <qx> <qy> <qz> <roll> <pitch> <heading>
                     <gx> <gy> <gz> <ax> <ay> <az> <ms>\\n"
                  (int16 raw BNO055-scaled quaternion, Euler angles
                  (deg*16), gyro rates (dps*16), linear accel
                  (m/s^2 * 100), then uint32 ms)

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

import io
import math
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

import numpy as np
from PIL import Image

import rclpy
import serial
from geometry_msgs.msg import Quaternion, TransformStamped, Twist
from nav_msgs.msg import OccupancyGrid, Odometry
from prometheus_client import Counter, Gauge, start_http_server
from rclpy.node import Node
from sensor_msgs.msg import Imu, LaserScan
from std_msgs.msg import Bool, Float32, Float32MultiArray, MultiArrayDimension, UInt8MultiArray
from tf2_ros import TransformBroadcaster

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
TICKS_PER_REV = 1283  # empirical 2026-04-22 geom-mean of two ruler drives
# Drive A: TPR=1364 gave 85.4% of ruler (15% low) -> implied TPR=1165
# Drive B: TPR=1166 gave 121% of ruler (21% high) -> implied TPR=1412
# Geometric mean ~= 1283. ~10% drift run-to-run from wheel slip is expected;
# SLAM toolbox's scan matching absorbs the rest.

# Signed 16-bit wrap threshold. The firmware sends int16 deltas; any
# absolute jump larger than this is assumed to be a wrap and the
# delta is rotated by +/- 65536.
ENCODER_WRAP_THRESHOLD = 32000
ENCODER_WRAP_RANGE = 65536

# Timers
SERIAL_DRAIN_HZ = 100.0
CMD_VEL_QOS_DEPTH = 10
ODOM_QOS_DEPTH = 10
IMU_QOS_DEPTH = 10
MOTOR_DIAG_QOS_DEPTH = 10

# Frame IDs (must match ekf.yaml and the URDF).
FRAME_ODOM = "odom"
FRAME_BASE_LINK = "base_link"
FRAME_IMU = "imu_link"

# Safety bounds
MIN_DT_S = 0.001  # clamp reported timestamp delta to avoid div-by-zero

# Per-wheel saturation threshold. At the open-loop 80 duty/mps mapping in
# firmware, 1.0 m/s -> 80% duty which is near the motors' physical capacity
# under load. When a combined linear+angular command would push one side
# above this, we scale BOTH wheels down proportionally so the turn ratio
# (v_right / v_left) is preserved -- otherwise the saturated wheel maxes
# out and the other keeps its commanded value, producing a lurching, off-
# axis turn instead of the commanded one. Used in _cmd_vel_cb.
MAX_WHEEL_MPS = 1.0

# Stiction-break floor. The firmware's 80-duty/mps open-loop mapping
# produces 6% PWM at 0.075 m/s -- well below the stall threshold on the
# damaged motors (BR dead, BL half-speed). On healthy hardware Nav2 would
# ramp through this band in <1 s, but on this CPU-bound Pi5 the controller
# runs at 5-7 Hz and its output rarely exceeds 0.1 m/s during explore,
# leaving the motors humming but stationary. When ANY non-zero twist
# arrives, boost the peak wheel velocity up to at least this floor while
# preserving the v_right/v_left ratio so turn direction is unchanged.
# Remove once motors are replaced and Nav2's control loop reaches 10 Hz.
MIN_WHEEL_MPS_STICTION = 0.30

# BNO055 integer-scale divisors. Must match k_imu_scale_* in
# star-rx72n-firmware/src/shared/shared_data.h.
IMU_SCALE_QUAT = 16384.0
IMU_SCALE_EULER_DEG = 16.0   # raw / 16 -> degrees
IMU_SCALE_GYRO_DPS = 16.0    # raw / 16 -> deg/s
IMU_SCALE_ACC_MPS2 = 100.0   # raw / 100 -> m/s^2

# BNO055 chip is soldered face-down on the PCB: its native +Z points
# into the board (== downward in robot frame), so the raw fusion output
# reports pitch ~= 180 deg when the robot is sitting level. Apply a
# 180-deg rotation about the IMU body Y axis to publish data in the
# robot's base_link convention.
#
# Correction quaternion q_corr = (w=0, x=0, y=1, z=0) (== 180 deg rot-Y).
# Applied as q_out = q_corr * q_raw (left-multiply). For 3-vectors the
# same rotation acts as (x, y, z) -> (-x, y, -z).
IMU_MOUNT_FLIP_Y = True

# DRV8263-side (firmware) duty_cycle_percent is sent in tenths of a
# percent. Must match k_serial_duty_scale in serial_bringup_task.c.
DUTY_TENTHS_PER_PERCENT = 10.0

# Per-axis wheel labels used for the Float32MultiArray dimension label.
# Order must match the firmware wire order [FL FR BL BR].
MOTOR_LABELS = ("fl", "fr", "bl", "br")

# -----------------------------------------------------------------
# Prometheus metrics.
# ---
# These live at module scope so the prometheus_client global registry
# exposes them on GET /metrics without any per-instance wiring. An
# HTTP server is started once in main().
# -----------------------------------------------------------------
PROMETHEUS_PORT = 9100
MAP_HTTP_PORT = 9101  # serves the SLAM OccupancyGrid as a PNG image

# Per-wheel signed values; label = wheel (fl/fr/bl/br).
PROM_MOTOR_VELOCITY = Gauge(
    "star_motor_velocity_mps",
    "Per-wheel linear velocity in meters per second",
    ["wheel"],
)
PROM_MOTOR_DUTY = Gauge(
    "star_motor_duty_percent",
    "Per-wheel PWM duty cycle in signed percent (-100 to +100)",
    ["wheel"],
)
PROM_MOTOR_FAULT = Gauge(
    "star_motor_fault_byte",
    "Per-wheel DRV8263 fault-status byte (0 = no fault)",
    ["wheel"],
)

# IMU scalars; label = axis (roll/pitch/heading, x/y/z).
PROM_IMU_ORIENTATION = Gauge(
    "star_imu_orientation_deg",
    "IMU Euler orientation in degrees (BNO055 roll/pitch/heading)",
    ["axis"],
)
PROM_IMU_GYRO = Gauge(
    "star_imu_gyro_dps",
    "IMU angular velocity in degrees per second",
    ["axis"],
)
PROM_IMU_ACCEL = Gauge(
    "star_imu_accel_mps2",
    "IMU linear acceleration in m/s^2 (gravity-compensated NDOF mode)",
    ["axis"],
)

# Odometry at the robot frame.
PROM_ODOM_LINEAR_MPS = Gauge(
    "star_odom_linear_mps",
    "Odometry linear velocity (body-x) in m/s",
)
PROM_ODOM_ANGULAR_RPS = Gauge(
    "star_odom_angular_radps",
    "Odometry angular velocity (body-z) in rad/s",
)
PROM_ODOM_POSE_X = Gauge(
    "star_odom_pose_x_m",
    "Integrated odom pose X in meters (odom frame)",
)
PROM_ODOM_POSE_Y = Gauge(
    "star_odom_pose_y_m",
    "Integrated odom pose Y in meters (odom frame)",
)
PROM_ODOM_POSE_THETA = Gauge(
    "star_odom_pose_theta_rad",
    "Integrated odom yaw in radians (odom frame)",
)

# /scan summary + angular bins. LaserScan is 360 ranges at 10Hz. Shipping
# every ray to Prometheus would be wasteful; for live polar viz use
# Foxglove. Here we ship summary stats + a 36-bin angular profile (one
# bin per 10 deg) which is plenty for a Grafana "where's the obstacle"
# heatmap / bar gauge.
PROM_SCAN_RANGE_STAT = Gauge(
    "star_scan_range_m",
    "LiDAR scan range aggregates in meters (min/max/mean of valid returns)",
    ["stat"],
)
PROM_SCAN_OBSTACLE_COUNT = Gauge(
    "star_scan_obstacles_under_1m",
    "Number of laser returns with range < 1.0 m (proximity warning)",
)
PROM_SCAN_CLOSEST_RANGE = Gauge(
    "star_scan_closest_range_m",
    "Closest valid laser return in meters",
)
PROM_SCAN_CLOSEST_BEARING = Gauge(
    "star_scan_closest_bearing_deg",
    "Bearing of the closest laser return in degrees (0=forward, CCW positive)",
)
# Bridge/transport counters.
PROM_RX_BYTES = Counter(
    "star_bridge_rx_bytes_total",
    "Total bytes read from the RX72N serial port",
)
PROM_RX_LINES = Counter(
    "star_bridge_rx_lines_total",
    "Total well-formed lines dispatched (E/M/I/#)",
    ["kind"],
)
PROM_PARSE_ERRORS = Counter(
    "star_bridge_parse_errors_total",
    "Total line-parse failures (malformed token count or non-integer)",
    ["kind"],
)
PROM_SERIAL_REOPENS = Counter(
    "star_bridge_serial_reopens_total",
    "Total times the serial port had to be reopened (USB glitch recovery)",
)


# -----------------------------------------------------------------
# Latest SLAM map snapshot, cached so the HTTP handler can render a
# PNG on demand without re-subscribing. Written from the ROS callback
# thread, read from the HTTP server thread -- hence the lock.
# -----------------------------------------------------------------
_map_lock = threading.Lock()
_latest_map: "OccupancyGrid | None" = None


def _grid_to_greyscale(grid: OccupancyGrid) -> "tuple[np.ndarray, tuple[int, int]]":
    """Convert an OccupancyGrid to an 8-bit greyscale numpy array.

    Returns (pixels, (width, height)) with the y-axis flipped so the
    image is ready for top-down viewing.

    Mapping matches the Foxglove / RViz convention:
        -1 (unknown) -> 180 (grey)
         0 (free)    -> 255 (white)
       100 (occupied)-> 0   (black)
    """
    max_cell = 100
    free_level = 255
    unknown_level = 180

    w = grid.info.width
    h = grid.info.height
    if w == 0 or h == 0:
        return np.full((16, 16), unknown_level, dtype=np.uint8), (16, 16)

    data = np.asarray(grid.data, dtype=np.int16).reshape((h, w))
    pixels = np.full((h, w), unknown_level, dtype=np.uint8)
    pixels[data == 0] = free_level
    occ = data > 0
    pixels[occ] = np.clip(
        free_level - (data[occ].astype(np.int32) * free_level // max_cell),
        0, free_level,
    ).astype(np.uint8)
    # ROS OccupancyGrid origin is bottom-left; image origin is top-left.
    pixels = np.flipud(pixels)
    return pixels, (w, h)


def _render_map_to_pgm_bytes(grid: OccupancyGrid) -> bytes:
    """Serialize the OccupancyGrid as a Nav2-compatible PGM (P5 binary)."""
    pixels, _ = _grid_to_greyscale(grid)
    img = Image.fromarray(pixels, mode="L")
    buf = io.BytesIO()
    # Pillow writes P5 PGM with format "PPM" when mode is "L".
    img.save(buf, format="PPM")
    return buf.getvalue()


def _render_map_yaml(grid: OccupancyGrid, pgm_filename: str = "map.pgm") -> bytes:
    """Emit the YAML metadata Nav2 map_server expects alongside the PGM."""
    origin = grid.info.origin
    yaml_text = (
        f"image: {pgm_filename}\n"
        f"resolution: {grid.info.resolution}\n"
        f"origin: [{origin.position.x}, {origin.position.y}, 0.0]\n"
        f"negate: 0\n"
        f"occupied_thresh: 0.65\n"
        f"free_thresh: 0.196\n"
    )
    return yaml_text.encode("ascii")


def _render_map_to_png_bytes(grid: OccupancyGrid) -> bytes:
    """Render a nav_msgs/OccupancyGrid to a PNG byte string.

    Color map matches the Foxglove / RViz convention:
        -1 (unknown) -> grey
         0 (free)    -> white
       100 (occupied)-> black
    Scales up tiny grids for visibility on Grafana panels.
    """
    max_cell = 100
    free_level = 255
    unknown_level = 180

    w = grid.info.width
    h = grid.info.height
    if w == 0 or h == 0:
        img = Image.new("L", (16, 16), unknown_level)
    else:
        data = np.asarray(grid.data, dtype=np.int16).reshape((h, w))
        # Convert to 8-bit greyscale.
        pixels = np.full((h, w), unknown_level, dtype=np.uint8)
        free = data == 0
        pixels[free] = free_level
        occ = data > 0
        pixels[occ] = np.clip(
            free_level - (data[occ].astype(np.int32) * free_level // max_cell),
            0, free_level,
        ).astype(np.uint8)
        # ROS OccupancyGrid origin is bottom-left; image origin is top-left.
        # Flip vertically so the rendered PNG matches a top-down view.
        pixels = np.flipud(pixels)
        img = Image.fromarray(pixels, mode="L")

    # Upscale small maps for readability in Grafana (nearest-neighbor so
    # pixels stay crisp). Target ~400 px on the short side.
    target_short = 400
    short = min(img.width, img.height)
    if short < target_short:
        scale = target_short // max(short, 1)
        img = img.resize(
            (img.width * scale, img.height * scale),
            resample=Image.NEAREST,
        )

    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


class _MapRequestHandler(BaseHTTPRequestHandler):
    """HTTP handler: serves /map.png, /map.pgm, /map.yaml, and POST /map/reset.

    Endpoints:
      GET  /map.png                     -> greyscale PNG, browser-friendly
      GET  /map.pgm                     -> P5 PGM for Nav2 map_server
      GET  /map.yaml                    -> Nav2 map metadata
      POST /map/reset?confirm=yes       -> wipe slam_toolbox map
        requires the explicit confirm=yes query parameter to avoid an
        accidental curl-obliteration of a long mapping session.
    """

    def log_message(self, fmt, *args):  # noqa: N802 (stdlib API)
        """Silence the per-request stderr spam."""

    def _send_body(self, status: int, content_type: str, body: bytes,
                   download_name: "str | None" = None) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("Access-Control-Allow-Origin", "*")
        if download_name is not None:
            self.send_header(
                "Content-Disposition",
                f'attachment; filename="{download_name}"',
            )
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802 (stdlib API)
        path = self.path.split("?", 1)[0]
        if path in ("/", "/map.png"):
            self._serve_map("png")
        elif path == "/map.pgm":
            self._serve_map("pgm")
        elif path == "/map.yaml":
            self._serve_map("yaml")
        else:
            self.send_error(404, "Endpoints: /map.png, /map.pgm, /map.yaml, POST /map/reset")

    def do_POST(self) -> None:  # noqa: N802 (stdlib API)
        path_only, _, query = self.path.partition("?")
        if path_only != "/map/reset":
            self.send_error(404, "POST only accepted at /map/reset")
            return
        if "confirm=yes" not in query:
            self.send_error(
                400,
                "Refusing to reset map without ?confirm=yes in the query string",
            )
            return
        try:
            subprocess.run(
                ["ros2", "lifecycle", "set", "/slam_toolbox", "deactivate"],
                check=False, capture_output=True, timeout=5,
            )
            subprocess.run(
                ["ros2", "lifecycle", "set", "/slam_toolbox", "cleanup"],
                check=False, capture_output=True, timeout=5,
            )
            subprocess.run(
                ["ros2", "lifecycle", "set", "/slam_toolbox", "configure"],
                check=False, capture_output=True, timeout=10,
            )
            subprocess.run(
                ["ros2", "lifecycle", "set", "/slam_toolbox", "activate"],
                check=False, capture_output=True, timeout=10,
            )
        except subprocess.TimeoutExpired as exc:
            self.send_error(504, f"slam_toolbox lifecycle call timed out: {exc}")
            return
        # Clear our cached grid too so the next /map.png read returns
        # the fresh (likely empty) grid rather than the stale one.
        global _latest_map
        with _map_lock:
            _latest_map = None
        self._send_body(200, "text/plain", b"map reset\n")

    def _serve_map(self, fmt: str) -> None:
        with _map_lock:
            grid = _latest_map
        if grid is None:
            self.send_error(503, "No /map received yet")
            return
        try:
            if fmt == "png":
                body = _render_map_to_png_bytes(grid)
                self._send_body(200, "image/png", body)
            elif fmt == "pgm":
                body = _render_map_to_pgm_bytes(grid)
                self._send_body(
                    200, "image/x-portable-graymap", body,
                    download_name="map.pgm",
                )
            elif fmt == "yaml":
                body = _render_map_yaml(grid)
                self._send_body(
                    200, "text/yaml", body, download_name="map.yaml",
                )
        except (ValueError, IndexError) as exc:
            self.send_error(500, f"Render failed: {exc}")


def _start_map_http_server() -> None:
    """Start the PNG HTTP server in a daemon thread."""
    server = ThreadingHTTPServer(("0.0.0.0", MAP_HTTP_PORT), _MapRequestHandler)
    thread = threading.Thread(
        target=server.serve_forever, name="map-png-server", daemon=True,
    )
    thread.start()


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

        # Subscribe to the supervisor's gated output, not raw /cmd_vel.
        # Manual teleop goes /cmd_vel -> star_supervisor -> /cmd_vel_out
        # so when autonomy or e-stop is active, nothing reaches the wire.
        # The star_supervisor node performs the arbitration; here we
        # also independently enforce e-stop as a belt-and-suspenders
        # safety layer (see _estop_cb).
        self._cmd_vel_sub = self.create_subscription(
            Twist, "/cmd_vel_out", self._cmd_vel_cb, CMD_VEL_QOS_DEPTH
        )

        # Operator-adjustable speed multiplier in [0.0, 1.0]. Published
        # by the Lichtblick / Foxglove slider panel. Default 1.0 (no
        # scaling) so the bridge behaves the same on boot as before.
        self._speed_scale = 1.0
        self._speed_scale_sub = self.create_subscription(
            Float32, "/star/speed_scale", self._speed_scale_cb, 1,
        )

        # Independent e-stop listener. Zeroes the serial output on the
        # bridge side regardless of whatever the supervisor forwards,
        # so a bug or crash in the supervisor cannot leave the motors
        # running. We subscribe to TWO topics:
        #   - /star/estop        (VOLATILE): direct operator command
        #   - /star/state/estop  (TRANSIENT_LOCAL): latched supervisor state,
        #     so a late-starting bridge immediately picks up the most
        #     recent e-stop decision rather than assuming "not estopped".
        from rclpy.qos import (
            DurabilityPolicy,
            HistoryPolicy,
            QoSProfile,
            ReliabilityPolicy,
        )
        latched_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
        )
        self._estop_active = False
        self._estop_cmd_sub = self.create_subscription(
            Bool, "/star/estop", self._estop_cb, 1,
        )
        self._estop_state_sub = self.create_subscription(
            Bool, "/star/state/estop", self._estop_cb, latched_qos,
        )
        self._odom_pub = self.create_publisher(
            Odometry, "/odom/unfiltered", ODOM_QOS_DEPTH
        )
        self._imu_pub = self.create_publisher(
            Imu, "/imu/data", IMU_QOS_DEPTH
        )
        self._motor_duty_pub = self.create_publisher(
            Float32MultiArray, "/motor/duty", MOTOR_DIAG_QOS_DEPTH
        )
        self._motor_faults_pub = self.create_publisher(
            UInt8MultiArray, "/motor/faults", MOTOR_DIAG_QOS_DEPTH
        )
        # Broadcast odom -> base_link TF so Nav2 / RViz / Foxglove see a
        # moving robot. Without this the TF tree would stop at odom.
        # Gated so the ekf_filter_node (robot_localization) can own this TF
        # edge when it's running -- two publishers on the same edge produce
        # race-conditioned jitter. Launch with `-p publish_odom_tf:=false`
        # whenever EKF is in the stack; leave default True for standalone.
        self.declare_parameter("publish_odom_tf", True)
        self._publish_odom_tf = bool(
            self.get_parameter("publish_odom_tf").value
        )
        self._tf_broadcaster = (
            TransformBroadcaster(self) if self._publish_odom_tf else None
        )

        # Per-wheel scalar diagnostics for Foxglove Gauge panels. The
        # existing Float32MultiArray topics are handy for code consumers
        # but Gauge panels prefer single-valued std_msgs/Float32 so one
        # topic == one dial.
        #
        # Wheel label order matches the firmware "E" / "M" wire order:
        #   FL, FR, BL, BR. If the harness ever changes, update
        #   _handle_encoder_line and _handle_motor_line, not these
        #   publisher names.
        self._wheel_velocity_pubs = {
            label: self.create_publisher(
                Float32, f"/motor/{label}/velocity_mps", MOTOR_DIAG_QOS_DEPTH
            )
            for label in MOTOR_LABELS
        }
        self._wheel_duty_pubs = {
            label: self.create_publisher(
                Float32, f"/motor/{label}/duty_percent", MOTOR_DIAG_QOS_DEPTH
            )
            for label in MOTOR_LABELS
        }

        # IMU scalar diagnostics. sensor_msgs/Imu is the authoritative
        # channel (quaternion + gyro + accel), but Foxglove Gauges can't
        # easily read nested message fields, so we mirror the raw Euler
        # angles into scalar topics.
        self._imu_scalar_pubs = {
            name: self.create_publisher(
                Float32, f"/imu/{name}", IMU_QOS_DEPTH
            )
            for name in (
                "roll_deg", "pitch_deg", "heading_deg",
                "gyro_x_dps", "gyro_y_dps", "gyro_z_dps",
                "accel_x_mps2", "accel_y_mps2", "accel_z_mps2",
            )
        }

        self._drain_timer = self.create_timer(
            1.0 / SERIAL_DRAIN_HZ, self._serial_drain_cb
        )

        # Scan summary publisher: subscribes to /scan from sllidar_node
        # and fills the PROM_SCAN_* gauges so Grafana has a proximity /
        # bearing view without having to re-plot the whole LaserScan.
        self._scan_sub = self.create_subscription(
            LaserScan, "/scan", self._scan_cb, 10
        )

        # /map is published by slam_toolbox with transient_local durability
        # (latched). Match that QoS so we actually receive the latest
        # snapshot. The PNG server reads _latest_map on demand.
        from rclpy.qos import (
            DurabilityPolicy,
            HistoryPolicy,
            QoSProfile,
            ReliabilityPolicy,
        )
        map_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
        )
        self._map_sub = self.create_subscription(
            OccupancyGrid, "/map", self._map_cb, map_qos
        )

    # -- /cmd_vel -> firmware ----------------------------------------

    def _cmd_vel_cb(self, msg: Twist) -> None:
        """Translate a Twist into per-wheel velocities and transmit."""
        # Hardware-level e-stop: force zero regardless of input. This is
        # the second layer on top of star_supervisor's arbitration.
        if self._estop_active:
            line = b"V 0.000 0.000 0.000 0.000\n"
            self._send_serial_line(line)
            return

        # Apply operator speed scaling in [0, 1].
        scale = max(0.0, min(1.0, self._speed_scale))
        half_base = WHEEL_BASE_M / 2.0
        v_left = (msg.linear.x - msg.angular.z * half_base) * scale
        v_right = (msg.linear.x + msg.angular.z * half_base) * scale

        # Proportional per-wheel clamp. A combined linear+angular command
        # can push one wheel past MAX_WHEEL_MPS while the other stays in
        # range. The firmware open-loop mapping saturates that wheel at
        # 100% duty and leaves the other alone, which distorts the
        # commanded direction (lurch + stall). Scaling both sides by the
        # same factor keeps v_right / v_left constant so the robot still
        # follows the requested arc -- just at the highest overall speed
        # the slower-side motor can realize.
        peak = max(abs(v_left), abs(v_right))
        if peak > MAX_WHEEL_MPS:
            factor = MAX_WHEEL_MPS / peak
            v_left *= factor
            v_right *= factor
        # Per-wheel stiction floor. Peak-scaling (boost both by peak/floor)
        # fails for asymmetric commands like forward+turn: the faster wheel
        # reaches floor, the slower stays well below and doesn't rotate,
        # so the robot pivots on the dead wheel for a centimeter and stops.
        # Instead, boost EACH wheel independently to stall threshold while
        # preserving its sign. This distorts the turn radius (tighter than
        # Nav2 intended) but both wheels actually spin, which matters more
        # than kinematic fidelity under damaged-motor conditions.
        if 0.0 < abs(v_left) < MIN_WHEEL_MPS_STICTION:
            v_left = math.copysign(MIN_WHEEL_MPS_STICTION, v_left)
        if 0.0 < abs(v_right) < MIN_WHEEL_MPS_STICTION:
            v_right = math.copysign(MIN_WHEEL_MPS_STICTION, v_right)

        # Skid-steer: front + back wheel on each side get the same
        # commanded linear velocity.
        line = (
            f"V {v_left:.3f} {v_right:.3f} "
            f"{v_left:.3f} {v_right:.3f}\n"
        ).encode("ascii")
        self._send_serial_line(line)

    # -- /star/speed_scale -> local state ----------------------------

    def _speed_scale_cb(self, msg: Float32) -> None:
        """Store a new operator speed multiplier; clamp to [0, 1]."""
        self._speed_scale = max(0.0, min(1.0, float(msg.data)))

    # -- /star/estop -> local state + immediate stop -----------------

    def _estop_cb(self, msg: Bool) -> None:
        """Flip the bridge-level e-stop flag; emit an instant zero Twist."""
        new_val = bool(msg.data)
        if new_val == self._estop_active:
            return
        self._estop_active = new_val
        if new_val:
            self.get_logger().warn("bridge e-stop engaged -- zeroing wheels")
            self._send_serial_line(b"V 0.000 0.000 0.000 0.000\n")
        else:
            self.get_logger().info("bridge e-stop cleared")

    # -- serial write helper -----------------------------------------

    def _send_serial_line(self, line: bytes) -> None:
        """Write a framed ASCII line to the RX72N with glitch recovery."""

        with self._serial_lock:
            if self._port is None:
                return  # drain tick will reopen; drop this command
            try:
                self._port.write(line)
            except (serial.SerialException,
                    serial.SerialTimeoutException,
                    OSError,
                    TypeError,
                    AttributeError) as exc:
                self.get_logger().warn(
                    f"Serial write failed: {exc}. Closing for reopen.")
                try:
                    self._port.close()
                except Exception:
                    pass
                self._port = None

    # -- firmware -> /odom/unfiltered --------------------------------

    def _serial_drain_cb(self) -> None:
        """Read any pending bytes and dispatch one line at a time.

        Resilient to the Cypress USB-UART bridge dropping off the bus
        (an intermittent hardware issue on this board). On any serial
        error (OSError, SerialException, TypeError) we null the port
        and retry open(). While self._port is None every subsequent
        drain tick simply attempts reopen.
        """
        with self._serial_lock:
            if self._port is None or not self._port.is_open:
                try:
                    self._port = serial.Serial(
                        SERIAL_DEVICE, SERIAL_BAUD, timeout=0.05,
                        write_timeout=0.05,
                    )
                    # Drop any stale half-line that accumulated before the
                    # disconnect -- mixing it with fresh bytes from the new
                    # connection produces concatenated / unparseable lines.
                    self._rx_buffer.clear()
                    PROM_SERIAL_REOPENS.inc()
                    self.get_logger().debug(f"Reopened {SERIAL_DEVICE}")
                except Exception as reopen_exc:
                    # Device still gone -- try again next tick.
                    self._port = None
                    return
            try:
                waiting = self._port.in_waiting
                if waiting:
                    chunk = self._port.read(waiting)
                    self._rx_buffer.extend(chunk)
                    PROM_RX_BYTES.inc(len(chunk))
            except (serial.SerialException, OSError, TypeError, AttributeError) as exc:
                self.get_logger().debug(
                    f"Serial read failed: {exc}. Will reopen on next tick.")
                try:
                    self._port.close()
                except Exception:
                    pass
                self._port = None
                return

        # Split the accumulated buffer on either '\n' or '\r' so we
        # never get a mid-line split when the firmware emits '\r\n'.
        while True:
            # Find first of b'\n' or b'\r'
            nl = self._rx_buffer.find(b"\n")
            cr = self._rx_buffer.find(b"\r")
            idx = min(x for x in (nl, cr) if x >= 0) if (nl >= 0 or cr >= 0) else -1
            if idx < 0:
                break
            raw_line = bytes(self._rx_buffer[:idx])
            # Drop the terminator plus any adjacent \n or \r (handles \r\n).
            consumed = idx + 1
            while (consumed < len(self._rx_buffer) and
                   self._rx_buffer[consumed:consumed+1] in (b"\n", b"\r")):
                consumed += 1
            del self._rx_buffer[:consumed]
            try:
                line = raw_line.decode("ascii").strip()
            except UnicodeDecodeError:
                self.get_logger().warn("Dropped non-ASCII serial line")
                continue
            if not line:
                continue
            self._dispatch_line(line)

    def _dispatch_line(self, line: str) -> None:
        """Route a single decoded ASCII line to the right handler.

        Silent tolerance of mid-line fragments at startup: if the host
        opens /dev/star-mcu mid-stream, the first 'line' may be a tail
        fragment that does not start with 'E ' or '#'. Flood-logging
        these as warns spams Foxglove, so we downgrade unknowns to
        throttled-debug.
        """
        # Internal '\r' left over from a \\r\\n boundary split.
        line = line.lstrip("\r")
        if line.startswith("E "):
            PROM_RX_LINES.labels(kind="encoder").inc()
            self._handle_encoder_line(line)
        elif line.startswith("M "):
            PROM_RX_LINES.labels(kind="motor").inc()
            self._handle_motor_line(line)
        elif line.startswith("I "):
            PROM_RX_LINES.labels(kind="imu").inc()
            self._handle_imu_line(line)
        elif line.startswith("#"):
            PROM_RX_LINES.labels(kind="comment").inc()
            self.get_logger().debug(f"fw: {line}")
        else:
            PROM_PARSE_ERRORS.labels(kind="unknown_prefix").inc()
            self.get_logger().debug(f"Unknown/fragment: {line!r}")

    def _handle_encoder_line(self, line: str) -> None:
        """Parse an "E fl fr bl br ms" frame and publish odometry."""
        tokens = line.split()
        expected_token_count = 6
        if len(tokens) != expected_token_count:
            # Silent drop: USB hiccups cause concatenated / truncated
            # lines. Spamming Foxglove as warns is unhelpful; SLAM
            # toolbox recovers via scan-matching when some odom frames
            # are missing.
            PROM_PARSE_ERRORS.labels(kind="encoder_tokens").inc()
            self.get_logger().debug(
                f"drop malformed: tokens={len(tokens)} {line!r}"
            )
            return
        try:
            ticks_fl = int(tokens[1])
            ticks_fr = int(tokens[2])
            ticks_bl = int(tokens[3])
            ticks_br = int(tokens[4])
            ts_ms = int(tokens[5])
        except ValueError:
            PROM_PARSE_ERRORS.labels(kind="encoder_int").inc()
            self.get_logger().debug(f"drop non-integer: {line!r}")
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

        # Per-wheel encoder direction signs. Empirically determined on
        # 2026-04-22: at commanded +0.6 m/s (forward) for 3 s the robot
        # physically moved 32 in = 0.813 m forward. With FL/BL flipped,
        # odom read -0.678 m (wrong sign, 17% low). The right-side
        # quadrature A/B phase is swapped vs the left-side on this
        # chassis, so flip FR/BR instead.
        dticks_fr = -dticks_fr
        dticks_br = -dticks_br

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

        # Per-wheel linear velocity (signed m/s). Published as scalar
        # Float32 topics so Foxglove Gauge panels can be bound directly.
        wheel_velocities = {
            "fl": d_fl / dt_s,
            "fr": d_fr / dt_s,
            "bl": d_bl / dt_s,
            "br": d_br / dt_s,
        }
        for label, vel in wheel_velocities.items():
            msg = Float32()
            msg.data = float(vel)
            self._wheel_velocity_pubs[label].publish(msg)
            PROM_MOTOR_VELOCITY.labels(wheel=label).set(float(vel))

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

        # Odometry Prometheus metrics.
        PROM_ODOM_LINEAR_MPS.set(float(linear_vel))
        PROM_ODOM_ANGULAR_RPS.set(float(angular_vel))
        PROM_ODOM_POSE_X.set(float(self._pose_x))
        PROM_ODOM_POSE_Y.set(float(self._pose_y))
        PROM_ODOM_POSE_THETA.set(float(self._pose_theta))

        # Also broadcast the odom -> base_link transform so Nav2 / RViz /
        # Foxglove can plot the moving robot. Uses the same timestamp and
        # pose as the Odometry message above for consistency.
        tf_msg = TransformStamped()
        tf_msg.header.stamp = odom.header.stamp
        tf_msg.header.frame_id = FRAME_ODOM
        tf_msg.child_frame_id = FRAME_BASE_LINK
        tf_msg.transform.translation.x = self._pose_x
        tf_msg.transform.translation.y = self._pose_y
        tf_msg.transform.translation.z = 0.0
        tf_msg.transform.rotation = odom.pose.pose.orientation
        if self._tf_broadcaster is not None:
            self._tf_broadcaster.sendTransform(tf_msg)

    def _handle_motor_line(self, line: str) -> None:
        """Parse "M d_fl d_fr d_bl d_br f_fl f_fr f_bl f_br ms" and publish.

        Emits two topics:
          /motor/duty   - Float32MultiArray of per-motor duty in percent
                           (recovered by dividing the int16 tenths-of-a-
                           percent wire value by DUTY_TENTHS_PER_PERCENT)
          /motor/faults - UInt8MultiArray of per-motor DRV8263 fault bytes
        Array slot order for both messages is [FL FR BL BR] to match the
        firmware wire order (same convention as the E line).
        """
        tokens = line.split()
        expected_token_count = 10  # "M" + 4 duty + 4 fault + 1 ms
        if len(tokens) != expected_token_count:
            self.get_logger().debug(
                f"drop malformed M: tokens={len(tokens)} {line!r}"
            )
            PROM_PARSE_ERRORS.labels(kind="motor_tokens").inc()
            return
        try:
            duty_tenths = [int(tokens[1 + i]) for i in range(4)]
            # Clamp to uint8 range: stale-buffer bytes or line-fragment
            # parses occasionally yield negative values which
            # UInt8MultiArray rejects with OverflowError.
            faults = [max(0, min(255, int(tokens[5 + i]))) for i in range(4)]
            # ts_ms is read but not currently attached to the outgoing
            # messages: Float32MultiArray / UInt8MultiArray have no
            # header. Diagnostics consumers stamp on receipt.
            int(tokens[9])
        except ValueError:
            PROM_PARSE_ERRORS.labels(kind="motor_int").inc()
            self.get_logger().debug(f"drop non-integer M: {line!r}")
            return

        duty_msg = Float32MultiArray()
        duty_dim = MultiArrayDimension()
        duty_dim.label = "wheel_fl_fr_bl_br_percent"
        duty_dim.size = len(MOTOR_LABELS)
        duty_dim.stride = len(MOTOR_LABELS)
        duty_msg.layout.dim.append(duty_dim)
        duty_percents = [
            tenths / DUTY_TENTHS_PER_PERCENT for tenths in duty_tenths
        ]
        duty_msg.data = duty_percents
        self._motor_duty_pub.publish(duty_msg)

        # Per-wheel scalar duty (signed percent) for Foxglove Gauges.
        for label, duty in zip(MOTOR_LABELS, duty_percents):
            msg = Float32()
            msg.data = float(duty)
            self._wheel_duty_pubs[label].publish(msg)
            PROM_MOTOR_DUTY.labels(wheel=label).set(float(duty))

        # Per-wheel fault byte for Prometheus (alerts on != 0).
        for label, fault in zip(MOTOR_LABELS, faults):
            PROM_MOTOR_FAULT.labels(wheel=label).set(float(fault))

        fault_msg = UInt8MultiArray()
        fault_dim = MultiArrayDimension()
        fault_dim.label = "wheel_fl_fr_bl_br_drv8263_fault_byte"
        fault_dim.size = len(MOTOR_LABELS)
        fault_dim.stride = len(MOTOR_LABELS)
        fault_msg.layout.dim.append(fault_dim)
        fault_msg.data = faults
        self._motor_faults_pub.publish(fault_msg)

    def _handle_imu_line(self, line: str) -> None:
        """Parse an "I qw qx qy qz roll pitch heading gx gy gz ax ay az ms"
        frame and publish a sensor_msgs/Imu on /imu/data.

        All thirteen numeric fields are raw int16 values scaled by
        BNO055 register-native divisors. We convert here so the
        published Imu message is already in SI units (rad, rad/s,
        m/s^2) as required by REP-145. Euler angles are not published
        on the Imu topic (the orientation quaternion already carries
        attitude) but could be added as a separate diagnostic topic.

        Covariances are left at zero; downstream consumers (ekf_node,
        slam_toolbox) treat "cov[0] == 0" as "unknown -> use
        defaults", which is the right behavior until we bench a real
        noise estimate for this BNO055 unit.
        """
        tokens = line.split()
        expected_token_count = 15  # "I" + 13 int16 + 1 ms
        if len(tokens) != expected_token_count:
            self.get_logger().debug(
                f"drop malformed I: tokens={len(tokens)} {line!r}"
            )
            PROM_PARSE_ERRORS.labels(kind="imu_tokens").inc()
            return
        try:
            raw = [int(tokens[1 + i]) for i in range(13)]
            int(tokens[14])  # ts_ms currently unused; Imu stamps on receipt
        except ValueError:
            PROM_PARSE_ERRORS.labels(kind="imu_int").inc()
            self.get_logger().debug(f"drop non-integer I: {line!r}")
            return

        quat_w = raw[0] / IMU_SCALE_QUAT
        quat_x = raw[1] / IMU_SCALE_QUAT
        quat_y = raw[2] / IMU_SCALE_QUAT
        quat_z = raw[3] / IMU_SCALE_QUAT
        # BNO055 Euler output, deg. The quaternion above already carries
        # attitude for sensor_msgs/Imu, but human-readable Euler is
        # invaluable for Foxglove Gauge dashboards.
        roll_deg = raw[4] / IMU_SCALE_EULER_DEG
        pitch_deg = raw[5] / IMU_SCALE_EULER_DEG
        heading_deg = raw[6] / IMU_SCALE_EULER_DEG
        gyro_x_dps = raw[7] / IMU_SCALE_GYRO_DPS
        gyro_y_dps = raw[8] / IMU_SCALE_GYRO_DPS
        gyro_z_dps = raw[9] / IMU_SCALE_GYRO_DPS
        gyro_x_rad = math.radians(gyro_x_dps)
        gyro_y_rad = math.radians(gyro_y_dps)
        gyro_z_rad = math.radians(gyro_z_dps)
        acc_x = raw[10] / IMU_SCALE_ACC_MPS2
        acc_y = raw[11] / IMU_SCALE_ACC_MPS2
        acc_z = raw[12] / IMU_SCALE_ACC_MPS2

        # Compensate for the IMU's soldered-face-down mount. See the
        # IMU_MOUNT_FLIP_Y comment block. q_corr = (0, 0, 1, 0) as
        # (x,y,z,w); left-multiplying with q_raw = (qx,qy,qz,qw) gives:
        #   q_out.w = -qy,  q_out.x = qz,  q_out.y = qw,  q_out.z = -qx
        # and vectors transform as (x, y, z) -> (-x, y, -z).
        if IMU_MOUNT_FLIP_Y:
            quat_w, quat_x, quat_y, quat_z = (
                -quat_y, quat_z, quat_w, -quat_x,
            )
            roll_deg, pitch_deg, heading_deg = -roll_deg, 180.0 - pitch_deg, heading_deg
            gyro_x_rad = -gyro_x_rad
            gyro_z_rad = -gyro_z_rad
            acc_x = -acc_x
            acc_z = -acc_z

        imu_msg = Imu()
        imu_msg.header.stamp = self.get_clock().now().to_msg()
        imu_msg.header.frame_id = FRAME_IMU
        imu_msg.orientation.w = quat_w
        imu_msg.orientation.x = quat_x
        imu_msg.orientation.y = quat_y
        imu_msg.orientation.z = quat_z
        imu_msg.angular_velocity.x = gyro_x_rad
        imu_msg.angular_velocity.y = gyro_y_rad
        imu_msg.angular_velocity.z = gyro_z_rad
        imu_msg.linear_acceleration.x = acc_x
        imu_msg.linear_acceleration.y = acc_y
        imu_msg.linear_acceleration.z = acc_z
        # Covariances left at default-zero (REP-145: "unknown").
        self._imu_pub.publish(imu_msg)

        # Scalar mirror topics for Foxglove Gauge dials.
        scalar_values = {
            "roll_deg": roll_deg,
            "pitch_deg": pitch_deg,
            "heading_deg": heading_deg,
            "gyro_x_dps": gyro_x_dps,
            "gyro_y_dps": gyro_y_dps,
            "gyro_z_dps": gyro_z_dps,
            "accel_x_mps2": acc_x,
            "accel_y_mps2": acc_y,
            "accel_z_mps2": acc_z,
        }
        for name, value in scalar_values.items():
            msg = Float32()
            msg.data = float(value)
            self._imu_scalar_pubs[name].publish(msg)

        # Prometheus: split by (group, axis) labels so Grafana queries
        # stay compact, e.g. star_imu_gyro_dps{axis="z"}.
        PROM_IMU_ORIENTATION.labels(axis="roll").set(float(roll_deg))
        PROM_IMU_ORIENTATION.labels(axis="pitch").set(float(pitch_deg))
        PROM_IMU_ORIENTATION.labels(axis="heading").set(float(heading_deg))
        PROM_IMU_GYRO.labels(axis="x").set(float(gyro_x_dps))
        PROM_IMU_GYRO.labels(axis="y").set(float(gyro_y_dps))
        PROM_IMU_GYRO.labels(axis="z").set(float(gyro_z_dps))
        PROM_IMU_ACCEL.labels(axis="x").set(float(acc_x))
        PROM_IMU_ACCEL.labels(axis="y").set(float(acc_y))
        PROM_IMU_ACCEL.labels(axis="z").set(float(acc_z))

    # -- /scan -> Prometheus ----------------------------------------

    def _scan_cb(self, msg: LaserScan) -> None:
        """Summarize a LaserScan into Prometheus gauges.

        Emits (1) aggregate min/max/mean over valid rays, (2) an
        obstacle count under 1.0m, (3) the closest return and its
        bearing, and (4) a 36-bin angular profile (one bin per 10 deg)
        containing the minimum range in each sector. Invalid returns
        (inf, NaN, <range_min, >range_max) are skipped.
        """
        proximity_threshold_m = 1.0

        ranges = msg.ranges
        rmin = msg.range_min
        rmax = msg.range_max
        angle = msg.angle_min
        step = msg.angle_increment

        # Accumulators.
        valid_ranges = []
        obstacle_count = 0
        closest_r = float("inf")
        closest_bearing_rad = 0.0

        for r in ranges:
            a = angle
            angle += step
            if not math.isfinite(r) or r < rmin or r > rmax:
                continue
            valid_ranges.append(r)
            if r < closest_r:
                closest_r = r
                closest_bearing_rad = a
            if r < proximity_threshold_m:
                obstacle_count += 1

        if valid_ranges:
            PROM_SCAN_RANGE_STAT.labels(stat="min").set(min(valid_ranges))
            PROM_SCAN_RANGE_STAT.labels(stat="max").set(max(valid_ranges))
            PROM_SCAN_RANGE_STAT.labels(stat="mean").set(
                sum(valid_ranges) / len(valid_ranges)
            )
            PROM_SCAN_OBSTACLE_COUNT.set(obstacle_count)
            PROM_SCAN_CLOSEST_RANGE.set(closest_r)
            PROM_SCAN_CLOSEST_BEARING.set(math.degrees(closest_bearing_rad))

    # -- /map -> PNG cache ------------------------------------------

    def _map_cb(self, msg: OccupancyGrid) -> None:
        """Cache the latest OccupancyGrid for the PNG HTTP handler."""
        global _latest_map
        with _map_lock:
            _latest_map = msg

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

    # Prometheus /metrics HTTP server bound to all interfaces so a
    # Prometheus scraper on another host (Grafana server) can pull
    # metrics directly. Lives for the duration of the node process.
    try:
        start_http_server(PROMETHEUS_PORT, addr="0.0.0.0")
    except OSError as exc:
        # Port collision or permission issue: log and continue -- losing
        # metrics is non-fatal, the bridge itself must stay up.
        rclpy.logging.get_logger("star_simple_bridge").warn(
            f"Could not start Prometheus server on :{PROMETHEUS_PORT}: {exc}"
        )

    # Serve the SLAM /map snapshot as PNG so Grafana can embed it.
    try:
        _start_map_http_server()
    except OSError as exc:
        rclpy.logging.get_logger("star_simple_bridge").warn(
            f"Could not start map PNG server on :{MAP_HTTP_PORT}: {exc}"
        )

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
