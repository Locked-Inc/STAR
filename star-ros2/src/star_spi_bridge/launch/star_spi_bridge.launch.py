from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode


def generate_launch_description():
    # Declare arguments
    spi_device_path_arg = DeclareLaunchArgument(
        'spi_device_path', default_value='/dev/spidev0.0',
        description='Path to SPI device'
    )

    # Node
    driver_node = LifecycleNode(
        package='star_spi_bridge',
        executable='star_spi_bridge_node',
        name='star_spi_bridge',
        namespace='',
        output='screen',
        parameters=[{
            'spi_device_path': LaunchConfiguration('spi_device_path'),
            'spi_speed_hz': 10000000,
            'cmd_vel_timeout_ms': 500,
            'wheel_base': 0.150,
            'wheel_radius': 0.0325,
            'ticks_per_rev': 11599
        }]
    )

    return LaunchDescription([
        spi_device_path_arg,
        driver_node
    ])
