# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import FindPackageShare, LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node


def generate_launch_description():
    pkg_star_bringup = get_package_share_directory('star_bringup')
    pkg_slam_toolbox = get_package_share_directory('slam_toolbox')

    serial_port_arg = DeclareLaunchArgument(
        'serial_port', default_value='/dev/ttyUSB0',
        description='Serial port for RPLiDAR C1'
    )

    use_nav2_arg = DeclareLaunchArgument(
        'use_nav2', default_value='true',
        description='Launch Nav2 navigation stack alongside SLAM'
    )

    # RPLiDAR C1 requires sllidar_ros2 (Slamtec's newer driver with SDK 2.x).
    # rplidar_ros 2.1.0 uses SDK 1.12.0 which returns 0x80008000/0x80008002 for the C1's
    # DTOF scan protocol. sllidar_ros2 uses the updated SDK that supports C1 natively.
    # Official C1 params: channel_type='serial', baudrate=460800, scan_mode='Standard'.
    rplidar = Node(
        name='sllidar_node',
        package='sllidar_ros2',
        executable='sllidar_node',
        output='screen',
        parameters=[{
            'channel_type': 'serial',
            'serial_port': LaunchConfiguration('serial_port'),
            'serial_baudrate': 460800,
            'frame_id': 'laser_frame',
            'inverted': False,
            'angle_compensate': True,
            'scan_mode': 'Standard',
        }],
        respawn=True,
        respawn_delay=2.0,
    )

    static_tf = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_star_bringup, 'launch', 'static_transforms.launch.py')
        ),
    )

    # EKF node: consumes /odom/unfiltered + /imu/data, publishes odom→base_link TF
    # and /odometry/filtered. Must start before SLAM so odom→base_link TF is available.
    ekf = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[os.path.join(pkg_star_bringup, 'config', 'ekf.yaml')],
        respawn=True,
        respawn_delay=2.0,
    )

    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_slam_toolbox, 'launch', 'online_async_launch.py')
        ),
        launch_arguments={
            'slam_params_file': os.path.join(pkg_star_bringup, 'config', 'slam_toolbox.yaml'),
            'use_sim_time': 'false',
        }.items(),
    )

    # Nav2 navigation stack (navigation_launch.py — no map server, uses /map from SLAM).
    # Requires: sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup
    # PathJoinSubstitution is resolved lazily at launch time; IfCondition(false) prevents
    # execution so FindPackageShare never runs when use_nav2:=false and nav2_bringup is absent.
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([
                FindPackageShare('nav2_bringup'), 'launch', 'navigation_launch.py'
            ])
        ),
        launch_arguments={
            'use_sim_time': 'false',
            'params_file': os.path.join(pkg_star_bringup, 'config', 'nav2_params.yaml'),
            'map_subscribe_transient_local': 'true',
        }.items(),
        condition=IfCondition(LaunchConfiguration('use_nav2')),
    )

    return LaunchDescription([
        serial_port_arg,
        use_nav2_arg,
        static_tf,
        ekf,
        rplidar,
        slam,
        nav2,
    ])
