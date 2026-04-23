"""SLAM MVP launch: minimal stack for bring-up demo on 2026-04-22.

Brings up only what SLAM needs:
  - robot_state_publisher + static TFs (via static_transforms.launch.py)
  - star_simple_bridge (ASCII /dev/ttyACM0 bridge)
  - sllidar_node (RPLiDAR C1)
  - slam_toolbox
  - foxglove_bridge
  - static odom->base_link TF (EKF disabled for MVP simplicity)

Does NOT launch rtabmap, cameras, stereo, hailo_yolo, apriltag, aruco, nav2, or ekf.

Run:
    ros2 launch star_bringup slam_mvp.launch.py
    # In another terminal (or Foxglove):
    ros2 topic echo /odom/unfiltered
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    pkg_bringup = get_package_share_directory('star_bringup')

    # Reuse existing static_transforms (brings robot_state_publisher + base->laser)
    static_tf = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_bringup, 'launch', 'static_transforms.launch.py')),
    )

    # Static odom->base_link TF: placeholder while EKF is disabled.
    # Robot is stationary on a box; the lidar scan renders relative to origin.
    static_odom_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_odom_tf',
        arguments=['0', '0', '0', '0', '0', '0', 'odom', 'base_link'],
        output='screen',
    )

    simple_bridge = Node(
        package='star_simple_bridge',
        executable='simple_bridge_node',
        name='star_simple_bridge',
        output='screen',
        respawn=True,
        respawn_delay=2.0,
    )

    rplidar = Node(
        package='sllidar_ros2',
        executable='sllidar_node',
        name='sllidar_node',
        parameters=[{
            'serial_port': LaunchConfiguration('lidar_port', default='/dev/ttyUSB0'),
            'serial_baudrate': 460800,
            'frame_id': 'laser_frame',
            'inverted': False,
            'angle_compensate': True,
            'scan_mode': 'Standard',
        }],
        output='screen',
    )

    slam = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        parameters=[{
            'odom_frame': 'odom',
            'base_frame': 'base_link',
            'map_frame': 'map',
            'scan_topic': '/scan',
            'use_scan_matching': True,
            'use_scan_barycenter': True,
            'mode': 'mapping',
            'resolution': 0.05,
            'max_laser_range': 12.0,
            'minimum_time_interval': 0.5,
        }],
        output='screen',
    )

    foxglove = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge',
        parameters=[{'port': 8765, 'address': '0.0.0.0'}],
        output='screen',
    )

    return LaunchDescription([
        static_tf,
        static_odom_tf,
        simple_bridge,
        rplidar,
        slam,
        foxglove,
    ])
