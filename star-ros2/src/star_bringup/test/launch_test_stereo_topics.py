"""
Launch test that boots stereo_camera.launch.py and confirms every
stereo topic publishes within a timeout.

Run on a Pi 5 with both IMX219-83 sensors connected:

    colcon test --packages-select star_bringup --event-handlers console_direct+
    launch_test install/star_bringup/share/star_bringup/test/launch_test_stereo_topics.py

Skips gracefully when the sensors are not present.
"""

import os
import time
import unittest

import launch
import launch_ros
import launch_testing.actions
import launch_testing.asserts
import pytest

import rclpy
from rclpy.node import Node


# Every stereo topic the pipeline is expected to publish.
EXPECTED_TOPICS = [
    ("/cam0/image_raw", "sensor_msgs/msg/Image"),
    ("/cam1/image_raw", "sensor_msgs/msg/Image"),
    ("/cam0/camera/image_rect_color", "sensor_msgs/msg/Image"),
    ("/cam1/camera/image_rect_color", "sensor_msgs/msg/Image"),
    ("/stereo/disparity", "stereo_msgs/msg/DisparityImage"),
    ("/stereo/points2", "sensor_msgs/msg/PointCloud2"),
]

WAIT_SECONDS = 30.0
HARDWARE_MARKER = "/dev/media2"


def _hardware_present() -> bool:
    return os.path.exists(HARDWARE_MARKER)


@pytest.mark.launch_test
def generate_test_description():
    if not _hardware_present():
        pytest.skip("IMX219-83 not detected (/dev/media2 missing)")

    stereo_launch_path = os.path.join(
        os.path.dirname(__file__),
        "..",
        "launch",
        "stereo_camera.launch.py",
    )
    stereo = launch.actions.IncludeLaunchDescription(
        launch.launch_description_sources.PythonLaunchDescriptionSource(
            stereo_launch_path
        ),
    )

    return launch.LaunchDescription([
        stereo,
        launch_testing.actions.ReadyToTest(),
    ])


class TestStereoTopicsPublishing(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = Node("stereo_launch_test_observer")

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    def test_every_stereo_topic_publishes_within_timeout(self):
        deadline = time.time() + WAIT_SECONDS
        missing = set(name for name, _ in EXPECTED_TOPICS)
        while time.time() < deadline and missing:
            rclpy.spin_once(self.node, timeout_sec=0.5)
            seen = dict(self.node.get_topic_names_and_types())
            for name, expected_type in list(missing):
                if name in seen and expected_type in seen[name]:
                    missing.discard(name)
        self.assertFalse(
            missing,
            f"Stereo topics never published: {sorted(missing)}",
        )
