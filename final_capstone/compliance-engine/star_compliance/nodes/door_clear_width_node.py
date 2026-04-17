"""Door clear width node - ARCHITECTED.

ADA 2010 Standards section 404.2.3: door openings shall provide a
clear width of 32 inches minimum.

Designed algorithm:
  1. Capture rectified stereo pairs from the Waveshare IMX219-83 via
     libcamera / picamera2.
  2. Compute disparity via OpenCV StereoSGBM; convert to depth using
     the calibration Q matrix.
  3. Detect door-frame vertical edges (Canny + RANSAC line fitting) at
     handle height (~36 inches from floor).
  4. Between a pair of parallel vertical frame lines, measure the
     horizontal clearance at handle height.
  5. If < 32 inches, flag a violation.

This check requires the stereo driver and calibration, neither of
which is implemented for the capstone demo. Placeholder.
"""

from __future__ import annotations

import rclpy
from rclpy.node import Node


ADA_DOOR_CLEAR_WIDTH_THRESHOLD_IN = 32.0


class DoorClearWidthNode(Node):
    def __init__(self) -> None:
        super().__init__("star_door_clear_width_node")
        self.get_logger().warn(
            "door_clear_width_node is ARCHITECTED only. Not implemented in the capstone."
        )


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = DoorClearWidthNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
