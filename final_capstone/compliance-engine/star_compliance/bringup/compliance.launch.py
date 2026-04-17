"""Launch the STAR ADA compliance engine nodes.

Usage (inside a sourced ROS2 Jazzy workspace that contains the
star_compliance package):

    ros2 launch star_compliance compliance.launch.py

Brings up the implemented ramp-slope node plus the stretch stubs so
the full node graph matches the architecture diagrams.
"""

from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:
    return LaunchDescription([
        Node(
            package="star_compliance",
            executable="ramp_slope_node",
            name="star_ramp_slope_node",
            output="screen",
        ),
        Node(
            package="star_compliance",
            executable="trip_hazard_node",
            name="star_trip_hazard_node",
            output="screen",
        ),
        Node(
            package="star_compliance",
            executable="path_width_node",
            name="star_path_width_node",
            output="screen",
        ),
    ])
