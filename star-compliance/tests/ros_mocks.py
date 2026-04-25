"""
Mock-rclpy harness for unit-testing compliance nodes off-robot.

Provides sys.modules-level replacements for every ROS 2 Python package a
compliance node touches during import and construction. Tests call
`install_ros_mocks()` BEFORE importing the node under test.

Design goals:

- Node classes must import cleanly with the mocks in place.
- The mocked Node base class records every `create_subscription`,
  `create_publisher`, `create_timer`, and `declare_parameter` call so
  tests can inspect topic names, message types, and QoS.
- Parameter reads return the declared defaults by default; a per-test
  override is available via `node.set_mock_parameter("name", value)`.
- Publish / log calls are recorded on the node so tests assert against
  them directly.

Keep this file out of production imports; it deliberately fabricates
ROS 2 types and should never be installed alongside real rclpy.
"""

from __future__ import annotations

import sys
import time
from dataclasses import dataclass, field
from types import ModuleType
from typing import Any
from unittest.mock import MagicMock


# ---------------------------------------------------------------------
# Minimal rclpy base classes our nodes inherit from
# ---------------------------------------------------------------------


class _FakeSubscription:
    def __init__(self, msg_type, topic, callback, qos):
        self.msg_type = msg_type
        self.topic = topic
        self.callback = callback
        self.qos = qos


class _FakePublisher:
    def __init__(self, msg_type, topic, qos):
        self.msg_type = msg_type
        self.topic = topic
        self.qos = qos
        self.sent: list[Any] = []

    def publish(self, msg):
        self.sent.append(msg)


class _FakeTimer:
    def __init__(self, period_sec, callback):
        self.period_sec = period_sec
        self.callback = callback


@dataclass
class _ParameterValue:
    value: Any


class _FakeClient:
    def __init__(self, srv_type, name):
        self.srv_type = srv_type
        self.name = name

    def service_is_ready(self):
        return False

    def call_async(self, req):
        return MagicMock()


class FakeClock:
    def __init__(self):
        self._start = time.time()

    def now(self):
        class _Now:
            def __init__(self, sec: float):
                self.nanoseconds = int(sec * 1e9)
            def to_msg(self):
                stamp = MagicMock()
                stamp.sec = int(self.nanoseconds / 1e9)
                stamp.nanosec = int(self.nanoseconds % 1_000_000_000)
                return stamp

        return _Now(time.time())


class FakeLogger:
    def __init__(self):
        self.records: list[tuple[str, str]] = []

    def info(self, msg: str, **_kwargs) -> None:
        self.records.append(("info", msg))

    def warn(self, msg: str, **_kwargs) -> None:
        self.records.append(("warn", msg))

    def warning(self, msg: str, **_kwargs) -> None:
        self.warn(msg)

    def error(self, msg: str, **_kwargs) -> None:
        self.records.append(("error", msg))

    def debug(self, msg: str, **_kwargs) -> None:
        self.records.append(("debug", msg))


class FakeNode:
    """Replacement for rclpy.node.Node that records all registrations."""

    def __init__(self, name: str):
        self.name = name
        self.subscriptions: list[_FakeSubscription] = []
        self.publishers: list[_FakePublisher] = []
        self.timers: list[_FakeTimer] = []
        self.parameters: dict[str, Any] = {}
        self._logger = FakeLogger()
        self._clock = FakeClock()
        self._clients: list[_FakeClient] = []

    # -- rclpy Node API used by our nodes -------------------------------

    def declare_parameter(self, name: str, default=None):
        if name not in self.parameters:
            self.parameters[name] = default
        return _ParameterValue(self.parameters[name])

    def get_parameter(self, name: str):
        return _ParameterValue(self.parameters.get(name))

    def create_subscription(self, msg_type, topic, callback, qos=10):
        sub = _FakeSubscription(msg_type, topic, callback, qos)
        self.subscriptions.append(sub)
        return sub

    def create_publisher(self, msg_type, topic, qos=10):
        pub = _FakePublisher(msg_type, topic, qos)
        self.publishers.append(pub)
        return pub

    def create_timer(self, period_sec, callback):
        timer = _FakeTimer(period_sec, callback)
        self.timers.append(timer)
        return timer

    def create_client(self, srv_type, name):
        client = _FakeClient(srv_type, name)
        self._clients.append(client)
        return client

    def get_logger(self):
        return self._logger

    def get_clock(self):
        return self._clock

    def destroy_node(self):
        pass

    # -- Test helpers ---------------------------------------------------

    def set_mock_parameter(self, name: str, value: Any) -> None:
        self.parameters[name] = value

    def find_subscription(self, topic: str) -> _FakeSubscription:
        for sub in self.subscriptions:
            if sub.topic == topic:
                return sub
        raise AssertionError(f"no subscription for {topic}")

    def find_publisher(self, topic: str) -> _FakePublisher:
        for pub in self.publishers:
            if pub.topic == topic:
                return pub
        raise AssertionError(f"no publisher for {topic}")


def _make_module(name: str) -> ModuleType:
    mod = ModuleType(name)
    sys.modules[name] = mod
    return mod


def install_ros_mocks() -> None:
    """Install sys.modules mocks for every ROS 2 package our nodes touch.

    Idempotent - re-calling leaves previously-installed mocks in place.
    Call this at the top of any test file that imports a compliance node.
    """
    if "rclpy" in sys.modules and isinstance(sys.modules["rclpy"], ModuleType):
        # A prior test already installed mocks.
        if hasattr(sys.modules["rclpy"], "_STAR_MOCK"):
            return

    # --- rclpy -------------------------------------------------------
    rclpy = _make_module("rclpy")
    rclpy._STAR_MOCK = True
    rclpy.init = MagicMock()
    rclpy.shutdown = MagicMock()
    rclpy.spin = MagicMock()

    rclpy_node = _make_module("rclpy.node")
    rclpy_node.Node = FakeNode

    rclpy_qos = _make_module("rclpy.qos")

    class _Profile:
        def __init__(self, reliability=None, durability=None, depth=10):
            self.reliability = reliability
            self.durability = durability
            self.depth = depth

    rclpy_qos.QoSProfile = _Profile
    rclpy_qos.ReliabilityPolicy = MagicMock(BEST_EFFORT=1, RELIABLE=2)
    rclpy_qos.DurabilityPolicy = MagicMock(VOLATILE=1, TRANSIENT_LOCAL=2)

    rclpy_parameter = _make_module("rclpy.parameter")

    class _Parameter:
        class Type:
            BOOL = "bool"
            INTEGER = "int"
            DOUBLE = "double"
            STRING = "string"

        def __init__(self, name, type_, value):
            self.name = name
            self.type_ = type_
            self.value = value

        def to_parameter_msg(self):
            return MagicMock(name=self.name, value=self.value)

    rclpy_parameter.Parameter = _Parameter

    # --- message packages --------------------------------------------
    for pkg in (
        "sensor_msgs", "sensor_msgs.msg",
        "nav_msgs", "nav_msgs.msg",
        "geometry_msgs", "geometry_msgs.msg",
        "std_msgs", "std_msgs.msg",
        "diagnostic_msgs", "diagnostic_msgs.msg",
        "rcl_interfaces", "rcl_interfaces.srv",
        "cv_bridge",
        "sensor_msgs_py", "sensor_msgs_py.point_cloud2",
        "star_compliance_msgs", "star_compliance_msgs.msg",
        "vision_msgs", "vision_msgs.msg",
        "tf2_ros",
        "rclpy.duration", "rclpy.time",
    ):
        if pkg not in sys.modules:
            _make_module(pkg)
            sys.modules[pkg].__dict__.update({"_STAR_MOCK": True})

    # Frequently-used message factories. MagicMock() works as a
    # constructor that returns a fresh MagicMock each call.
    for msg_cls in (
        "LaserScan", "Imu", "PointCloud2", "Image", "Range",
    ):
        setattr(sys.modules["sensor_msgs.msg"], msg_cls, MagicMock())
    for msg_cls in ("Odometry", "OccupancyGrid"):
        setattr(sys.modules["nav_msgs.msg"], msg_cls, MagicMock())
    for msg_cls in ("PoseStamped", "Pose", "Twist"):
        setattr(sys.modules["geometry_msgs.msg"], msg_cls, MagicMock())
    for msg_cls in ("String", "Bool"):
        setattr(sys.modules["std_msgs.msg"], msg_cls, MagicMock())
    setattr(sys.modules["rcl_interfaces.srv"], "SetParameters", MagicMock())

    # Custom compliance messages: MagicMock() auto-creates nested
    # attributes on access, which matches how our nodes assemble
    # messages (e.g., msg.header.stamp = ..., msg.pose.pose.position.x
    # = ...). Any setattr succeeds, and .objects.append(...) works
    # because MagicMock gives you a MagicMock() list-like child.
    for msg_cls in (
        "DoorwayMeasurement", "ThresholdMeasurement",
        "ProtrudingObject", "ProtrudingObjectArray",
        "DynamicObstacle", "DynamicObstacleArray",
        "PathBlockage",
    ):
        setattr(sys.modules["star_compliance_msgs.msg"], msg_cls, MagicMock)

    # vision_msgs Detection3DArray is consumed by the YOLO-aware
    # branches in protruding_objects_node and dynamic_obstacle_node.
    setattr(sys.modules["vision_msgs.msg"], "Detection3DArray", MagicMock)

    # tf2_ros: a Buffer with a configurable lookup_transform return,
    # plus a stub TransformListener. Tests poke node._tf_buffer
    # directly to inject a TransformStamped.
    class _FakeTfBuffer:
        def __init__(self) -> None:
            self.transform_to_return = None  # tests assign a stub.

        def lookup_transform(self, target, source, time, timeout=None):
            if self.transform_to_return is None:
                raise RuntimeError("test did not set transform_to_return")
            return self.transform_to_return

    class _FakeTransformListener:
        def __init__(self, buffer, node):
            self.buffer = buffer
            self.node = node

    setattr(sys.modules["tf2_ros"], "Buffer", _FakeTfBuffer)
    setattr(sys.modules["tf2_ros"],
            "TransformListener", _FakeTransformListener)

    # rclpy.time.Time and rclpy.duration.Duration are referenced by
    # the YOLO callbacks. Stubs satisfy the constructor and the
    # lookup_transform timeout argument. Both submodules must also be
    # exposed as attributes of `rclpy` so `rclpy.time.Time()` works
    # after a plain `import rclpy`.
    sys.modules["rclpy.time"].Time = MagicMock
    sys.modules["rclpy.duration"].Duration = MagicMock
    rclpy.time = sys.modules["rclpy.time"]
    rclpy.duration = sys.modules["rclpy.duration"]
