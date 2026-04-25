"""Trip hazard node - STRETCH.

ADA 2010 Standards section 303: changes in level up to 0.25 inch may be
vertical; changes between 0.25 and 0.5 inch must be beveled 1:2; above
0.5 inch requires a ramp.

Algorithm (to implement during the 7-day window):

1. Subscribe to /scan. When robot is moving forward on a floor plane,
   accumulate returns along the ground.
2. For each angular bin, compute the vertical delta between the current
   scan and the previous accumulated median.
3. Cross-validate against BNO055 vertical-acceleration spikes during
   traversal - a real threshold produces both a LiDAR delta and an
   accel jolt at the same robot-x.
4. If the measured delta exceeds 0.25 inch and the jolt signature
   confirms it, flag a violation.
"""

from __future__ import annotations

import rclpy
from rclpy.node import Node


ADA_TRIP_HAZARD_THRESHOLD_IN = 0.25


class TripHazardNode(Node):
    def __init__(self) -> None:
        super().__init__("star_trip_hazard_node")
        self.get_logger().warn(
            "trip_hazard_node is a STRETCH stub. "
            "Full implementation planned for the 7-day capstone window."
        )
        # TODO: subscribe to /scan, /imu/data, /odom
        # TODO: detect vertical delta between adjacent scan bins on the
        #       floor plane
        # TODO: correlate against BNO055 z-acceleration spike on
        #       traversal
        # TODO: write flagged violations to validation_log.csv


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = TripHazardNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
