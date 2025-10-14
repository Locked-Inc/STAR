"""
STAR-Z2 Board Constants
Pin mappings and hardware configuration for STAR robot
"""

# Board identification
BOARD_NAME = "STAR-Z2"
BOARD_REVISION = "1.0"

# LiDAR Configuration
LIDAR_UART_PORT = "/dev/ttyPS1"
LIDAR_BAUD_RATE = 115200

# Stereo Camera Configuration
CAMERA_LEFT = "/dev/video0"
CAMERA_RIGHT = "/dev/video1"
CAMERA_WIDTH = 1280
CAMERA_HEIGHT = 720
CAMERA_FPS = 30

# Motor Control
MOTOR_PWM_FREQUENCY = 50  # Hz
MOTOR_LEFT_CHANNEL = 0
MOTOR_RIGHT_CHANNEL = 1

# Network Configuration
DEFAULT_SSID = "STAR-Robot"
DEFAULT_PORT = 5000

# LED Indicators
LED_POWER = 0
LED_STATUS = 1
LED_ERROR = 2

# Sensor Limits
MAX_LIDAR_RANGE = 12.0  # meters
MIN_LIDAR_RANGE = 0.15  # meters
