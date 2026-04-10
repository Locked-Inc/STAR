# Copyright 2026 Locked Inc.
#
# Use of this source code is governed by an MIT-style
# license that can be found in the LICENSE file or at
# https://opensource.org/licenses/MIT.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import (
    Command,
    LaunchConfiguration,
    PathJoinSubstitution,
)
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    pkg_star_simulation = get_package_share_directory('star_simulation')
    pkg_star_bringup = get_package_share_directory('star_bringup')
    pkg_slam_toolbox = get_package_share_directory('slam_toolbox')

    # ------------------------------------------------------------------ #
    # Launch arguments                                                     #
    # ------------------------------------------------------------------ #
    world_arg = DeclareLaunchArgument(
        'world',
        default_value=os.path.join(
            pkg_star_simulation, 'worlds', 'indoor_slam_test.sdf'
        ),
        description='Path to Gazebo SDF world file',
    )

    use_rviz_arg = DeclareLaunchArgument(
        'use_rviz', default_value='true',
        description='Launch RViz for visualization',
    )

    headless_arg = DeclareLaunchArgument(
        'headless', default_value='false',
        description='Run Gazebo without GUI (for CI/headless testing)',
    )

    use_nav2_arg = DeclareLaunchArgument(
        'use_nav2', default_value='true',
        description='Launch Nav2 navigation stack',
    )

    use_ekf_arg = DeclareLaunchArgument(
        'use_ekf', default_value='true',
        description='Run EKF odometry fusion',
    )

    # ------------------------------------------------------------------ #
    # Gazebo resource path (so Gazebo finds our models/worlds)             #
    # ------------------------------------------------------------------ #
    # GZ_SIM_RESOURCE_PATH must point to the parent of star_simulation/
    # so Gazebo resolves model://star_simulation/meshes/... URIs correctly.
    gz_resource_path = SetEnvironmentVariable(
        name='GZ_SIM_RESOURCE_PATH',
        value=os.path.dirname(pkg_star_simulation),
    )

    # Force software rendering via llvmpipe when no hardware GPU is available.
    # Pi5's VideoCore VII only supports OpenGL 3.1 (OGRE2 needs 3.3).
    # Xvfb + llvmpipe provides OpenGL 4.5 in software.
    gz_display = SetEnvironmentVariable(
        name='DISPLAY',
        value=os.environ.get('DISPLAY', ':99'),
    )

    # ------------------------------------------------------------------ #
    # Process URDF via xacro                                               #
    # ------------------------------------------------------------------ #
    robot_description = ParameterValue(
        Command([
            'xacro ',
            os.path.join(pkg_star_simulation, 'urdf', 'star_sim.urdf.xacro'),
        ]),
        value_type=str,
    )

    # ------------------------------------------------------------------ #
    # Robot state publisher                                                #
    # ------------------------------------------------------------------ #
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': robot_description,
            'use_sim_time': True,
        }],
    )

    # ------------------------------------------------------------------ #
    # Gazebo Harmonic                                                      #
    # ------------------------------------------------------------------ #
    # Start Gazebo with GUI (default).
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([
                FindPackageShare('ros_gz_sim'), 'launch', 'gz_sim.launch.py',
            ])
        ),
        launch_arguments={
            'gz_args': [
                LaunchConfiguration('world'),
                ' -r',
            ],
            'on_exit_shutdown': 'true',
        }.items(),
        condition=UnlessCondition(LaunchConfiguration('headless')),
    )

    # Start Gazebo server only (no GUI) for CI/headless testing.
    # Sensors still render via Xvfb + llvmpipe (set DISPLAY above).
    gazebo_headless = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([
                FindPackageShare('ros_gz_sim'), 'launch', 'gz_sim.launch.py',
            ])
        ),
        launch_arguments={
            'gz_args': [
                LaunchConfiguration('world'),
                ' -r --headless-rendering',
            ],
            'on_exit_shutdown': 'true',
        }.items(),
        condition=IfCondition(LaunchConfiguration('headless')),
    )

    # Spawn the robot model from robot_description into Gazebo.
    spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-name', 'star',
            '-topic', 'robot_description',
            '-x', '0.0',
            '-y', '0.0',
            '-z', '0.05',
        ],
        output='screen',
        parameters=[{'use_sim_time': True}],
    )

    # ------------------------------------------------------------------ #
    # ros_gz_bridge: Gazebo <-> ROS2 topic bridge                          #
    # ------------------------------------------------------------------ #
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        name='ros_gz_bridge',
        output='screen',
        parameters=[{
            'config_file': os.path.join(
                pkg_star_simulation, 'config', 'bridge.yaml'
            ),
            'use_sim_time': True,
        }],
    )

    # ------------------------------------------------------------------ #
    # EKF sensor fusion (reuses star_bringup config)                       #
    # ------------------------------------------------------------------ #
    ekf = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[
            os.path.join(pkg_star_bringup, 'config', 'ekf.yaml'),
            {'use_sim_time': True},
        ],
        condition=IfCondition(LaunchConfiguration('use_ekf')),
    )

    # Fallback static odom->base_link TF when EKF is disabled.
    static_odom_tf = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_odom_to_base_link',
        output='screen',
        arguments=[
            '--x', '0', '--y', '0', '--z', '0',
            '--yaw', '0', '--pitch', '0', '--roll', '0',
            '--frame-id', 'odom',
            '--child-frame-id', 'base_link',
        ],
        parameters=[{'use_sim_time': True}],
        condition=UnlessCondition(LaunchConfiguration('use_ekf')),
    )

    # ------------------------------------------------------------------ #
    # SLAM toolbox (reuses star_bringup config)                            #
    # ------------------------------------------------------------------ #
    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_slam_toolbox, 'launch', 'online_async_launch.py')
        ),
        launch_arguments={
            'slam_params_file': os.path.join(
                pkg_star_bringup, 'config', 'slam_toolbox.yaml'
            ),
            'use_sim_time': 'true',
        }.items(),
    )

    # ------------------------------------------------------------------ #
    # Nav2 (reuses star_bringup config)                                    #
    # ------------------------------------------------------------------ #
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            PathJoinSubstitution([
                FindPackageShare('nav2_bringup'), 'launch',
                'navigation_launch.py',
            ])
        ),
        launch_arguments={
            'use_sim_time': 'true',
            'params_file': os.path.join(
                pkg_star_bringup, 'config', 'nav2_params.yaml'
            ),
            'map_subscribe_transient_local': 'true',
        }.items(),
        condition=IfCondition(LaunchConfiguration('use_nav2')),
    )

    # ------------------------------------------------------------------ #
    # RViz                                                                 #
    # ------------------------------------------------------------------ #
    rviz = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        output='screen',
        arguments=[
            '-d', os.path.join(pkg_star_simulation, 'rviz', 'simulation.rviz'),
        ],
        parameters=[{'use_sim_time': True}],
        condition=IfCondition(LaunchConfiguration('use_rviz')),
    )

    return LaunchDescription([
        # Arguments
        world_arg,
        use_rviz_arg,
        headless_arg,
        use_nav2_arg,
        use_ekf_arg,
        # Environment
        gz_resource_path,
        gz_display,
        # Gazebo
        gazebo,
        gazebo_headless,
        spawn_robot,
        # ROS2 nodes
        robot_state_publisher,
        bridge,
        ekf,
        static_odom_tf,
        slam,
        nav2,
        rviz,
    ])
