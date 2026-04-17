"""
CPU safety-net monitor for the STAR compliance engine.

Reads the Pi 5's 1-minute load average and, when it exceeds 80%,
sets `/star_protruding_objects_node/enabled` to false so the heavy
ADA 307 processing stops. Re-enables when load drops below 50%.

Also publishes a plain `/compliance/monitor/status` string so the UI
can show which nodes are currently active or throttled.
"""

from __future__ import annotations

import os

import rclpy
from rclpy.node import Node
from rclpy.parameter import Parameter
from rcl_interfaces.srv import SetParameters
from std_msgs.msg import String


HIGH_LOAD_THRESHOLD = 0.80
LOW_LOAD_THRESHOLD = 0.50
SAMPLE_INTERVAL_SEC = 5.0


class ComplianceMonitorNode(Node):

    def __init__(self) -> None:
        super().__init__("star_compliance_monitor_node")

        self.declare_parameter("cpu_count", max(1, os.cpu_count() or 4))
        self._cpu_count = int(self.get_parameter("cpu_count").value)

        self._throttled_ada_307 = False
        self._status_pub = self.create_publisher(
            String, "/compliance/monitor/status", 10
        )
        self._timer = self.create_timer(
            SAMPLE_INTERVAL_SEC, self._tick
        )

        self._set_param_client = self.create_client(
            SetParameters, "/star_protruding_objects_node/set_parameters"
        )
        self.get_logger().info(
            "star_compliance_monitor_node ready "
            f"(cpu_count={self._cpu_count}, "
            f"throttle>{HIGH_LOAD_THRESHOLD:.0%}, "
            f"release<{LOW_LOAD_THRESHOLD:.0%})"
        )

    def _tick(self) -> None:
        load1 = self._one_minute_load()
        norm = load1 / self._cpu_count
        if norm > HIGH_LOAD_THRESHOLD and not self._throttled_ada_307:
            self._set_ada_307_enabled(False)
            self._throttled_ada_307 = True
            self._publish_status(f"throttled ada_307 (load={norm:.2f})")
            self.get_logger().warn(
                f"CPU load {norm:.2f} > {HIGH_LOAD_THRESHOLD} "
                f"- disabling ADA 307 node"
            )
        elif norm < LOW_LOAD_THRESHOLD and self._throttled_ada_307:
            self._set_ada_307_enabled(True)
            self._throttled_ada_307 = False
            self._publish_status(f"ada_307 re-enabled (load={norm:.2f})")
            self.get_logger().info(
                f"CPU load {norm:.2f} < {LOW_LOAD_THRESHOLD} "
                f"- re-enabling ADA 307 node"
            )
        else:
            self._publish_status(
                f"nominal (load={norm:.2f}, ada_307="
                f"{'off' if self._throttled_ada_307 else 'on'})"
            )

    def _set_ada_307_enabled(self, enabled: bool) -> None:
        if not self._set_param_client.service_is_ready():
            self.get_logger().warn(
                "protruding_objects_node SetParameters service not ready; "
                "skipping toggle"
            )
            return
        req = SetParameters.Request()
        param = Parameter("enabled", Parameter.Type.BOOL, enabled)
        req.parameters = [param.to_parameter_msg()]
        self._set_param_client.call_async(req)

    def _publish_status(self, text: str) -> None:
        msg = String()
        msg.data = text
        self._status_pub.publish(msg)

    @staticmethod
    def _one_minute_load() -> float:
        try:
            return os.getloadavg()[0]
        except (AttributeError, OSError):
            return 0.0


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = ComplianceMonitorNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
