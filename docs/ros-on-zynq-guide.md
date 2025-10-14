# ROS on Zynq: Complete Integration Guide

## ROS Architecture Basics

ROS (Robot Operating System) uses a **publisher-subscriber** model:
- **Nodes** = individual programs (can be C++, Python, etc.)
- **Topics** = named channels for messages
- **Messages** = data structures passed between nodes
- **Services** = request-response interactions

## How ROS Accesses Hardware on Zynq

### Option 1: **ROS Nodes Using Linux Drivers** (Most Common)

Your ROS node (in C++ or Python) uses standard Linux APIs:

#### **C++ Example - GPIO Control:**
```cpp
#include <ros/ros.h>
#include <std_msgs/Bool.h>
#include <gpiod.h>

class GPIOController {
private:
    ros::NodeHandle nh_;
    ros::Subscriber sub_;
    struct gpiod_chip *chip_;
    struct gpiod_line *line_;

public:
    GPIOController() {
        // Open GPIO chip
        chip_ = gpiod_chip_open("/dev/gpiochip0");
        line_ = gpiod_chip_get_line(chip_, 54);
        gpiod_line_request_output(line_, "ros_gpio", 0);

        // Subscribe to ROS topic
        sub_ = nh_.subscribe("gpio_control", 10,
                            &GPIOController::callback, this);
    }

    void callback(const std_msgs/Bool::ConstPtr& msg) {
        // Set GPIO based on ROS message
        gpiod_line_set_value(line_, msg->data ? 1 : 0);
        ROS_INFO("GPIO set to: %d", msg->data);
    }
};

int main(int argc, char** argv) {
    ros::init(argc, argv, "gpio_controller");
    GPIOController controller;
    ros::spin();
    return 0;
}
```

#### **Python Example - I2C Sensor:**
```python
#!/usr/bin/env python3
import rospy
from sensor_msgs.msg import Temperature
from smbus2 import SMBus

class I2CSensorNode:
    def __init__(self):
        rospy.init_node('i2c_sensor')
        self.pub = rospy.Publisher('temperature', Temperature, queue_size=10)
        self.bus = SMBus(0)  # I2C bus 0
        self.device_addr = 0x48  # Example temp sensor address

        # Read at 10 Hz
        self.timer = rospy.Timer(rospy.Duration(0.1), self.read_sensor)

    def read_sensor(self, event):
        # Read from I2C device
        raw_temp = self.bus.read_word_data(self.device_addr, 0x00)

        # Convert and publish
        temp_msg = Temperature()
        temp_msg.header.stamp = rospy.Time.now()
        temp_msg.temperature = raw_temp * 0.0625  # Example conversion
        self.pub.publish(temp_msg)

if __name__ == '__main__':
    try:
        node = I2CSensorNode()
        rospy.spin()
    except rospy.ROSInterruptException:
        pass
```

### Option 2: **ROS with PYNQ for FPGA Access**

If you need to access custom FPGA blocks:

```python
#!/usr/bin/env python3
import rospy
from std_msgs.msg import UInt32
from pynq import Overlay, MMIO

class FPGANode:
    def __init__(self):
        rospy.init_node('fpga_interface')

        # Load FPGA overlay
        self.overlay = Overlay('/home/xilinx/my_overlay.bit')

        # Access custom FPGA block
        self.gpio_ip = self.overlay.axi_gpio_0

        # Subscribe to commands
        self.sub = rospy.Subscriber('fpga_gpio_cmd', UInt32,
                                   self.gpio_callback)

        # Publish status
        self.pub = rospy.Publisher('fpga_gpio_status', UInt32,
                                  queue_size=10)

        # Timer to read FPGA status
        rospy.Timer(rospy.Duration(0.1), self.read_status)

    def gpio_callback(self, msg):
        # Write to FPGA
        self.gpio_ip.write(0x0, msg.data)

    def read_status(self, event):
        # Read from FPGA
        status = self.gpio_ip.read(0x0)
        self.pub.publish(status)

if __name__ == '__main__':
    node = FPGANode()
    rospy.spin()
```

## Lidar with ROS - Real Example

For a typical lidar setup:

### Architecture:
```
Lidar Hardware (UART/USB/Ethernet)
         ↓
   ROS Lidar Driver Node
         ↓ (publishes sensor_msgs/LaserScan)
   ROS Topic: /scan
         ↓
   [Your Processing Node] ← Can subscribe to /scan
         ↓
   ROS Topic: /point_cloud
         ↓
   RViz (on laptop) ← Visualizes the data
```

### Lidar Driver Node (usually provided by manufacturer):
```cpp
// Example structure (simplified)
#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>

int main(int argc, char** argv) {
    ros::init(argc, argv, "lidar_driver");
    ros::NodeHandle nh;
    ros::Publisher scan_pub = nh.advertise<sensor_msgs::LaserScan>("scan", 50);

    // Open serial port to lidar
    int fd = open("/dev/ttyUSB0", O_RDWR);

    ros::Rate rate(10);  // 10 Hz
    while (ros::ok()) {
        sensor_msgs::LaserScan scan;
        scan.header.stamp = ros::Time::now();
        scan.header.frame_id = "laser_frame";

        // Read lidar data from serial
        read_lidar_data(fd, scan);

        // Publish to ROS
        scan_pub.publish(scan);
        rate.sleep();
    }
    return 0;
}
```

## Typical ROS Package Structure

```
your_robot_ws/
├── src/
│   ├── hardware_interface/     # Low-level hardware access
│   │   ├── src/
│   │   │   ├── gpio_node.cpp
│   │   │   ├── i2c_sensor_node.cpp
│   │   │   └── motor_controller_node.cpp
│   │   ├── include/
│   │   ├── CMakeLists.txt
│   │   └── package.xml
│   │
│   ├── lidar_processing/       # Lidar specific code
│   │   ├── src/
│   │   │   └── lidar_processor.cpp
│   │   └── ...
│   │
│   └── robot_control/          # High-level control
│       ├── src/
│       └── ...
```

## CMakeLists.txt for ROS C++ Node

```cmake
cmake_minimum_required(VERSION 3.0.2)
project(hardware_interface)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  sensor_msgs
)

# Link against system libraries
find_package(PkgConfig REQUIRED)
pkg_check_modules(GPIOD REQUIRED libgpiod)

catkin_package(
  CATKIN_DEPENDS roscpp std_msgs sensor_msgs
)

include_directories(
  include
  ${catkin_INCLUDE_DIRS}
  ${GPIOD_INCLUDE_DIRS}
)

# GPIO node
add_executable(gpio_node src/gpio_node.cpp)
target_link_libraries(gpio_node
  ${catkin_LIBRARIES}
  ${GPIOD_LIBRARIES}
)

# I2C sensor node
add_executable(i2c_sensor_node src/i2c_sensor_node.cpp)
target_link_libraries(i2c_sensor_node
  ${catkin_LIBRARIES}
  i2c  # link against i2c library
)
```

## ROS on Zynq: Language Choice

### **C++:**
- Better for real-time performance
- Direct hardware access (mmap, ioctl, etc.)
- Compiled, faster execution
- More verbose

### **Python:**
- Easier/faster development
- Good for high-level logic
- Can be slower for tight loops
- Great for prototyping

### **Mix both!** Common pattern:
- Low-level hardware drivers in C++
- High-level planning/control in Python

## How ROS Gets Installed

When you add the ROS package (as discussed earlier):

```bash
# After installing ROS 2 Humble on your image:
apt install ros-humble-ros-base           # Core ROS
apt install ros-humble-sensor-msgs        # Message definitions
apt install ros-humble-geometry-msgs
apt install ros-humble-urg-node           # Hokuyo lidar driver
apt install ros-humble-sick-scan          # SICK lidar driver

# Development tools
apt install ros-humble-ament-cmake        # Build system
apt install python3-colcon-common-extensions
```

## Workflow on Your Robot

### 1. **Boot Zynq with custom image**
Boots automatically with ROS installed

### 2. **ROS automatically starts** (via systemd service, optional):
```bash
sudo systemctl enable ros-robot.service
sudo systemctl start ros-robot.service
```

### 3. **Launch your robot software**:
```bash
source /opt/ros/humble/setup.bash
ros2 launch my_robot robot.launch.py
```

### 4. **On your laptop** (for visualization):
```bash
export ROS_DOMAIN_ID=0  # Match robot's domain ID
rviz2  # Opens visualization
```

## Example Launch File

**robot.launch.py:**
```python
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # Lidar driver node
        Node(
            package='urg_node',
            executable='urg_node_driver',
            name='lidar',
            parameters=[{
                'serial_port': '/dev/ttyACM0',
                'frame_id': 'laser_frame',
            }]
        ),

        # GPIO controller
        Node(
            package='hardware_interface',
            executable='gpio_node',
            name='gpio_controller'
        ),

        # Motor controller
        Node(
            package='hardware_interface',
            executable='motor_controller',
            name='motor_controller',
            parameters=[{
                'max_speed': 1.0,
                'i2c_bus': 0,
                'i2c_addr': 0x60
            }]
        ),
    ])
```

## Lidar Visualization Setup

### On the Zynq board:
```bash
# Launch lidar driver
ros2 run urg_node urg_node_driver --ros-args -p serial_port:=/dev/ttyUSB0

# Or use launch file
ros2 launch my_robot lidar.launch.py
```

### On your laptop:
```bash
# Make sure you're on the same network
# Check you can ping the robot
ping robot.local

# Set ROS domain ID to match robot (if needed)
export ROS_DOMAIN_ID=0

# Check if you can see the topics
ros2 topic list
# Should see: /scan

# Launch RViz
rviz2

# In RViz:
# 1. Add → LaserScan
# 2. Topic: /scan
# 3. Fixed Frame: laser_frame
# You should now see the lidar data!
```

## Complete Example: Motor Controller with I2C

**motor_controller.cpp:**
```cpp
#include <ros/ros.h>
#include <geometry_msgs/Twist.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

class MotorController {
private:
    ros::NodeHandle nh_;
    ros::Subscriber cmd_vel_sub_;
    int i2c_fd_;
    int device_addr_;

public:
    MotorController(int bus, int addr) : device_addr_(addr) {
        // Open I2C bus
        std::string i2c_dev = "/dev/i2c-" + std::to_string(bus);
        i2c_fd_ = open(i2c_dev.c_str(), O_RDWR);

        if (i2c_fd_ < 0) {
            ROS_ERROR("Failed to open I2C bus");
            return;
        }

        // Set I2C slave address
        if (ioctl(i2c_fd_, I2C_SLAVE, device_addr_) < 0) {
            ROS_ERROR("Failed to set I2C address");
            return;
        }

        // Subscribe to velocity commands
        cmd_vel_sub_ = nh_.subscribe("cmd_vel", 10,
                                     &MotorController::cmdVelCallback, this);

        ROS_INFO("Motor controller initialized on I2C bus %d, addr 0x%02X",
                 bus, addr);
    }

    ~MotorController() {
        if (i2c_fd_ >= 0) {
            close(i2c_fd_);
        }
    }

    void cmdVelCallback(const geometry_msgs::Twist::ConstPtr& msg) {
        // Convert ROS twist to motor commands
        float left_speed = msg->linear.x - msg->angular.z;
        float right_speed = msg->linear.x + msg->angular.z;

        // Send to motor controller via I2C
        uint8_t buffer[4];
        buffer[0] = 0x00;  // Left motor register
        buffer[1] = static_cast<uint8_t>(left_speed * 127);
        buffer[2] = 0x01;  // Right motor register
        buffer[3] = static_cast<uint8_t>(right_speed * 127);

        if (write(i2c_fd_, buffer, 4) != 4) {
            ROS_WARN("Failed to write to motor controller");
        }
    }
};

int main(int argc, char** argv) {
    ros::init(argc, argv, "motor_controller");
    ros::NodeHandle nh("~");

    int i2c_bus, i2c_addr;
    nh.param("i2c_bus", i2c_bus, 0);
    nh.param("i2c_addr", i2c_addr, 0x60);

    MotorController controller(i2c_bus, i2c_addr);
    ros::spin();

    return 0;
}
```

## Summary

- **ROS nodes are regular C++/Python programs** that talk via topics/services
- **Hardware access happens through Linux drivers** (GPIO, I2C, serial, etc.)
- **No special magic** - just normal Linux programming with ROS communication
- **FPGA access** (if needed) can be done via PYNQ or memory-mapped I/O
- **Lidar drivers** are ROS nodes that read from hardware and publish messages
- **Visualization** typically happens on a laptop running RViz
- **Development** can be done directly on the board (code persists on SD card)
