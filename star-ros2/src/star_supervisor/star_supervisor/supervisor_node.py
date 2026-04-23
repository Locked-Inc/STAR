"""STAR supervisor: arbiter for manual teleop vs autonomy vs e-stop.

Topology (topics split along cmd vs state axes to avoid QoS mismatches):

    /cmd_vel           (manual teleop, VOLATILE)     ---\\
                                                          >---[arbiter]---> /cmd_vel_out  ---> motor bridge
    /nav2/cmd_vel      (Nav2 final output, VOLATILE) ---/

    /star/autonomy_enable  (Bool, VOLATILE) -- operator command input
    /star/estop            (Bool, VOLATILE) -- operator command input

    /star/state/autonomy_enable  (Bool, TRANSIENT_LOCAL) -- current flag state,
    /star/state/estop            (Bool, TRANSIENT_LOCAL)    latched so newly
                                                            connecting indicators
                                                            see the value immediately

Why split cmd (VOLATILE) and state (TRANSIENT_LOCAL) topics:
  ROS2 QoS requires subscribers to be compatible with ALL publishers
  on a topic. If the supervisor publishes /star/estop with
  TRANSIENT_LOCAL and Lichtblick's Publish panel writes to the same
  topic with its default VOLATILE, the indicator subscriber with
  TRANSIENT_LOCAL durability rejects the Publish-panel stream and
  misses operator commands. Splitting cmd and state topics means both
  sides use the QoS that fits their role.

And a parallel goal-forwarding path so a user "2D Goal Pose" click in
Lichtblick / Foxglove only reaches Nav2 when autonomy is armed:

    /goal_pose  --[forward if autonomy && !estop]-->  /nav2/goal_pose

This node intentionally does NOT also drive the hardware directly.
The motor bridge ALSO subscribes to /star/estop and zeros its serial
output -- two independent safety layers.
"""

from __future__ import annotations

import rclpy
from geometry_msgs.msg import PoseStamped, Twist
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from std_msgs.msg import Bool

# Input / output topic names -- keep in sync with the Lichtblick layout
# panels on the cluster side.
TOPIC_MANUAL_CMD = "/cmd_vel"
TOPIC_NAV2_CMD = "/nav2/cmd_vel"
TOPIC_GATED_CMD_OUT = "/cmd_vel_out"

# Operator command topics (VOLATILE, what Lichtblick's Publish panels write).
TOPIC_AUTONOMY_CMD = "/star/autonomy_enable"
TOPIC_ESTOP_CMD = "/star/estop"

# Latched state echoes (TRANSIENT_LOCAL, what indicator panels subscribe to).
# Separate topic names so the command and state QoS profiles do not clash.
TOPIC_AUTONOMY_STATE = "/star/state/autonomy_enable"
TOPIC_ESTOP_STATE = "/star/state/estop"

TOPIC_GOAL_IN = "/goal_pose"
TOPIC_GOAL_OUT_NAV2 = "/nav2/goal_pose"

# Latched QoS -- late subscribers (Lichtblick panels) pick up the current
# state immediately on connect.
LATCHED_QOS = QoSProfile(
    depth=1,
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
)


class SupervisorNode(Node):
    """Single-owner mux between teleop, Nav2, and an e-stop gate."""

    def __init__(self) -> None:
        super().__init__("star_supervisor")

        self._autonomy_enabled = False
        self._estop_active = False

        self._cmd_pub = self.create_publisher(Twist, TOPIC_GATED_CMD_OUT, 10)
        self._goal_pub = self.create_publisher(PoseStamped, TOPIC_GOAL_OUT_NAV2, 10)

        # Latched state-echo publishers on DEDICATED topic names so
        # indicator panels always receive the current accepted value on
        # connect, without conflicting with Lichtblick's VOLATILE
        # Publish-panel writes to the plain command topic.
        self._autonomy_echo_pub = self.create_publisher(
            Bool, TOPIC_AUTONOMY_STATE, LATCHED_QOS,
        )
        self._estop_echo_pub = self.create_publisher(
            Bool, TOPIC_ESTOP_STATE, LATCHED_QOS,
        )

        # Command-input subscriptions use default (VOLATILE, depth=10)
        # so they can compatibly receive from Lichtblick's Publish
        # panels. Operator command semantics: "fire-and-forget".
        self.create_subscription(
            Twist, TOPIC_MANUAL_CMD, self._on_manual_cmd, 10,
        )
        self.create_subscription(
            Twist, TOPIC_NAV2_CMD, self._on_nav2_cmd, 10,
        )
        self.create_subscription(
            Bool, TOPIC_AUTONOMY_CMD, self._on_autonomy, 10,
        )
        self.create_subscription(
            Bool, TOPIC_ESTOP_CMD, self._on_estop, 10,
        )
        self.create_subscription(
            PoseStamped, TOPIC_GOAL_IN, self._on_goal, 10,
        )

        # Seed indicators with the boot defaults (autonomy off, not estopped).
        self._publish_state_echo()
        self.get_logger().info(
            f"supervisor ready. autonomy={self._autonomy_enabled} "
            f"estop={self._estop_active} "
            f"cmd_out={TOPIC_GATED_CMD_OUT} "
            f"autonomy_state={TOPIC_AUTONOMY_STATE} "
            f"estop_state={TOPIC_ESTOP_STATE}"
        )

    # -- state transitions --------------------------------------------

    def _on_autonomy(self, msg: Bool) -> None:
        """Handle autonomy-arm toggle from Lichtblick / operator."""
        new_val = bool(msg.data)
        if new_val == self._autonomy_enabled:
            return
        self._autonomy_enabled = new_val
        self.get_logger().info(
            f"autonomy_enable -> {new_val}. "
            f"Active cmd_vel source: "
            f"{'NAV2' if new_val and not self._estop_active else 'MANUAL'}"
        )
        # When toggling modes, emit a single zero Twist so the robot
        # doesn't coast on a stale command from the previous owner.
        self._cmd_pub.publish(Twist())
        self._publish_state_echo()

    def _on_estop(self, msg: Bool) -> None:
        """Handle e-stop toggle."""
        new_val = bool(msg.data)
        if new_val == self._estop_active:
            return
        self._estop_active = new_val
        if new_val:
            self.get_logger().warn("E-STOP ENGAGED -- zeroing cmd_vel_out")
            self._cmd_pub.publish(Twist())
        else:
            self.get_logger().info("E-STOP CLEARED")
        self._publish_state_echo()

    # -- cmd_vel arbitration ------------------------------------------

    def _on_manual_cmd(self, msg: Twist) -> None:
        """Forward manual Twist only when autonomy is off AND no e-stop."""
        if self._estop_active or self._autonomy_enabled:
            return
        self._cmd_pub.publish(msg)

    def _on_nav2_cmd(self, msg: Twist) -> None:
        """Forward Nav2 Twist only when autonomy is on AND no e-stop."""
        if self._estop_active or not self._autonomy_enabled:
            return
        self._cmd_pub.publish(msg)

    # -- goal forwarding ----------------------------------------------

    def _on_goal(self, msg: PoseStamped) -> None:
        """Forward a 2D goal to Nav2 only while autonomy is armed."""
        if self._estop_active:
            self.get_logger().warn("Goal rejected: e-stop active")
            return
        if not self._autonomy_enabled:
            self.get_logger().warn("Goal rejected: autonomy disabled")
            return
        self._goal_pub.publish(msg)

    # -- indicator echoing --------------------------------------------

    def _publish_state_echo(self) -> None:
        """Republish the current flag state on the same topics.

        Late subscribers (Lichtblick just-connected) use transient_local
        to read the last message; this seeds that message without
        waiting for the next operator click.
        """
        auto_msg = Bool()
        auto_msg.data = self._autonomy_enabled
        self._autonomy_echo_pub.publish(auto_msg)

        estop_msg = Bool()
        estop_msg.data = self._estop_active
        self._estop_echo_pub.publish(estop_msg)


def main(args=None) -> None:
    """ament_python console_scripts entry point."""
    rclpy.init(args=args)
    node = None
    try:
        node = SupervisorNode()
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node is not None:
            node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
