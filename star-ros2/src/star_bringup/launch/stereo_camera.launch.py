# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

"""
Launch file for the Waveshare IMX219-83 stereo camera on Raspberry Pi 5.

Both IMX219 sensors are driven via gscam (GStreamer + libcamerasrc).
The libpisp 1.2.1 / 1.3.0 ABI conflict requires the system library to win
the LD_LIBRARY_PATH search; this file prepends /usr/lib/aarch64-linux-gnu
in each node's env so the Pi IPA module loads correctly regardless of
whether the ROS workspace has been sourced.

Topics published:
  /cam0/image_raw        sensor_msgs/Image   (CAM0 -- left)
  /cam0/camera_info      sensor_msgs/CameraInfo
  /cam1/image_raw        sensor_msgs/Image   (CAM1 -- right)
  /cam1/camera_info      sensor_msgs/CameraInfo

TF frames:
  base_link -> cam0_link -> cam0_optical_frame  (left sensor)
  base_link -> cam1_link -> cam1_optical_frame  (right sensor, +83 mm in Y)
"""

import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch_ros.actions import Node

# ---------------------------------------------------------------------------
# Camera hardware identifiers.  Mapping verified by stereo calibration:
# physical-left sensor is on the i2c@80000 bus, physical-right on i2c@88000.
# ---------------------------------------------------------------------------
CAM0_ID = '/base/axi/pcie@120000/rp1/i2c@80000/imx219@10'
CAM1_ID = '/base/axi/pcie@120000/rp1/i2c@88000/imx219@10'

# ---------------------------------------------------------------------------
# Default capture resolution.  IMX219 native modes:
#   3280x2464 @ 15 fps   (full)
#   1640x1232 @ 30 fps   (2x2 binned, recommended for stereo)
#    640x480  @ 30 fps   (4x4 binned, lowest CPU)
# ---------------------------------------------------------------------------
DEFAULT_WIDTH = '1640'
DEFAULT_HEIGHT = '1232'
DEFAULT_FPS = '15'

# ---------------------------------------------------------------------------
# libpisp ABI fix: system libpisp 1.2.1 must precede ROS libpisp 1.3.0.
# Without this the IPA proxy fails with an undefined symbol error.
# ---------------------------------------------------------------------------
SYSTEM_LIB_PATH = '/usr/lib/aarch64-linux-gnu'

# ---------------------------------------------------------------------------
# Stereo baseline geometry (Waveshare IMX219-83: 83 mm horizontal baseline).
# CAM0 is the left sensor; CAM1 is offset +83 mm along the Y axis of
# base_link when the board faces forward.
# ---------------------------------------------------------------------------
STEREO_BASELINE_M = 0.083


def _gst_pipeline(camera_id: str, width: str, height: str, fps: str) -> str:
    """
    Build the GStreamer pipeline string for one IMX219 sensor.

    The pipeline lets libcamera's PiSP ISP debayer in hardware and output
    RGB directly.  The previous software bayer2rgb approach produced broken
    images (horizontal line artifacts) because it cannot handle the 16-bit
    Bayer output from libcamerasrc on Pi5.
    """
    return (
        f'libcamerasrc camera-name={camera_id} '
        f'! video/x-raw,format=RGB,width={width},height={height},'
        f'framerate={fps}/1 '
        '! videoconvert '
        '! video/x-raw,format=RGB'
    )


def _cam_env() -> dict:
    """Environment overrides applied to every gscam node."""
    env = dict(os.environ)
    # Prepend system lib directory so libpisp 1.2.1 is found before ROS 1.3.0
    ld_path = env.get('LD_LIBRARY_PATH', '')
    env['LD_LIBRARY_PATH'] = (
        f'{SYSTEM_LIB_PATH}:{ld_path}' if ld_path else SYSTEM_LIB_PATH
    )
    return env


def generate_launch_description() -> LaunchDescription:
    width_arg = DeclareLaunchArgument(
        'width',
        default_value=DEFAULT_WIDTH,
        description='Capture width in pixels (must match an IMX219 native mode)',
    )
    height_arg = DeclareLaunchArgument(
        'height',
        default_value=DEFAULT_HEIGHT,
        description='Capture height in pixels (must match an IMX219 native mode)',
    )
    fps_arg = DeclareLaunchArgument(
        'fps',
        default_value=DEFAULT_FPS,
        description='Capture framerate (integer, frames per second)',
    )

    env = _cam_env()

    cam0_node = Node(
        package='gscam',
        executable='gscam_node',
        name='cam0',
        namespace='cam0',
        output='screen',
        env=env,
        parameters=[
            {
                'gscam_config': _gst_pipeline(CAM0_ID, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS),
                'camera_name': 'cam0',
                'frame_id': 'cam0_optical_frame',
                'image_encoding': 'rgb8',
                'camera_info_url': 'package://star_bringup/config/camera_info/cam0.yaml',
            }
        ],
    )

    cam1_node = Node(
        package='gscam',
        executable='gscam_node',
        name='cam1',
        namespace='cam1',
        output='screen',
        env=env,
        parameters=[
            {
                'gscam_config': _gst_pipeline(CAM1_ID, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS),
                'camera_name': 'cam1',
                'frame_id': 'cam1_optical_frame',
                'image_encoding': 'rgb8',
                'camera_info_url': 'package://star_bringup/config/camera_info/cam1.yaml',
            }
        ],
    )

    # Static transform: base_link -> cam0_link (left sensor, at origin)
    cam0_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='cam0_tf',
        arguments=[
            '--x', '0.0',
            '--y', '0.0',
            '--z', '0.0',
            '--roll', '0.0',
            '--pitch', '0.0',
            '--yaw', '0.0',
            '--frame-id', 'base_link',
            '--child-frame-id', 'cam0_link',
        ],
    )

    # Optical frame: cam0_link -> cam0_optical_frame
    # ROS convention: optical frame has Z forward, X right, Y down
    cam0_optical_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='cam0_optical_tf',
        arguments=[
            '--x', '0.0',
            '--y', '0.0',
            '--z', '0.0',
            '--roll', '-1.5708',
            '--pitch', '0.0',
            '--yaw', '-1.5708',
            '--frame-id', 'cam0_link',
            '--child-frame-id', 'cam0_optical_frame',
        ],
    )

    # Static transform: base_link -> cam1_link (right sensor, +83 mm in Y)
    cam1_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='cam1_tf',
        arguments=[
            '--x', '0.0',
            '--y', str(STEREO_BASELINE_M),
            '--z', '0.0',
            '--roll', '0.0',
            '--pitch', '0.0',
            '--yaw', '0.0',
            '--frame-id', 'base_link',
            '--child-frame-id', 'cam1_link',
        ],
    )

    # Optical frame: cam1_link -> cam1_optical_frame
    cam1_optical_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='cam1_optical_tf',
        arguments=[
            '--x', '0.0',
            '--y', '0.0',
            '--z', '0.0',
            '--roll', '-1.5708',
            '--pitch', '0.0',
            '--yaw', '-1.5708',
            '--frame-id', 'cam1_link',
            '--child-frame-id', 'cam1_optical_frame',
        ],
    )

    return LaunchDescription(
        [
            width_arg,
            height_arg,
            fps_arg,
            cam0_node,
            cam1_node,
            cam0_tf,
            cam0_optical_tf,
            cam1_tf,
            cam1_optical_tf,
        ]
    )
