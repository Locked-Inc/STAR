"""Ramp landing node - ARCHITECTED.

ADA 2010 Standards section 405.7: ramps require landings at the top
and bottom with a minimum length of 60 inches in the direction of
travel and a width at least as wide as the widest ramp run leading to
them (60 inches minimum for accessible routes).

Designed algorithm:
  1. From ramp_slope_node, get the ramp plane and its bounding polygon.
  2. Identify flat regions adjacent to the ramp endpoints: surface
     normal within 1 deg of vertical, using the same Open3D RANSAC
     segmentation.
  3. For each ramp end, fit the largest axis-aligned inscribed
     rectangle inside the contiguous flat region.
  4. If either rectangle is smaller than 60 x 60 inches, flag an
     inadequate landing.

Placeholder node - not implemented for the capstone demo.
"""

from __future__ import annotations

import rclpy
from rclpy.node import Node


ADA_LANDING_THRESHOLD_IN = 60.0


class RampLandingNode(Node):
    def __init__(self) -> None:
        super().__init__("star_ramp_landing_node")
        self.get_logger().warn(
            "ramp_landing_node is ARCHITECTED only. Not implemented in the capstone."
        )


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = RampLandingNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
