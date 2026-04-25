"""Accessible path width node - STRETCH.

ADA 2010 Standards section 403.5: accessible routes must provide a
continuous clear width of at least 36 inches except at doorways and
turning spaces.

Algorithm (to implement during the 7-day window):

1. Subscribe to /map (nav_msgs/OccupancyGrid) published by
   slam_toolbox async.
2. Compute a medial-axis transform on the free-space region of the
   grid (skimage.morphology.medial_axis, or
   skimage.morphology.skeletonize + distance_transform_edt).
3. At each centerline pixel, the local clearance is twice the
   distance-transform value.
4. Minimum local clearance along each accessible segment is the
   reported path width.
5. Flag any segment whose minimum width is below 36 inches (0.9144 m).
"""

from __future__ import annotations

import rclpy
from rclpy.node import Node


ADA_PATH_WIDTH_THRESHOLD_IN = 36.0
ADA_PATH_WIDTH_THRESHOLD_M = 0.9144


class PathWidthNode(Node):
    def __init__(self) -> None:
        super().__init__("star_path_width_node")
        self.get_logger().warn(
            "path_width_node is a STRETCH stub. "
            "Full implementation planned for the 7-day capstone window."
        )
        # TODO: subscribe to /map
        # TODO: medial-axis transform on occupancy grid
        # TODO: compute min local clearance along each accessible
        #       segment
        # TODO: write flagged sub-36-inch segments to
        #       validation_log.csv and the PDF report


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = PathWidthNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
