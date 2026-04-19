# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

# Slamtec RPLiDAR C1 DTOF protocol requires exactly 460800 baud.
# This is a hardware-fixed parameter, not user-tunable.
RPLIDAR_C1_BAUDRATE = 460800

# static_transform_publisher uses named flags (--x, --y, --z, --yaw, --pitch, --roll)
# rather than positional parameters, to be self-documenting and immune to argument
# order changes across ROS2 / tf2_ros releases:
#   --x   -> TRANSFORM_X    --y     -> TRANSFORM_Y    --z     -> TRANSFORM_Z
#   --yaw -> TRANSFORM_YAW  --pitch -> TRANSFORM_PITCH  --roll -> TRANSFORM_ROLL
#   --frame-id     -> parent frame (odom)
#   --child-frame-id -> child frame (base_link)
TRANSFORM_X = '0'
TRANSFORM_Y = '0'
TRANSFORM_Z = '0'
TRANSFORM_ROLL = '0'
TRANSFORM_PITCH = '0'
TRANSFORM_YAW = '0'

# TF frame ID constants shared between the static_transform_publisher arguments
# and any other nodes that reference these frames, so renaming a frame requires
# only a single change here.
FRAME_ODOM = 'odom'
FRAME_BASE_LINK = 'base_link'

# Default respawn delay (seconds) for nodes configured with respawn=True.
RESPAWN_DELAY_SEC = 2.0

# Drivetrain kinematics for the BBB skid-steer platform. Source: CAD
# measurement of the goBILDA Wasteland chassis (2026-04 build) and goBILDA
# motor datasheet. Mirrored in star_spi_bridge.launch.py and the BBB
# hardware_config.h to keep producers and consumers in sync; bump all
# three together if the chassis or motor changes.
WHEEL_BASE_M = 0.356            # track width: left-right wheel center-to-center
WHEEL_RADIUS_M = 0.072          # rolling radius: 144 mm wheel / 2
ENCODER_TICKS_PER_REV = 11599   # 341 PPR Hall encoder x 34.02:1 gearbox


def generate_launch_description():
    pkg_star_bringup = get_package_share_directory('star_bringup')
    pkg_slam_toolbox = get_package_share_directory('slam_toolbox')

    serial_port_arg = DeclareLaunchArgument(
        'serial_port', default_value='/dev/rplidar',
        description='Serial port for RPLiDAR C1 (stable udev symlink)'
    )

    use_nav2_arg = DeclareLaunchArgument(
        'use_nav2', default_value='true',
        description='Launch Nav2 navigation stack alongside SLAM'
    )

    use_ekf_arg = DeclareLaunchArgument(
        'use_ekf', default_value='true',
        description='Run EKF odometry fusion (disable in dev mode - use static odom TF instead)'
    )

    use_bbb_arg = DeclareLaunchArgument(
        'use_bbb', default_value='true',
        description='Enable BeagleBone Blue telemetry bridging (publishes /odom/unfiltered, '
                    '/imu/data, /joint_states from BBB via gateway and forwards /cmd_vel to BBB)'
    )

    use_foxglove_arg = DeclareLaunchArgument(
        'use_foxglove', default_value='false',
        description='Launch Foxglove Studio WebSocket bridge for browser-based visualization'
    )

    use_stereo_arg = DeclareLaunchArgument(
        'use_stereo', default_value='true',
        description='Launch the IMX219-83 stereo camera pipeline (gscam + '
                    'stereo_image_proc + RTAB-Map). Disable for LiDAR-only '
                    'operation or on systems without the camera connected.'
    )

    # RPLiDAR C1 requires sllidar_ros2 (Slamtec's newer driver with SDK 2.x).
    # rplidar_ros 2.1.0 uses SDK 1.12.0 which returns 0x80008000/0x80008002 for the C1's
    # DTOF scan protocol. sllidar_ros2 uses the updated SDK that supports C1 natively.
    # Official C1 params: serial, RPLIDAR_C1_BAUDRATE baud, scan_mode='Standard'.
    rplidar = Node(
        name='sllidar_node',
        package='sllidar_ros2',
        executable='sllidar_node',
        output='screen',
        parameters=[{
            'channel_type': 'serial',
            'serial_port': LaunchConfiguration('serial_port'),
            'serial_baudrate': RPLIDAR_C1_BAUDRATE,
            'frame_id': 'laser_frame',
            'inverted': False,
            'angle_compensate': True,
            'scan_mode': 'Standard',
        }],
        respawn=True,
        respawn_delay=RESPAWN_DELAY_SEC,
    )

    static_tf = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_star_bringup, 'launch', 'static_transforms.launch.py')
        ),
    )

    # EKF node: consumes /odom/unfiltered + /imu/data, publishes odom->base_link TF
    # and /odometry/filtered. Must start before SLAM so odom->base_link TF is available.
    ekf = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[os.path.join(pkg_star_bringup, 'config', 'ekf.yaml')],
        respawn=True,
        respawn_delay=RESPAWN_DELAY_SEC,
        condition=IfCondition(LaunchConfiguration('use_ekf')),
    )

    # Fallback odom->base_link TF when EKF is disabled.
    static_odom_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_odom_to_base_link',
        output='screen',
        arguments=[
            '--x',
            TRANSFORM_X,
            '--y',
            TRANSFORM_Y,
            '--z',
            TRANSFORM_Z,
            '--yaw',
            TRANSFORM_YAW,
            '--pitch',
            TRANSFORM_PITCH,
            '--roll',
            TRANSFORM_ROLL,
            '--frame-id',
            FRAME_ODOM,
            '--child-frame-id',
            FRAME_BASE_LINK,
        ],
        condition=UnlessCondition(LaunchConfiguration('use_ekf')),
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

    # Nav2 navigation stack (navigation_launch.py -- no map server, uses /map from SLAM).
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

    # Stereo camera pipeline: gscam cam0/cam1 + stereo_image_proc rectify /
    # disparity / point cloud + RTAB-Map 3D mapping. Gated by use_stereo so
    # the stack still runs LiDAR-only when the camera is disconnected.
    stereo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_star_bringup, 'launch', 'stereo_camera.launch.py')
        ),
        condition=IfCondition(LaunchConfiguration('use_stereo')),
    )

    # Foxglove Studio WebSocket bridge -- enables browser-based visualization
    # at app.foxglove.dev without X11. Bound to 0.0.0.0 for remote browser access.
    # Connect from laptop: ws://<PI5_IP>:8765
    # Install: sudo apt install ros-jazzy-foxglove-bridge
    foxglove_bridge = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge',
        output='log',
        parameters=[{
            'port': 8765,
            'address': '0.0.0.0',
            'send_buffer_limit': 50000000,
            'use_sim_time': False,
        }],
        respawn=True,
        respawn_delay=RESPAWN_DELAY_SEC,
        condition=IfCondition(LaunchConfiguration('use_foxglove')),
    )

    # Gateway bridge node -- bridges ROS2 to Go gateway via gRPC.
    # When use_bbb is true, also bridges BBB telemetry to ROS2 topics and
    # forwards /cmd_vel to BBB motors via gateway.
    gateway_bridge = Node(
        package='star_gateway_bridge',
        executable='star_gateway_bridge_main',
        name='star_gateway_bridge',
        output='screen',
        parameters=[{
            'gateway_address': 'localhost:50051',
            'telemetry_rate_hz': 10.0,
            'teleop_rate_hz': 50.0,
            'use_bbb_telemetry': LaunchConfiguration('use_bbb'),
            'wheel_base': WHEEL_BASE_M,
            'wheel_radius': WHEEL_RADIUS_M,
            'ticks_per_rev': ENCODER_TICKS_PER_REV,
        }],
        respawn=True,
        respawn_delay=RESPAWN_DELAY_SEC,
    )

    return LaunchDescription([
        serial_port_arg,
        use_nav2_arg,
        use_ekf_arg,
        use_bbb_arg,
        use_foxglove_arg,
        use_stereo_arg,
        static_tf,
        ekf,
        static_odom_tf,
        rplidar,
        foxglove_bridge,
        gateway_bridge,
        slam,
        nav2,
        stereo,
    ])
