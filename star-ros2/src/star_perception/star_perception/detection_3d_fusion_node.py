"""
3D detection fusion node.

Fuses each `vision_msgs/Detection2DArray` from the Hailo YOLO node with
the rectified disparity image already published by
`stereo_image_proc::DisparityNode` to emit a 3D detection in
`cam0_optical_frame`.

Subscribes to:
  /perception/detections_2d   vision_msgs/Detection2DArray
  /stereo/disparity            stereo_msgs/DisparityImage
  /cam0/camera/camera_info    sensor_msgs/CameraInfo  (latched-style)

Publishes:
  /perception/detections_3d   vision_msgs/Detection3DArray
  /perception/markers          visualization_msgs/MarkerArray  (RViz)

Parameters:
  enabled                  bool   default true
  min_valid_samples        int    default 30
  max_depth_m              float  default 6.0
  min_depth_m              float  default 0.20
  disparity_border_pct     float  default 0.15
  sync_slop_sec            float  default 0.10
  marker_lifetime_sec      float  default 0.50

Time synchronisation uses message_filters.ApproximateTimeSynchronizer
between the 2D detections and the disparity image. The CameraInfo is
held in the latest-known cache (the existing pipeline only ever sends
one set of intrinsics, then they are static).
"""

from __future__ import annotations

from typing import Optional

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import (
    DurabilityPolicy,
    QoSProfile,
    ReliabilityPolicy,
)

from sensor_msgs.msg import CameraInfo
from stereo_msgs.msg import DisparityImage
from vision_msgs.msg import (
    BoundingBox3D,
    Detection2DArray,
    Detection3D,
    Detection3DArray,
    ObjectHypothesisWithPose,
)
from visualization_msgs.msg import Marker, MarkerArray
import message_filters

from star_perception.depth_fusion import (
    CameraIntrinsics,
    Detection3D as Det3D,
    DISPARITY_BORDER_PCT_DEFAULT,
    MAX_DEPTH_M_DEFAULT,
    MIN_DEPTH_M_DEFAULT,
    MIN_VALID_SAMPLES_DEFAULT,
    fuse_bbox_to_3d,
)


DETECTIONS_2D_TOPIC = "/perception/detections_2d"
DISPARITY_TOPIC = "/stereo/disparity"
CAMERA_INFO_TOPIC = "/cam0/camera/camera_info"
DETECTIONS_3D_TOPIC = "/perception/detections_3d"
MARKERS_TOPIC = "/perception/markers"
DEFAULT_FRAME = "cam0_optical_frame"


class Detection3dFusionNode(Node):

    def __init__(self) -> None:
        super().__init__("star_detection_3d_fusion_node")

        self.declare_parameter("enabled", True)
        self.declare_parameter(
            "min_valid_samples", MIN_VALID_SAMPLES_DEFAULT)
        self.declare_parameter("max_depth_m", MAX_DEPTH_M_DEFAULT)
        self.declare_parameter("min_depth_m", MIN_DEPTH_M_DEFAULT)
        self.declare_parameter(
            "disparity_border_pct", DISPARITY_BORDER_PCT_DEFAULT)
        self.declare_parameter("sync_slop_sec", 0.10)
        self.declare_parameter("marker_lifetime_sec", 0.50)

        self._intrinsics: Optional[CameraIntrinsics] = None
        self._cv_bridge = self._import_cv_bridge()

        qos_sensor = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=5,
        )
        # gscam publishes CameraInfo with VOLATILE durability (normal
        # streaming, not latched). A TRANSIENT_LOCAL subscriber cannot
        # match a VOLATILE publisher, so use VOLATILE here and cache the
        # first message in the callback instead of relying on latching.
        qos_camera_info = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=1,
        )

        self._sub_camera_info = self.create_subscription(
            CameraInfo, CAMERA_INFO_TOPIC, self._on_camera_info,
            qos_camera_info,
        )
        self._sub_dets = message_filters.Subscriber(
            self, Detection2DArray, DETECTIONS_2D_TOPIC, qos_profile=qos_sensor,
        )
        self._sub_disp = message_filters.Subscriber(
            self, DisparityImage, DISPARITY_TOPIC, qos_profile=qos_sensor,
        )
        slop = float(self.get_parameter("sync_slop_sec").value)
        self._sync = message_filters.ApproximateTimeSynchronizer(
            [self._sub_dets, self._sub_disp], queue_size=10, slop=slop,
        )
        self._sync.registerCallback(self._on_synced)

        self._pub_3d = self.create_publisher(
            Detection3DArray, DETECTIONS_3D_TOPIC, 10,
        )
        self._pub_markers = self.create_publisher(
            MarkerArray, MARKERS_TOPIC, 10,
        )

        self.get_logger().info(
            "star_detection_3d_fusion_node ready. "
            f"slop={slop:.2f}s, "
            f"depth=[{self.get_parameter('min_depth_m').value:.2f}, "
            f"{self.get_parameter('max_depth_m').value:.2f}] m."
        )

    # ------------------------------------------------------------------

    def _import_cv_bridge(self):
        try:
            from cv_bridge import CvBridge
            return CvBridge()
        except ImportError:  # pragma: no cover
            self.get_logger().warn(
                "cv_bridge not available; node will not function.")
            return None

    def _on_camera_info(self, msg: CameraInfo) -> None:
        # K is the original camera matrix; for rectified images we
        # actually want P[:3, :3] (the projection matrix's left 3x3),
        # but the existing stereo_image_proc pipeline rectifies, so
        # the published CameraInfo's K already reflects rectified
        # intrinsics on the rectified image_rect_color topic. The
        # camera_info topic carries identical intrinsics either way
        # for this rig (camera_info is sourced from the same YAML).
        try:
            self._intrinsics = CameraIntrinsics.from_k_matrix(
                np.asarray(msg.k))
        except Exception as exc:  # pragma: no cover
            self.get_logger().error(f"Bad CameraInfo K matrix: {exc}")

    # ------------------------------------------------------------------

    def _on_synced(self, dets: Detection2DArray,
                   disp_msg: DisparityImage) -> None:
        if not self.get_parameter("enabled").value:
            return
        if self._intrinsics is None:
            self.get_logger().warn(
                "No CameraInfo received yet; dropping detections.",
                throttle_duration_sec=5.0)
            return
        if self._cv_bridge is None:
            return

        try:
            disparity = self._cv_bridge.imgmsg_to_cv2(
                disp_msg.image, desired_encoding="passthrough")
        except Exception as exc:  # pragma: no cover
            self.get_logger().warn(f"cv_bridge disparity convert: {exc}")
            return
        if disparity.dtype != np.float32:
            disparity = disparity.astype(np.float32)

        focal_px = float(disp_msg.f)
        baseline_m = float(disp_msg.t)
        if not (focal_px > 0.0 and baseline_m > 0.0):
            self.get_logger().warn(
                f"Invalid disparity calibration: f={focal_px}, "
                f"T={baseline_m}", throttle_duration_sec=5.0)
            return

        out = Detection3DArray()
        out.header.stamp = dets.header.stamp
        out.header.frame_id = DEFAULT_FRAME

        markers = MarkerArray()
        marker_id = 0
        marker_lifetime = float(
            self.get_parameter("marker_lifetime_sec").value)

        min_samples = int(self.get_parameter("min_valid_samples").value)
        min_depth = float(self.get_parameter("min_depth_m").value)
        max_depth = float(self.get_parameter("max_depth_m").value)
        border = float(self.get_parameter("disparity_border_pct").value)

        for d2d in dets.detections:
            if not d2d.results:
                continue
            class_name = d2d.results[0].hypothesis.class_id
            score = float(d2d.results[0].hypothesis.score)

            cx = d2d.bbox.center.position.x
            cy = d2d.bbox.center.position.y
            sx = d2d.bbox.size_x
            sy = d2d.bbox.size_y
            x_min = cx - 0.5 * sx
            y_min = cy - 0.5 * sy
            x_max = cx + 0.5 * sx
            y_max = cy + 0.5 * sy

            det3d = fuse_bbox_to_3d(
                disparity, self._intrinsics,
                focal_px, baseline_m,
                class_id=0,  # YOLO node passes class via class_id string.
                class_name=class_name,
                score=score,
                x_min=x_min, y_min=y_min,
                x_max=x_max, y_max=y_max,
                border_pct=border,
                min_valid_samples=min_samples,
                min_depth_m=min_depth,
                max_depth_m=max_depth,
            )
            if det3d is None:
                continue

            out.detections.append(self._to_detection3d_msg(det3d, out.header))
            markers.markers.extend(self._to_markers(
                det3d, marker_id, out.header, marker_lifetime))
            marker_id += 2  # one cube + one text per detection.

        self._pub_3d.publish(out)
        self._pub_markers.publish(markers)

    # ------------------------------------------------------------------

    def _to_detection3d_msg(self, d: Det3D, header) -> Detection3D:
        msg = Detection3D()
        msg.header = header

        bbox = BoundingBox3D()
        bbox.center.position.x = d.cx_m
        bbox.center.position.y = d.cy_m
        bbox.center.position.z = d.cz_m
        bbox.center.orientation.w = 1.0
        bbox.size.x = d.size_x_m
        bbox.size.y = d.size_y_m
        bbox.size.z = d.size_z_m
        msg.bbox = bbox

        hyp = ObjectHypothesisWithPose()
        hyp.hypothesis.class_id = d.class_name
        hyp.hypothesis.score = d.score
        hyp.pose.pose.position.x = d.cx_m
        hyp.pose.pose.position.y = d.cy_m
        hyp.pose.pose.position.z = d.cz_m
        hyp.pose.pose.orientation.w = 1.0
        msg.results.append(hyp)
        return msg

    def _to_markers(self, d: Det3D, base_id: int, header,
                    lifetime_sec: float) -> list:
        from builtin_interfaces.msg import Duration as DurationMsg
        cube = Marker()
        cube.header = header
        cube.ns = "yolo3d"
        cube.id = base_id
        cube.type = Marker.CUBE
        cube.action = Marker.ADD
        cube.pose.position.x = d.cx_m
        cube.pose.position.y = d.cy_m
        cube.pose.position.z = d.cz_m
        cube.pose.orientation.w = 1.0
        cube.scale.x = max(0.01, d.size_x_m)
        cube.scale.y = max(0.01, d.size_y_m)
        cube.scale.z = max(0.01, d.size_z_m)
        cube.color.r = 0.0
        cube.color.g = 0.85
        cube.color.b = 0.0
        cube.color.a = 0.35
        cube.lifetime = DurationMsg(
            sec=int(lifetime_sec),
            nanosec=int((lifetime_sec - int(lifetime_sec)) * 1e9),
        )

        text = Marker()
        text.header = header
        text.ns = "yolo3d_label"
        text.id = base_id + 1
        text.type = Marker.TEXT_VIEW_FACING
        text.action = Marker.ADD
        text.pose.position.x = d.cx_m
        text.pose.position.y = d.cy_m - 0.5 * d.size_y_m - 0.05
        text.pose.position.z = d.cz_m
        text.pose.orientation.w = 1.0
        text.scale.z = 0.10
        text.color.r = 1.0
        text.color.g = 1.0
        text.color.b = 1.0
        text.color.a = 1.0
        text.text = f"{d.class_name} {d.score:.2f} ({d.cz_m:.2f}m)"
        text.lifetime = cube.lifetime
        return [cube, text]


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = Detection3dFusionNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
