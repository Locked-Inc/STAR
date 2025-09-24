# PYNQ-Z2 Robot System Development Guide

Development workflow and customization guide for the PYNQ-Z2 robot system.

## Table of Contents

1. [Development Environment](#development-environment)
2. [Custom Recipe Development](#custom-recipe-development)
3. [ROS2 Integration](#ros2-integration)
4. [FPGA Development](#fpga-development)
5. [Testing and Debugging](#testing-and-debugging)
6. [Performance Optimization](#performance-optimization)

## Development Environment

### Development Workflow

The recommended development workflow uses Docker for consistent builds and native development for testing:

```bash
# 1. Develop and test recipes in Docker
./docker-run.sh

# 2. Build specific packages
bitbake my-custom-package

# 3. Test on hardware
./flash-sdcard.sh deploy/latest/*.wic /dev/sdX

# 4. Iterate and refine
```

### Setting Up IDE Integration

For VS Code development:

```bash
# Install VS Code extensions
code --install-extension ms-vscode.cpptools
code --install-extension ms-python.python
code --install-extension ms-vscode.cmake-tools
code --install-extension ros.ros

# Configure workspace
cat > .vscode/settings.json << EOF
{
    "python.defaultInterpreterPath": "/usr/bin/python3",
    "cmake.configureOnOpen": true,
    "ros.distro": "humble"
}
EOF
```

### Remote Development

Set up remote development on PYNQ-Z2:

```bash
# On PYNQ-Z2, enable SSH key authentication
ssh-keygen -t rsa -b 4096
cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys

# Install development tools
opkg update
opkg install python3-dev cmake gdb

# Set up VS Code remote development
code --install-extension ms-vscode-remote.remote-ssh
```

## Custom Recipe Development

### Creating a New Recipe

```bash
# Create recipe directory
mkdir -p layers/meta-robot-slam/recipes-robot/my-component

# Create recipe file
cat > layers/meta-robot-slam/recipes-robot/my-component/my-component_1.0.bb << 'EOF'
DESCRIPTION = "My custom robot component"
SECTION = "robot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=..."

SRC_URI = "git://github.com/user/my-component.git;protocol=https;branch=main"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

inherit cmake

DEPENDS = "ros2-core opencv"
RDEPENDS:${PN} = "python3 python3-numpy"

do_install:append() {
    install -d ${D}/opt/robot/components
    install -m 0755 ${S}/scripts/* ${D}/opt/robot/components/
}

FILES:${PN} += "/opt/robot/components/*"
EOF
```

### Recipe Best Practices

1. **Use appropriate inheritance**:
   ```bash
   # For Python packages
   inherit setuptools3
   
   # For CMake projects
   inherit cmake
   
   # For ROS2 packages
   inherit ros2-launch
   
   # For systemd services
   inherit systemd
   ```

2. **Handle dependencies correctly**:
   ```bash
   # Build-time dependencies
   DEPENDS = "package1 package2"
   
   # Runtime dependencies
   RDEPENDS:${PN} = "python3-package runtime-lib"
   
   # Recommended packages
   RRECOMMENDS:${PN} = "optional-package"
   ```

3. **Proper file installation**:
   ```bash
   do_install() {
       # Use install command for proper permissions
       install -d ${D}${bindir}
       install -m 0755 ${S}/my-binary ${D}${bindir}/
       
       install -d ${D}${sysconfdir}/my-component
       install -m 0644 ${S}/config.yaml ${D}${sysconfdir}/my-component/
   }
   ```

### Testing Custom Recipes

```bash
# Test recipe syntax
bitbake -e my-component | grep ^DEPENDS

# Build specific recipe
bitbake my-component

# Clean and rebuild
bitbake -c cleanall my-component
bitbake my-component

# Test on target
bitbake my-component -c package
# Copy to target and test
```

## ROS2 Integration

### Adding ROS2 Packages

Create ROS2 package recipe:

```bash
cat > layers/meta-robot-slam/recipes-robot/robot-slam/robot-slam_1.0.bb << 'EOF'
DESCRIPTION = "Robot SLAM ROS2 package"
SECTION = "robot/ros2"
LICENSE = "Apache-2.0"

inherit ros2-launch

SRC_URI = "file://robot_slam_pkg"
S = "${WORKDIR}/robot_slam_pkg"

DEPENDS = "ros2-core ros2-geometry-msgs ros2-sensor-msgs"
RDEPENDS:${PN} = "ros2-launch python3-numpy"
EOF
```

### ROS2 Launch Files

Create launch configuration:

```python
# files/robot_slam_pkg/launch/robot_slam.launch.py
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='robot_slam',
            executable='slam_node',
            name='slam_node',
            parameters=['/opt/robot/config/slam_config.yaml'],
            output='screen'
        ),
        Node(
            package='robot_slam',
            executable='lidar_processor',
            name='lidar_processor',
            remappings=[
                ('scan', '/scan'),
                ('map', '/map')
            ]
        )
    ])
```

### Custom ROS2 Nodes

Example ROS2 node in Python:

```python
#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
from geometry_msgs.msg import Twist

class RobotController(Node):
    def __init__(self):
        super().__init__('robot_controller')
        
        # Publishers
        self.cmd_pub = self.create_publisher(Twist, '/cmd_vel', 10)
        
        # Subscribers
        self.scan_sub = self.create_subscription(
            LaserScan, '/scan', self.scan_callback, 10)
        
        # Timers
        self.timer = self.create_timer(0.1, self.control_loop)
        
    def scan_callback(self, msg):
        # Process LiDAR data
        min_distance = min([r for r in msg.ranges if r > 0.1])
        self.get_logger().info(f'Min distance: {min_distance:.2f}m')
        
    def control_loop(self):
        # Implement control logic
        cmd = Twist()
        cmd.linear.x = 0.5
        cmd.angular.z = 0.0
        self.cmd_pub.publish(cmd)

def main():
    rclpy.init()
    controller = RobotController()
    rclpy.spin(controller)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
```

## FPGA Development

### Hardware Acceleration Integration

1. **Create Vivado Project**:
   ```tcl
   # create_project.tcl
   create_project robot_accel ./project -part xc7z020clg400-1
   
   # Add IP cores for acceleration
   create_bd_design "robot_system"
   
   # Add processing system
   create_bd_cell -type ip -vlnv xilinx.com:ip:processing_system7:5.5 processing_system7_0
   
   # Add custom IP
   create_bd_cell -type ip -vlnv user.org:user:cv_accelerator:1.0 cv_accelerator_0
   ```

2. **Generate Bitstream Recipe**:
   ```bash
   cat > layers/meta-robot-slam/recipes-fpga/robot-bitstream/robot-bitstream_1.0.bb << 'EOF'
   DESCRIPTION = "Robot FPGA bitstream"
   LICENSE = "MIT"
   
   SRC_URI = "file://robot_system.bit file://robot_system.hwh"
   
   do_install() {
       install -d ${D}/lib/firmware
       install -m 0644 ${WORKDIR}/robot_system.bit ${D}/lib/firmware/
       install -m 0644 ${WORKDIR}/robot_system.hwh ${D}/lib/firmware/
   }
   
   FILES:${PN} = "/lib/firmware/*"
   EOF
   ```

### PYNQ Overlay Development

Create PYNQ overlay:

```python
from pynq import Overlay, allocate
import numpy as np

class RobotOverlay(Overlay):
    def __init__(self, bitfile_name, **kwargs):
        super().__init__(bitfile_name, **kwargs)
        
        # Initialize accelerators
        self.cv_accel = self.cv_accelerator_0
        self.slam_accel = self.slam_accelerator_0
        
    def process_stereo(self, left_img, right_img):
        """Hardware-accelerated stereo processing"""
        # Allocate contiguous memory
        left_buffer = allocate(shape=left_img.shape, dtype=np.uint8)
        right_buffer = allocate(shape=right_img.shape, dtype=np.uint8)
        depth_buffer = allocate(shape=left_img.shape[:2], dtype=np.float32)
        
        # Copy data
        left_buffer[:] = left_img
        right_buffer[:] = right_img
        
        # Configure accelerator
        self.cv_accel.write(0x10, left_buffer.device_address)
        self.cv_accel.write(0x18, right_buffer.device_address)
        self.cv_accel.write(0x20, depth_buffer.device_address)
        
        # Start processing
        self.cv_accel.write(0x00, 0x01)
        
        # Wait for completion
        while not (self.cv_accel.read(0x00) & 0x02):
            pass
            
        return np.array(depth_buffer)
```

## Testing and Debugging

### Unit Testing

Set up unit testing framework:

```python
# test_robot_controller.py
import unittest
import rclpy
from robot_slam.robot_controller import RobotController

class TestRobotController(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        
    @classmethod
    def tearDownClass(cls):
        rclpy.shutdown()
        
    def setUp(self):
        self.controller = RobotController()
        
    def test_initialization(self):
        self.assertIsNotNone(self.controller.cmd_pub)
        self.assertIsNotNone(self.controller.scan_sub)
        
    def test_scan_processing(self):
        # Create mock laser scan
        from sensor_msgs.msg import LaserScan
        scan = LaserScan()
        scan.ranges = [1.0, 2.0, 0.5, 3.0]
        
        # Test callback
        self.controller.scan_callback(scan)
        
        # Add assertions based on expected behavior

if __name__ == '__main__':
    unittest.main()
```

### Integration Testing

```bash
# Create test recipe
cat > layers/meta-robot-slam/recipes-robot/robot-tests/robot-tests_1.0.bb << 'EOF'
DESCRIPTION = "Robot system integration tests"

SRC_URI = "file://test_integration.py file://test_runner.sh"

RDEPENDS:${PN} = "python3-unittest python3-rclpy"

do_install() {
    install -d ${D}/opt/robot/tests
    install -m 0755 ${WORKDIR}/test_*.py ${D}/opt/robot/tests/
    install -m 0755 ${WORKDIR}/test_runner.sh ${D}/usr/bin/
}
EOF
```

### Hardware-in-the-Loop Testing

```python
# hardware_test.py
import rclpy
from sensor_msgs.msg import LaserScan
import time

def test_lidar_connectivity():
    """Test LiDAR sensor connectivity"""
    rclpy.init()
    node = rclpy.create_node('lidar_test')
    
    scan_received = False
    
    def scan_callback(msg):
        nonlocal scan_received
        scan_received = True
        print(f"LiDAR scan received: {len(msg.ranges)} points")
        print(f"Range: {min(msg.ranges):.2f}m - {max(msg.ranges):.2f}m")
    
    subscription = node.create_subscription(
        LaserScan, '/scan', scan_callback, 10)
    
    # Wait for scan data
    timeout = 10.0
    start_time = time.time()
    
    while not scan_received and (time.time() - start_time) < timeout:
        rclpy.spin_once(node, timeout_sec=0.1)
    
    if scan_received:
        print("✅ LiDAR test passed")
    else:
        print("❌ LiDAR test failed - no data received")
    
    rclpy.shutdown()
    return scan_received

if __name__ == '__main__':
    test_lidar_connectivity()
```

### Debugging Tools

1. **ROS2 debugging**:
   ```bash
   # Check node status
   ros2 node list
   ros2 node info /robot_controller
   
   # Monitor topics
   ros2 topic list
   ros2 topic echo /scan
   ros2 topic hz /cmd_vel
   
   # Debug services
   ros2 service list
   ros2 service call /robot_control/stop std_srvs/srv/Empty
   ```

2. **System debugging**:
   ```bash
   # Check system resources
   htop
   iotop
   free -h
   
   # Monitor network
   iftop
   netstat -tulpn
   
   # Check logs
   journalctl -u robot-gateway-bridge -f
   dmesg | tail -20
   ```

## Performance Optimization

### System Tuning

1. **CPU Governor**:
   ```bash
   # Set performance governor
   echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
   ```

2. **Memory optimization**:
   ```bash
   # Tune swappiness
   echo 10 | sudo tee /proc/sys/vm/swappiness
   
   # Configure hugepages
   echo 256 | sudo tee /proc/sys/vm/nr_hugepages
   ```

3. **Real-time scheduling**:
   ```python
   import os
   import sched
   
   # Set real-time priority
   param = os.sched_param(50)
   os.sched_setscheduler(0, os.SCHED_FIFO, param)
   ```

### ROS2 Performance Tuning

```python
# Optimized ROS2 node configuration
class OptimizedController(Node):
    def __init__(self):
        super().__init__('optimized_controller')
        
        # Use custom QoS for low latency
        from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy
        
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.BEST_EFFORT,
            history=HistoryPolicy.KEEP_LAST,
            depth=1
        )
        
        # Create optimized publisher
        self.cmd_pub = self.create_publisher(
            Twist, '/cmd_vel', qos_profile)
        
        # Use callback groups for parallelism
        from rclpy.callback_groups import MutuallyExclusiveCallbackGroup
        self.sensor_group = MutuallyExclusiveCallbackGroup()
        
        self.scan_sub = self.create_subscription(
            LaserScan, '/scan', self.scan_callback, 
            qos_profile, callback_group=self.sensor_group)
```

### FPGA Acceleration

Optimize data transfer:

```python
class OptimizedOverlay(Overlay):
    def __init__(self, bitfile_name):
        super().__init__(bitfile_name)
        
        # Pre-allocate buffers
        self.input_buffer = allocate((480, 640), dtype=np.uint8)
        self.output_buffer = allocate((480, 640), dtype=np.float32)
        
    def process_frame_async(self, frame):
        """Asynchronous frame processing"""
        # Copy data without blocking
        np.copyto(self.input_buffer, frame)
        
        # Start hardware processing
        self.cv_accel.write(0x10, self.input_buffer.device_address)
        self.cv_accel.write(0x18, self.output_buffer.device_address)
        self.cv_accel.write(0x00, 0x01)  # Start
        
        return self.output_buffer
```

### Development Best Practices

1. **Code organization**:
   - Use proper Python packaging
   - Follow ROS2 naming conventions
   - Implement proper error handling
   - Add comprehensive logging

2. **Testing strategy**:
   - Unit tests for individual components
   - Integration tests for system behavior
   - Hardware-in-the-loop tests for real sensors
   - Performance benchmarks

3. **Documentation**:
   - API documentation with Sphinx
   - README files for each component
   - Configuration examples
   - Troubleshooting guides

4. **Version control**:
   - Use semantic versioning
   - Tag releases properly
   - Maintain changelog
   - Use feature branches for development