"""
Launch all STAR compliance nodes.

Invocation:

    ros2 launch star_compliance compliance.launch.py

Arguments:
    use_stereo                 - enable stereo-dependent nodes W2/W4
    use_ada_307                - enable the ADA 307 protruding-objects node
    use_path_blockage          - enable the ADA 403.5 path-blockage node
    use_dynamic_obstacles      - enable the DBSCAN clusterer
    use_compliance_monitor     - enable the CPU safety-net monitor
    ada_307_input_topic        - /cloud_map (default) or /stereo/points2

Each node gates its own creation on the corresponding IfCondition so
missing hardware or CPU headroom scenarios are covered by a single
argument flip.
"""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    args = [
        DeclareLaunchArgument("use_stereo", default_value="true"),
        DeclareLaunchArgument("use_ada_307", default_value="true"),
        DeclareLaunchArgument("use_path_blockage", default_value="true"),
        DeclareLaunchArgument("use_dynamic_obstacles", default_value="true"),
        DeclareLaunchArgument("use_compliance_monitor", default_value="true"),
        DeclareLaunchArgument("ada_307_input_topic",
                              default_value="/cloud_map"),
    ]

    # Ramp slope - already implemented, always on.
    ramp_slope = Node(
        package="star_compliance",
        executable="ramp_slope_node",
        name="star_ramp_slope_node",
        output="screen",
    )

    door_clear_width = Node(
        package="star_compliance",
        executable="door_clear_width_node",
        name="star_door_clear_width_node",
        output="screen",
        condition=IfCondition(LaunchConfiguration("use_stereo")),
    )

    door_threshold = Node(
        package="star_compliance",
        executable="door_threshold_node",
        name="star_door_threshold_node",
        output="screen",
    )

    protruding_objects = Node(
        package="star_compliance",
        executable="protruding_objects_node",
        name="star_protruding_objects_node",
        output="screen",
        parameters=[{
            "input_cloud_topic": LaunchConfiguration("ada_307_input_topic"),
        }],
        condition=IfCondition(LaunchConfiguration("use_ada_307")),
    )

    path_blockage = Node(
        package="star_compliance",
        executable="path_blockage_node",
        name="star_path_blockage_node",
        output="screen",
        condition=IfCondition(LaunchConfiguration("use_path_blockage")),
    )

    dynamic_obstacle = Node(
        package="star_compliance",
        executable="dynamic_obstacle_node",
        name="star_dynamic_obstacle_node",
        output="screen",
        condition=IfCondition(LaunchConfiguration("use_dynamic_obstacles")),
    )

    monitor = Node(
        package="star_compliance",
        executable="compliance_monitor_node",
        name="star_compliance_monitor_node",
        output="screen",
        condition=IfCondition(LaunchConfiguration("use_compliance_monitor")),
    )

    return LaunchDescription(args + [
        ramp_slope,
        door_clear_width,
        door_threshold,
        protruding_objects,
        path_blockage,
        dynamic_obstacle,
        monitor,
    ])
