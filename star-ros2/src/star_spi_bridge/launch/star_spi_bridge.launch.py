from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LifecycleNode


# Drivetrain kinematics for the goBILDA Wasteland chassis. Source: CAD
# measurement (2026-04) and goBILDA motor datasheet. Must match the
# values in star_bringup/launch/slam.launch.py and the BBB
# hardware_config.h to keep producers and consumers in sync.
WHEEL_BASE_M = 0.356            # track width: left-right wheel center-to-center
WHEEL_RADIUS_M = 0.072          # rolling radius: 144 mm wheel / 2
ENCODER_TICKS_PER_REV = 11599   # 341 PPR Hall encoder x 34.02:1 gearbox

SPI_SPEED_HZ = 10_000_000       # 10 MHz SPI clock
CMD_VEL_TIMEOUT_MS = 500        # command-watchdog timeout


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
            'spi_speed_hz': SPI_SPEED_HZ,
            'cmd_vel_timeout_ms': CMD_VEL_TIMEOUT_MS,
            'wheel_base': WHEEL_BASE_M,
            'wheel_radius': WHEEL_RADIUS_M,
            'ticks_per_rev': ENCODER_TICKS_PER_REV
        }]
    )

    return LaunchDescription([
        spi_device_path_arg,
        driver_node
    ])
