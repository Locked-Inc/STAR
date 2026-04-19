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

When use_stereo_proc is true (default), stereo_image_proc runs alongside
the cameras to produce rectified images, a disparity map, and a 3D point
cloud.  When use_rtabmap is true, RTAB-Map runs in parallel with
slam_toolbox (publish_tf=false) to build a 3D map from stereo vision.

TF frames:
  base_link -> cam0_link -> cam0_optical_frame  (left sensor)
  base_link -> cam1_link -> cam1_optical_frame  (right sensor, +61 mm in Y)
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode

# ---------------------------------------------------------------------------
# Camera hardware identifiers.  Mapping verified by stereo calibration:
# physical-left sensor is on the i2c@80000 bus, physical-right on i2c@88000.
# ---------------------------------------------------------------------------
CAM0_ID = '/base/axi/pcie@120000/rp1/i2c@80000/imx219@10'
CAM1_ID = '/base/axi/pcie@120000/rp1/i2c@88000/imx219@10'

# ---------------------------------------------------------------------------
# Default capture resolution.  IMX219 native modes:
#   3280x2464 @ 15 fps   (full)
#   1640x1232 @ 30 fps   (2x2 binned)
#    640x480  @ 30 fps   (4x4 binned, optimal for stereo on Pi5)
#
# 640x480 gives ~5-10 Hz stereo matching vs ~1-2 Hz at 1640x1232.
# Calibration YAMLs in config/camera_info/ are scaled to match.
# ---------------------------------------------------------------------------
DEFAULT_WIDTH = '640'
DEFAULT_HEIGHT = '480'
DEFAULT_FPS = '15'

# ---------------------------------------------------------------------------
# libpisp ABI fix: system libpisp 1.2.1 must precede ROS libpisp 1.3.0.
# Without this the IPA proxy fails with an undefined symbol error.
# ---------------------------------------------------------------------------
SYSTEM_LIB_PATH = '/usr/lib/aarch64-linux-gnu'

# ---------------------------------------------------------------------------
# Stereo baseline geometry.  Physically measured at 61 mm (confirmed by
# calibration which computed ~60 mm).  The Waveshare "83" in IMX219-83
# refers to the field of view, not the baseline distance.
# CAM0 is the left sensor; CAM1 is offset +61 mm along the Y axis of
# base_link when the board faces forward.
# ---------------------------------------------------------------------------
STEREO_BASELINE_M = 0.061


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
    use_stereo_proc_arg = DeclareLaunchArgument(
        'use_stereo_proc',
        default_value='true',
        description='Run stereo_image_proc for rectification, disparity, and point cloud',
    )
    use_rtabmap_arg = DeclareLaunchArgument(
        'use_rtabmap',
        default_value='true',
        description='Run RTAB-Map 3D stereo mapping (publish_tf=false, alongside slam_toolbox)',
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
                'gscam_config': _gst_pipeline(
                    CAM0_ID, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS
                ),
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
                'gscam_config': _gst_pipeline(
                    CAM1_ID, DEFAULT_WIDTH, DEFAULT_HEIGHT, DEFAULT_FPS
                ),
                'camera_name': 'cam1',
                'frame_id': 'cam1_optical_frame',
                'image_encoding': 'rgb8',
                'camera_info_url': 'package://star_bringup/config/camera_info/cam1.yaml',
            }
        ],
    )

    # -----------------------------------------------------------------------
    # Stereo processing pipeline: rectify + disparity + point cloud.
    #
    # Runs in a multi-threaded component container for better throughput on
    # Pi5 (4 cores).  At 640x480 with block matching this produces ~5-10 Hz
    # disparity and point cloud.
    #
    # approximate_sync is required because the two IMX219 sensors are NOT
    # hardware-synced.  The namespace 'stereo' puts disparity and points2
    # under /stereo/*.
    # -----------------------------------------------------------------------
    stereo_proc = ComposableNodeContainer(
        condition=IfCondition(LaunchConfiguration('use_stereo_proc')),
        name='stereo_pipeline',
        namespace='',
        package='rclcpp_components',
        executable='component_container_mt',
        composable_node_descriptions=[
            ComposableNode(
                package='image_proc',
                plugin='image_proc::RectifyNode',
                name='rectify_left',
                namespace='cam0/camera',
                remappings=[
                    ('image', 'image_raw'),
                    ('image_rect', 'image_rect_color'),
                ],
            ),
            ComposableNode(
                package='image_proc',
                plugin='image_proc::RectifyNode',
                name='rectify_right',
                namespace='cam1/camera',
                remappings=[
                    ('image', 'image_raw'),
                    ('image_rect', 'image_rect_color'),
                ],
            ),
            ComposableNode(
                package='stereo_image_proc',
                plugin='stereo_image_proc::DisparityNode',
                namespace='stereo',
                parameters=[{
                    'approximate_sync': True,
                    'stereo_algorithm': 0,
                    'correlation_window_size': 15,
                    'disparity_range': 64,
                    'speckle_size': 100,
                    'speckle_range': 4,
                    'uniqueness_ratio': 15.0,
                }],
                remappings=[
                    ('left/image_rect', '/cam0/camera/image_rect_color'),
                    ('left/camera_info', '/cam0/camera/camera_info'),
                    ('right/image_rect', '/cam1/camera/image_rect_color'),
                    ('right/camera_info', '/cam1/camera/camera_info'),
                ],
            ),
            ComposableNode(
                package='stereo_image_proc',
                plugin='stereo_image_proc::PointCloudNode',
                namespace='stereo',
                parameters=[{
                    'approximate_sync': True,
                    'use_color': True,
                }],
                remappings=[
                    ('left/image_rect_color', '/cam0/camera/image_rect_color'),
                    ('left/camera_info', '/cam0/camera/camera_info'),
                    ('right/camera_info', '/cam1/camera/camera_info'),
                ],
            ),
        ],
        output='screen',
    )

    # -----------------------------------------------------------------------
    # RTAB-Map 3D stereo mapping.  Runs alongside slam_toolbox with
    # publish_tf=false so slam_toolbox owns the map->odom transform.
    # Builds an independent 3D map from stereo vision at ~2 Hz.
    # -----------------------------------------------------------------------
    pkg_star_bringup = get_package_share_directory('star_bringup')
    rtabmap_node = Node(
        condition=IfCondition(LaunchConfiguration('use_rtabmap')),
        package='rtabmap_slam',
        executable='rtabmap',
        name='rtabmap',
        output='screen',
        parameters=[
            os.path.join(pkg_star_bringup, 'config', 'rtabmap.yaml'),
        ],
        remappings=[
            ('left/image_rect', '/cam0/camera/image_raw'),
            ('left/camera_info', '/cam0/camera/camera_info'),
            ('right/image_rect', '/cam1/camera/image_raw'),
            ('right/camera_info', '/cam1/camera/camera_info'),
            ('odom', '/odometry/filtered'),
            ('scan', '/scan'),
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

    # Static transform: base_link -> cam1_link (right sensor, +61 mm in Y)
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
            use_stereo_proc_arg,
            use_rtabmap_arg,
            cam0_node,
            cam1_node,
            stereo_proc,
            rtabmap_node,
            cam0_tf,
            cam0_optical_tf,
            cam1_tf,
            cam1_optical_tf,
        ]
    )
