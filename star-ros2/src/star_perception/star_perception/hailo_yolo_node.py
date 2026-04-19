"""
Hailo-8L YOLOv8 inference node.

Subscribes to:
  /cam0/camera/image_raw   sensor_msgs/Image (RGB8, unrectified)

Publishes:
  /perception/detections_2d   vision_msgs/Detection2DArray
  /perception/image_overlay   sensor_msgs/Image  (debug, only if
                              `publish_overlay` parameter is true)

Parameters:
  hef_path          Absolute path to the YOLOv8 .hef. Defaults to
                    `<share>/star_perception/models/yolov8s_h8l.hef`
                    -- override on the Pi 5 if installed elsewhere.
  score_threshold   Drop detections below this score. Default 0.35.
  target_fps        Soft cap. Frames arriving faster than this are
                    dropped at the subscriber so the NPU is not
                    saturated. Default 15.0 (matches cam0 capture).
  publish_overlay   Publish a debug image with bbox overlays. Default
                    false (saves CPU; enable for tuning sessions).
  class_filter      Optional list of class names to keep; empty list
                    means all 80 COCO classes. Default empty.

Runtime disable:
  Parameter `enabled` (default true) -- toggle live to short-circuit
  inference without tearing down the node.

The node reads `header.stamp` from the incoming Image and copies it to
the outgoing Detection2DArray, so the 3D fusion node downstream can
ApproximateTimeSync against `/stereo/disparity` correctly.
"""

from __future__ import annotations

import os
import time
from pathlib import Path

import rclpy
from rclpy.node import Node
from rclpy.qos import (
    DurabilityPolicy,
    QoSProfile,
    ReliabilityPolicy,
)
from rcl_interfaces.msg import ParameterDescriptor

from sensor_msgs.msg import Image
from vision_msgs.msg import (
    BoundingBox2D,
    Detection2D,
    Detection2DArray,
    ObjectHypothesisWithPose,
)

from star_perception.hailo_runner import (
    COCO_CLASSES,
    Detection,
    HailoYoloRunner,
    load_class_names,
)


DEFAULT_TARGET_FPS = 30.0
DEFAULT_SCORE_THRESHOLD = 0.35
# Subscribe to the unrectified stream so inference does not wait on the
# CPU rectify stage. YOLOv8 tolerates IMX219 wide-angle distortion; 3D
# fusion downstream still uses the rectified pair for geometry.
INPUT_TOPIC = "/cam0/camera/image_raw"
DETECTIONS_TOPIC = "/perception/detections_2d"
OVERLAY_TOPIC = "/perception/image_overlay"


def _default_hef_path() -> str:
    """Look for the .hef under the installed package share directory."""
    try:
        from ament_index_python.packages import get_package_share_directory
        share = get_package_share_directory("star_perception")
        candidate = Path(share) / "models" / "yolov8s_h8l.hef"
        if candidate.exists():
            return str(candidate)
    except Exception:  # pragma: no cover - ament missing in tests
        pass
    # Fallback to source-tree path (development workflow).
    return str(Path(__file__).resolve().parents[1] /
               "models" / "yolov8s_h8l.hef")


class HailoYoloNode(Node):

    def __init__(self) -> None:
        super().__init__("star_hailo_yolo_node")

        self.declare_parameter("enabled", True)
        self.declare_parameter("hef_path", _default_hef_path())
        self.declare_parameter("score_threshold", DEFAULT_SCORE_THRESHOLD)
        self.declare_parameter("target_fps", DEFAULT_TARGET_FPS)
        self.declare_parameter("publish_overlay", False)
        self.declare_parameter("class_filter", [])
        self.declare_parameter(
            "classes_yaml", "",
            descriptor=ParameterDescriptor(
                description=(
                    "Absolute path to a YAML file with a top-level "
                    "`names:` list giving class names in HEF index "
                    "order. Empty string -> use the built-in COCO 80 "
                    "list. Point this at "
                    "share/star_perception/models/oiv7_classes.yaml "
                    "when running a yolov8n-oiv7.hef."
                )))

        self._min_period_sec = 1.0 / max(
            float(self.get_parameter("target_fps").value), 0.1)
        self._last_infer_sec: float = 0.0
        self._first_inference_logged = False
        self._infer_count = 0

        self._runner: HailoYoloRunner | None = None
        self._cv_bridge = self._import_cv_bridge()

        self._class_filter = set(
            self.get_parameter("class_filter").value or []
        )

        # Match gscam's RELIABLE publisher. Depth 1 drops stale frames
        # at the queue. Tried BestEffort on the 30 fps path and the
        # kernel UDP recv buffer overflowed on 900 kB RGB frames so
        # inference never fired; RELIABLE is the honest choice until
        # we can move to an intra-process / shared-memory transport
        # (camera_ros + ComposableNode Hailo, or TAPPAS bypass).
        qos_image = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            depth=1,
        )
        self._sub = self.create_subscription(
            Image, INPUT_TOPIC, self._on_image, qos_image,
        )
        self._pub_dets = self.create_publisher(
            Detection2DArray, DETECTIONS_TOPIC, 10,
        )
        self._pub_overlay = self.create_publisher(
            Image, OVERLAY_TOPIC, 1,
        )

        self.get_logger().info(
            f"star_hailo_yolo_node ready. HEF="
            f"{self.get_parameter('hef_path').value}, "
            f"threshold={self.get_parameter('score_threshold').value}, "
            f"target_fps={self.get_parameter('target_fps').value}."
        )

    # ------------------------------------------------------------------

    def _import_cv_bridge(self):
        """Lazy cv_bridge import; returns the CvBridge instance or None."""
        try:
            from cv_bridge import CvBridge
            return CvBridge()
        except ImportError:  # pragma: no cover
            self.get_logger().warn(
                "cv_bridge not available; node will not function.")
            return None

    def _ensure_runner(self) -> HailoYoloRunner | None:
        if self._runner is not None:
            return self._runner
        hef = str(self.get_parameter("hef_path").value)
        threshold = float(self.get_parameter("score_threshold").value)
        classes_yaml = str(self.get_parameter("classes_yaml").value).strip()
        class_names = COCO_CLASSES
        if classes_yaml:
            try:
                class_names = load_class_names(classes_yaml)
                self.get_logger().info(
                    f"Loaded {len(class_names)} class names from "
                    f"{classes_yaml}")
            except Exception as exc:
                self.get_logger().warn(
                    f"Could not load classes_yaml={classes_yaml}: {exc}. "
                    "Falling back to COCO 80.")
        try:
            self._runner = HailoYoloRunner(
                hef_path=hef,
                score_threshold=threshold,
                class_names=class_names,
            )
        except Exception as exc:
            self.get_logger().error(
                f"Failed to initialise HailoYoloRunner: {exc}")
            self._runner = None
        return self._runner

    # ------------------------------------------------------------------

    def _on_image(self, msg: Image) -> None:
        if not self.get_parameter("enabled").value:
            return
        if self._cv_bridge is None:
            return

        now = self.get_clock().now().nanoseconds * 1e-9
        if now - self._last_infer_sec < self._min_period_sec:
            return  # Soft FPS cap.
        self._last_infer_sec = now

        runner = self._ensure_runner()
        if runner is None:
            return

        try:
            rgb = self._cv_bridge.imgmsg_to_cv2(msg, desired_encoding="rgb8")
        except Exception as exc:  # pragma: no cover
            self.get_logger().warn(f"cv_bridge convert failed: {exc}")
            return

        t0 = time.perf_counter()
        try:
            detections = runner.infer(rgb)
        except Exception as exc:
            self.get_logger().error(f"HEF inference failed: {exc}")
            return
        elapsed_ms = (time.perf_counter() - t0) * 1000.0

        if self._class_filter:
            detections = [d for d in detections
                          if d.class_name in self._class_filter]

        self._publish_detections(detections, msg)
        if bool(self.get_parameter("publish_overlay").value):
            self._publish_overlay(rgb, detections, msg)

        self._infer_count += 1
        if not self._first_inference_logged:
            self.get_logger().info(
                f"First inference: {elapsed_ms:.1f} ms, "
                f"{len(detections)} detections.")
            self._first_inference_logged = True
        elif self._infer_count % 30 == 0:
            self.get_logger().debug(
                f"Inference {self._infer_count}: {elapsed_ms:.1f} ms, "
                f"{len(detections)} detections.")

    # ------------------------------------------------------------------

    def _publish_detections(self,
                            detections: list[Detection],
                            source: Image) -> None:
        arr = Detection2DArray()
        arr.header.stamp = source.header.stamp
        arr.header.frame_id = source.header.frame_id or "cam0_optical_frame"

        for det in detections:
            d2d = Detection2D()
            d2d.header = arr.header

            bbox = BoundingBox2D()
            bbox.center.position.x = det.cx
            bbox.center.position.y = det.cy
            bbox.center.theta = 0.0
            bbox.size_x = det.width
            bbox.size_y = det.height
            d2d.bbox = bbox

            hyp = ObjectHypothesisWithPose()
            hyp.hypothesis.class_id = det.class_name
            hyp.hypothesis.score = float(det.score)
            d2d.results.append(hyp)

            arr.detections.append(d2d)

        self._pub_dets.publish(arr)

    def _publish_overlay(self,
                         rgb,
                         detections: list[Detection],
                         source: Image) -> None:
        try:
            import cv2
        except ImportError:  # pragma: no cover
            return
        overlay = rgb.copy()
        for d in detections:
            p1 = (int(d.x_min), int(d.y_min))
            p2 = (int(d.x_max), int(d.y_max))
            cv2.rectangle(overlay, p1, p2, (0, 255, 0), 2)
            label = f"{d.class_name} {d.score:.2f}"
            cv2.putText(overlay, label, (p1[0], max(15, p1[1] - 4)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)
        try:
            out = self._cv_bridge.cv2_to_imgmsg(overlay, encoding="rgb8")
            out.header = source.header
            self._pub_overlay.publish(out)
        except Exception as exc:  # pragma: no cover
            self.get_logger().warn(f"overlay publish failed: {exc}")


def main(args=None):  # pragma: no cover
    rclpy.init(args=args)
    node = HailoYoloNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":  # pragma: no cover
    main()
