"""SLAM MVP launch: minimal stack for bring-up demo on 2026-04-22.

Brings up only what SLAM needs:
  - robot_state_publisher + static TFs (via static_transforms.launch.py)
  - star_simple_bridge (ASCII /dev/ttyACM0 bridge; -p publish_odom_tf:=false)
  - ekf_filter_node (robot_localization; fuses wheel vx + BNO055 yaw;
    owns the odom->base_link TF)
  - sllidar_node (RPLiDAR C1)
  - slam_toolbox
  - foxglove_bridge
  - [optional] Nav2 navigation stack under use_nav2:=true (default false).
    For autonomous-navigation demo with a pre-built map.

Does NOT launch rtabmap, cameras, stereo, hailo_yolo, apriltag, aruco.

Run:
    ros2 launch star_bringup slam_mvp.launch.py                    # SLAM only
    ros2 launch star_bringup slam_mvp.launch.py use_nav2:=true     # + Nav2
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    GroupAction,
    IncludeLaunchDescription,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node, SetRemap
from launch_ros.substitutions import FindPackageShare


def generate_launch_description() -> LaunchDescription:
    pkg_bringup = get_package_share_directory('star_bringup')
    pkg_slam_toolbox = get_package_share_directory('slam_toolbox')

    use_nav2_arg = DeclareLaunchArgument(
        'use_nav2', default_value='false',
        description='If true, include Nav2 navigation stack with our tuned params.'
    )

    # Reuse existing static_transforms (brings robot_state_publisher + base->laser)
    static_tf = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_bringup, 'launch', 'static_transforms.launch.py')),
    )

    # NOTE: odom->base_link TF is owned by ekf_filter_node below (publish_tf:
    # true in ekf.yaml). We tell simple_bridge to not broadcast it so the two
    # don't race on the same TF edge.

    simple_bridge = Node(
        package='star_simple_bridge',
        executable='simple_bridge_node',
        name='star_simple_bridge',
        parameters=[{'publish_odom_tf': False}],
        output='screen',
        respawn=True,
        respawn_delay=2.0,
    )

    # robot_localization EKF -- fuses wheel vx (odom0) + BNO055 yaw/gyro_z
    # (imu0) and publishes /odometry/filtered at 30 Hz + owns odom->base_link.
    # The YAML's top-level key is ekf_filter_node so we must match on name.
    ekf = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        parameters=[os.path.join(pkg_bringup, 'config', 'ekf.yaml')],
        output='screen',
        respawn=True,
        respawn_delay=2.0,
    )

    # star_supervisor arbitrates between manual teleop (/cmd_vel) and
    # Nav2 (/nav2/cmd_vel), emitting a single /cmd_vel_out that the
    # bridge consumes. Without this node the bridge sees nothing.
    supervisor = Node(
        package='star_supervisor',
        executable='supervisor_node',
        name='star_supervisor',
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

    # Use upstream slam_toolbox's online_async_launch.py, which already wires
    # autostart=true via configure->activate event handlers internally. An
    # inline `LifecycleNode(autostart=True)` here drops its initial Configure
    # event under the heavy concurrent-launch load of nav2 (launch_ros's
    # shared rclpy node gets starved during DDS discovery), leaving slam in
    # `unconfigured` -- which cascades into Nav2's global_costmap timing out
    # on the missing `map` frame and lifecycle_manager_navigation aborting.
    # The IncludeLaunchDescription form is what slam.launch.py uses and is
    # known to work alongside nav2.
    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_slam_toolbox, 'launch', 'online_async_launch.py')
        ),
        launch_arguments={
            'slam_params_file': os.path.join(pkg_bringup, 'config', 'slam_toolbox.yaml'),
            'use_sim_time': 'false',
        }.items(),
    )

    foxglove = Node(
        package='foxglove_bridge',
        executable='foxglove_bridge',
        name='foxglove_bridge',
        parameters=[{'port': 8765, 'address': '0.0.0.0'}],
        output='screen',
    )

    # Nav2 navigation stack: reuses the tuned nav2_params.yaml from the
    # full slam.launch.py but without cameras/rtabmap/stereo perception.
    # Uses the /map published live by slam_toolbox above.
    #
    # Remaps below route the stack into the star_supervisor mux:
    #   - bt_navigator subscribes to /goal_pose -> /nav2/goal_pose, so a
    #     Lichtblick "2D Goal Pose" click lands on the supervisor, which
    #     forwards to /nav2/goal_pose only when autonomy is armed.
    #   - collision_monitor already writes to /nav2/cmd_vel via
    #     nav2_params.yaml (cmd_vel_out_topic), so no launch-level remap
    #     is needed for the command path.
    # Defer Nav2 by 12s. Reason: when nav2's 11 lifecycle nodes spawn at the
    # same instant as slam_toolbox, launch_ros's shared rclpy node loses the
    # autostart Configure event for slam_toolbox to DDS-discovery starvation
    # and slam stays in `unconfigured`. Without slam, no `map` TF, and Nav2's
    # planner_server's global_costmap times out activating. Letting slam
    # autostart finish first removes the race -- by the time nav2's
    # lifecycle_manager activates planner_server, the map TF is already
    # being broadcast.
    nav2 = TimerAction(
        period=12.0,
        actions=[
            GroupAction(
                [
                    SetRemap(src='/goal_pose', dst='/nav2/goal_pose'),
                    IncludeLaunchDescription(
                        PythonLaunchDescriptionSource(
                            PathJoinSubstitution([
                                FindPackageShare('nav2_bringup'),
                                'launch',
                                'navigation_launch.py',
                            ])
                        ),
                        launch_arguments={
                            'use_sim_time': 'false',
                            'params_file': os.path.join(
                                pkg_bringup, 'config', 'nav2_params.yaml'
                            ),
                            'map_subscribe_transient_local': 'true',
                        }.items(),
                    ),
                ],
            ),
        ],
        condition=IfCondition(LaunchConfiguration('use_nav2')),
    )

    return LaunchDescription([
        use_nav2_arg,
        static_tf,
        simple_bridge,
        ekf,
        supervisor,
        rplidar,
        slam,
        foxglove,
        nav2,
    ])
