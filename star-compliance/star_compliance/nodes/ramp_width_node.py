"""Ramp width node - ARCHITECTED (not implemented for the capstone
demo, specified for deployment phase).

ADA 2010 Standards section 405.5: ramp runs shall have a clear width
of 36 inches minimum.

Designed algorithm:
  1. Reuse the ramp plane identified by ramp_slope_node's RANSAC fit.
  2. Project the inlier points onto the plane to obtain a 2D polygon.
  3. Compute the principal axis of the polygon (SVD on the 2D
     coordinates).
  4. The minimum width perpendicular to the principal axis is the
     ramp width.
  5. Flag if < 36 inches.

This node is a placeholder so the ROS2 package can launch with the
architected topology in place.
"""

from __future__ import annotations

import rclpy
from rclpy.node import Node


ADA_RAMP_WIDTH_THRESHOLD_IN = 36.0


class RampWidthNode(Node):
    def __init__(self) -> None:
        super().__init__("star_ramp_width_node")
        self.get_logger().warn(
            "ramp_width_node is ARCHITECTED only. Not implemented in the capstone."
        )


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = RampWidthNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
