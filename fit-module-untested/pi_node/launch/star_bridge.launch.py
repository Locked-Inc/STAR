"""Launch the serial bridge. Optional EKF + slam_toolbox wiring is left
commented as guidance for the v1 deliverable -- v0 just wants /imu/data."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    port_arg = DeclareLaunchArgument("port", default_value="/dev/ttyACM0")
    baud_arg = DeclareLaunchArgument("baud", default_value="115200")

    bridge = Node(
        package="star_serial_bridge",
        executable="star_serial_bridge",
        name="star_serial_bridge",
        output="screen",
        parameters=[{
            "port": LaunchConfiguration("port"),
            "baud": LaunchConfiguration("baud"),
            "frame_id": "imu_link",
            "wheel_base_m": 0.20,
            "max_wheel_mps": 0.5,
        }],
    )

    # Uncomment once /odom is published from a wheel-encoder source:
    # ekf = Node(
    #     package="robot_localization",
    #     executable="ekf_node",
    #     name="ekf_filter_node",
    #     output="screen",
    #     parameters=["config/ekf.yaml"],
    # )

    return LaunchDescription([port_arg, baud_arg, bridge])
