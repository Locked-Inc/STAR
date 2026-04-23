"""STAR sandbox serial bridge: RX72N (FIT-untested firmware) <-> ROS2 Jazzy.

Translates the binary serial protocol defined in
fit-module-untested/src/serial_proto.h to / from ROS2 topics:

    /imu/data   sensor_msgs/Imu      (published, 100 Hz from RX72N)
    /cmd_vel    geometry_msgs/Twist  (subscribed, sent as MOTOR_CMD)

If the on-the-wire format here drifts from serial_proto.h, the link goes silent.

Run:
    ros2 run star_serial_bridge star_serial_bridge --ros-args \\
        -p port:=/dev/ttyACM0 -p baud:=921600

Differential-drive kinematics: maps Twist.linear.x and Twist.angular.z to four
wheel duties using a simple skid-steer model (front and back wheels mirrored
left/right). Wheel base + max linear speed are configurable parameters.
"""

import math
import struct
import threading
from dataclasses import dataclass

import rclpy
import serial
from geometry_msgs.msg import Twist
from rclpy.node import Node
from rclpy.qos import QoSPresetProfiles
from sensor_msgs.msg import Imu

# ---- Wire constants (mirror serial_proto.h) ---------------------------------

SYNC0 = 0xAA
SYNC1 = 0x55
TYPE_IMU_SAMPLE = 0x01
TYPE_MOTOR_CMD  = 0x02
TYPE_ESTOP      = 0x03
TYPE_STATUS     = 0x04
MAX_PAYLOAD     = 32

IMU_PAYLOAD_FMT     = "<6fI"     # accel xyz + gyro xyz + uint32 ts_ms
IMU_PAYLOAD_LEN     = struct.calcsize(IMU_PAYLOAD_FMT)
MOTOR_CMD_FMT       = "<4h"      # int16 duty[4]
MOTOR_CMD_LEN       = struct.calcsize(MOTOR_CMD_FMT)
STATUS_FMT          = "<BBI"
STATUS_LEN          = struct.calcsize(STATUS_FMT)


# ---- CRC-8 (Dallas/Maxim, poly 0x07, init 0x00) -----------------------------

def _build_crc_table():
    table = bytearray(256)
    for i in range(256):
        c = i
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if (c & 0x80) else (c << 1) & 0xFF
        table[i] = c
    return bytes(table)

_CRC8_TABLE = _build_crc_table()

def crc8(data: bytes) -> int:
    c = 0
    for b in data:
        c = _CRC8_TABLE[c ^ b]
    return c


# ---- Frame encoding ---------------------------------------------------------

def encode_frame(frame_type: int, payload: bytes) -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too long")
    head = bytes([SYNC0, SYNC1, frame_type, len(payload)])
    body = head[2:] + payload  # CRC covers TYPE + LEN + payload
    return head + payload + bytes([crc8(body)])


# ---- Streaming decoder (mirror of proto_rx_feed) ----------------------------

@dataclass
class _RxState:
    state: int = 0       # 0=sync0 1=sync1 2=type 3=len 4=payload 5=crc
    ftype: int = 0
    flen: int = 0
    payload: bytearray = None  # type: ignore

    def reset(self):
        self.state = 0
        self.payload = bytearray()


class FrameDecoder:
    """Yield (frame_type, payload_bytes) tuples as bytes are fed in."""

    def __init__(self):
        self._st = _RxState()
        self._st.reset()

    def feed(self, b: int):
        st = self._st
        if st.state == 0:
            if b == SYNC0:
                st.state = 1
            return None
        if st.state == 1:
            if b == SYNC1:
                st.state = 2
            elif b != SYNC0:
                st.state = 0
            return None
        if st.state == 2:
            st.ftype = b
            st.state = 3
            return None
        if st.state == 3:
            if b > MAX_PAYLOAD:
                st.reset()
                return None
            st.flen = b
            st.payload = bytearray()
            st.state = 5 if b == 0 else 4
            return None
        if st.state == 4:
            st.payload.append(b)
            if len(st.payload) >= st.flen:
                st.state = 5
            return None
        if st.state == 5:
            buf = bytes([st.ftype, st.flen]) + bytes(st.payload)
            ok = (b == crc8(buf))
            ftype, payload = st.ftype, bytes(st.payload)
            st.reset()
            if ok:
                return (ftype, payload)
            return None
        st.reset()
        return None


# ---- ROS2 node --------------------------------------------------------------

class StarSerialBridge(Node):
    def __init__(self):
        super().__init__("star_serial_bridge")

        self.declare_parameter("port", "/dev/ttyACM0")
        # 115200 matches the firmware default (921600 would give ~1.7% BRR
        # error at PCLKB=60 MHz on the RX72N, per the production HAL).
        self.declare_parameter("baud", 115200)
        self.declare_parameter("frame_id", "imu_link")
        self.declare_parameter("wheel_base_m", 0.20)        # m, left-right wheel separation
        self.declare_parameter("max_wheel_mps", 0.5)        # m/s at full duty

        port = self.get_parameter("port").value
        baud = self.get_parameter("baud").value
        self.frame_id = self.get_parameter("frame_id").value
        self.wheel_base = float(self.get_parameter("wheel_base_m").value)
        self.max_wheel = float(self.get_parameter("max_wheel_mps").value)

        self.get_logger().info(f"opening {port} @ {baud}")
        self.ser = serial.Serial(port, baud, timeout=0.05)

        self.imu_pub = self.create_publisher(
            Imu, "/imu/data", QoSPresetProfiles.SENSOR_DATA.value
        )
        self.create_subscription(Twist, "/cmd_vel", self._on_cmd_vel, 10)

        self.decoder = FrameDecoder()
        self._stop = threading.Event()
        self._rx_thread = threading.Thread(target=self._reader_loop, daemon=True)
        self._rx_thread.start()

    # ---- shutdown ----------------------------------------------------------
    def shutdown(self):
        self._stop.set()
        try:
            self.ser.write(encode_frame(TYPE_ESTOP, b""))
            self.ser.flush()
        except Exception:
            pass
        try:
            self.ser.close()
        except Exception:
            pass

    # ---- RX loop -----------------------------------------------------------
    def _reader_loop(self):
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(64)
            except serial.SerialException as exc:
                self.get_logger().error(f"serial read error: {exc}")
                return
            for b in chunk:
                got = self.decoder.feed(b)
                if got is None:
                    continue
                ftype, payload = got
                if ftype == TYPE_IMU_SAMPLE and len(payload) == IMU_PAYLOAD_LEN:
                    self._publish_imu(payload)
                elif ftype == TYPE_STATUS and len(payload) == STATUS_LEN:
                    imu_ok, motor_armed, uptime = struct.unpack(STATUS_FMT, payload)
                    self.get_logger().info(
                        f"STATUS imu_ok={imu_ok} armed={motor_armed} up={uptime} ms"
                    )

    def _publish_imu(self, payload: bytes):
        ax, ay, az, gx, gy, gz, ts_ms = struct.unpack(IMU_PAYLOAD_FMT, payload)
        msg = Imu()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.header.frame_id = self.frame_id
        msg.linear_acceleration.x = ax
        msg.linear_acceleration.y = ay
        msg.linear_acceleration.z = az
        msg.angular_velocity.x = gx
        msg.angular_velocity.y = gy
        msg.angular_velocity.z = gz
        # No orientation in IMU mode -- mark covariance[0] = -1 per REP-145.
        msg.orientation_covariance[0] = -1.0
        # Conservative covariances; tune in ekf.yaml not here.
        msg.linear_acceleration_covariance[0] = 0.04
        msg.linear_acceleration_covariance[4] = 0.04
        msg.linear_acceleration_covariance[8] = 0.04
        msg.angular_velocity_covariance[0] = 0.02
        msg.angular_velocity_covariance[4] = 0.02
        msg.angular_velocity_covariance[8] = 0.02
        self.imu_pub.publish(msg)

    # ---- TX path -----------------------------------------------------------
    def _on_cmd_vel(self, msg: Twist):
        # Skid-steer: left wheels = vx - w*L/2, right wheels = vx + w*L/2.
        v = float(msg.linear.x)
        w = float(msg.angular.z)
        v_left  = v - 0.5 * w * self.wheel_base
        v_right = v + 0.5 * w * self.wheel_base

        # Map m/s to signed duty in [-10000, +10000].
        def to_duty(v_mps: float) -> int:
            d = int(round((v_mps / self.max_wheel) * 10000))
            return max(-10000, min(10000, d))

        # Motor index map (per memory project_motor_spin_test_workflow.md):
        #   0 = FL, 1 = FR, 2 = BR, 3 = BL
        duties = (
            to_duty(v_left),   # 0 FL
            to_duty(v_right),  # 1 FR
            to_duty(v_right),  # 2 BR
            to_duty(v_left),   # 3 BL
        )
        payload = struct.pack(MOTOR_CMD_FMT, *duties)
        try:
            self.ser.write(encode_frame(TYPE_MOTOR_CMD, payload))
        except serial.SerialException as exc:
            self.get_logger().error(f"serial write error: {exc}")


def main(args=None):
    rclpy.init(args=args)
    node = StarSerialBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.shutdown()
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
