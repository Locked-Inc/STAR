import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_star_bringup = get_package_share_directory('star_bringup')
    pkg_slam_toolbox = get_package_share_directory('slam_toolbox')

    serial_port_arg = DeclareLaunchArgument(
        'serial_port', default_value='/dev/ttyUSB0',
        description='Serial port for RPLiDAR C1'
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
    )

    static_tf = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_star_bringup, 'launch', 'static_transforms.launch.py')
        ),
    )

    # EKF node: consumes /odom/unfiltered, publishes odom→base_link TF and /odometry/filtered.
    # Must start before SLAM so the odom→base_link TF link is available.
    ekf = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[os.path.join(pkg_star_bringup, 'config', 'ekf.yaml')],
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

    return LaunchDescription([
        serial_port_arg,
        static_tf,
        ekf,
        rplidar,
        slam,
    ])
