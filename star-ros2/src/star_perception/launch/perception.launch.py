# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""
Launch the Hailo-8L YOLO + 3D fusion perception pipeline.

Brings up two nodes:
  - star_hailo_yolo_node       2D YOLOv8 inference on Hailo-8L
  - star_detection_3d_fusion_node    fuses with /stereo/disparity to 3D

This launch file is included by star_bringup/launch/slam.launch.py
behind a `use_perception` flag (default true). The stereo pipeline
must already be running (stereo_camera.launch.py) for the disparity
topic to exist.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _default_hef_path() -> str:
    share = get_package_share_directory("star_perception")
    return os.path.join(share, "models", "yolov8s_h8l.hef")


def generate_launch_description() -> LaunchDescription:
    score_arg = DeclareLaunchArgument(
        "score_threshold",
        default_value="0.35",
        description="Drop YOLO detections below this confidence.",
    )
    target_fps_arg = DeclareLaunchArgument(
        "target_fps",
        default_value="15.0",
        description=(
            "Soft FPS cap on the YOLO node. Frames arriving faster are "
            "dropped at the subscriber. 15 matches the cam0 capture rate."
        ),
    )
    overlay_arg = DeclareLaunchArgument(
        "publish_overlay",
        default_value="false",
        description="Publish /perception/image_overlay (debug).",
    )
    hef_arg = DeclareLaunchArgument(
        "hef_path",
        default_value=_default_hef_path(),
        description="Path to the YOLOv8 .hef compiled for Hailo-8L.",
    )
    sync_slop_arg = DeclareLaunchArgument(
        "sync_slop_sec",
        default_value="0.10",
        description=(
            "ApproximateTimeSynchronizer slop between detections and "
            "disparity image."
        ),
    )

    yolo_node = Node(
        package="star_perception",
        executable="hailo_yolo_node",
        name="star_hailo_yolo_node",
        output="screen",
        parameters=[{
            "hef_path": LaunchConfiguration("hef_path"),
            "score_threshold": LaunchConfiguration("score_threshold"),
            "target_fps": LaunchConfiguration("target_fps"),
            "publish_overlay": LaunchConfiguration("publish_overlay"),
        }],
    )

    fusion_node = Node(
        package="star_perception",
        executable="detection_3d_fusion_node",
        name="star_detection_3d_fusion_node",
        output="screen",
        parameters=[{
            "sync_slop_sec": LaunchConfiguration("sync_slop_sec"),
        }],
    )

    return LaunchDescription([
        score_arg,
        target_fps_arg,
        overlay_arg,
        hef_arg,
        sync_slop_arg,
        yolo_node,
        fusion_node,
    ])
