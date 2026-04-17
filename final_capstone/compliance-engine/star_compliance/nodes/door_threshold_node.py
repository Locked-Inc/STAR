"""Door threshold node - ARCHITECTED.

ADA 2010 Standards section 404.2.5: thresholds, if provided at
doorways, shall be 1/2 inch high maximum. Raised thresholds and floor
level changes at accessible doorways shall be beveled with a slope not
steeper than 1:2.

Designed algorithm:
  1. When the robot is centered in a doorway (cross-reference
     door_clear_width_node), sample a dense vertical profile across
     the door bottom.
  2. Use both LiDAR horizontal returns (at the lowest plane) and
     stereo depth disparity on the floor region.
  3. The maximum vertical step within the door footprint is the
     threshold height.
  4. If > 0.5 inch, flag a violation.

Depends on door_clear_width_node and a live stereo driver.
Placeholder only.
"""

from __future__ import annotations

import rclpy
from rclpy.node import Node


ADA_DOOR_THRESHOLD_THRESHOLD_IN = 0.5


class DoorThresholdNode(Node):
    def __init__(self) -> None:
        super().__init__("star_door_threshold_node")
        self.get_logger().warn(
            "door_threshold_node is ARCHITECTED only. Not implemented in the capstone."
        )


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
