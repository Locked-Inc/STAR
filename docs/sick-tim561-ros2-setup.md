# SICK TiM561 ROS2 Setup Guide for STAR Robot

Complete step-by-step guide for integrating the SICK TiM561 2D Lidar with ROS2 on the PYNQ-Z2 board.

**Source:** Research compilation from SICK AG documentation, ROS2 documentation, and community resources.

---

## Overview

The SICK TiM561 is a 2D laser scanner that connects via Ethernet and integrates seamlessly with ROS2 using the `sick_scan_xd` driver package.

**Key Features:**
- Native ROS2 support via `sick_scan_xd` driver
- Ethernet interface (no USB/serial required)
- Comprehensive SLAM examples included
- Active maintenance by SICK AG

---

## Hardware Connection and Network Setup

### Step 1: Connect the Sensor

1. Connect the TiM561 to PYNQ-Z2 via Ethernet cable (direct or through switch/router)
2. **Default sensor IP address:** `192.168.0.1`
3. **Default port:** `2112` (TCP)
4. Power the sensor with 24V external supply

**Network Topology Options:**

**Option A: Direct Connection**
```
PYNQ-Z2 Ethernet <--Cable--> TiM561
```

**Option B: Through Switch (Recommended)**
```
PYNQ-Z2 <---> Ethernet Switch <---> TiM561
                    |
                    └---> Development Laptop (for RViz)
```

### Step 2: Configure PYNQ-Z2 Network Interface

The PYNQ-Z2 needs a static IP in the same subnet as the sensor.

**Method 1: Temporary Configuration (Testing)**
```bash
# Find your ethernet interface name
ip addr
# Look for: eth0, enp0s31f6, or similar

# Set static IP (replace eth0 with your interface)
sudo ip addr add 192.168.0.100/24 dev eth0
sudo ip link set eth0 up
```

**Method 2: Permanent Configuration**
```bash
# Edit network configuration
sudo nano /etc/netplan/01-netcfg.yaml
```

Add:
```yaml
network:
  version: 2
  renderer: networkd
  ethernets:
    eth0:  # Replace with your interface name
      dhcp4: no
      addresses:
        - 192.168.0.100/24
      # Optional: Add gateway if you have one
      # gateway4: 192.168.0.1
```

Apply:
```bash
sudo netplan apply
```

### Step 3: Verify Connection

```bash
# Ping the sensor
ping 192.168.0.1
# Expected: replies from 192.168.0.1

# Test TCP connection to COLA-B port
nc -z -v -w5 192.168.0.1 2112
# Expected: "Connection to 192.168.0.1 2112 port [tcp/*] succeeded!"
```

---

## ROS2 Driver Installation

You have two installation options: pre-built binaries or build from source.

### Prerequisites

```bash
# Ensure ROS2 Humble is installed
source /opt/ros/humble/setup.bash

# Install dependencies
sudo apt update
sudo apt install -y \
    ros-humble-diagnostic-updater \
    ros-humble-diagnostic-msgs \
    python3-colcon-common-extensions \
    build-essential \
    git
```

### Option A: Install Pre-built Binaries (Recommended)

**For Ubuntu 22.04 + ROS2 Humble:**

```bash
sudo apt update
sudo apt-get install ros-humble-sick-scan-xd
```

**For other ROS2 distributions:**
- Replace `humble` with your distribution: `foxy`, `iron`, `jazzy`, etc.
- Example: `sudo apt-get install ros-iron-sick-scan-xd`

### Option B: Build from Source

**Reasons to build from source:**
- Ubuntu 20.04 (binary packages require 22.04+)
- Need latest development features
- Custom modifications required

**Build Steps:**

```bash
# 1. Create workspace
mkdir -p ~/sick_scan_ws/src
cd ~/sick_scan_ws/src

# 2. Clone repositories
git clone https://github.com/SICKAG/libsick_ldmrs.git
git clone -b master https://github.com/SICKAG/sick_scan_xd.git

# 3. Return to workspace root
cd ~/sick_scan_ws

# 4. Clean any previous builds
rm -rf ./build ./install ./log

# 5. Source ROS2
source /opt/ros/humble/setup.bash

# 6. Build libsick_ldmrs first
colcon build --packages-select libsick_ldmrs --event-handlers console_direct+

# 7. Source the build
source ./install/setup.bash

# 8. Build sick_scan_xd
colcon build --packages-select sick_scan_xd --cmake-args " -DROS_VERSION=2" --event-handlers console_direct+

# 9. Source the final build
source ./install/setup.bash
```

**Add to bashrc for persistence:**
```bash
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
echo "source ~/sick_scan_ws/install/setup.bash" >> ~/.bashrc
```

---

## Running the Driver

### Basic Launch

**If installed from binaries:**
```bash
source /opt/ros/humble/setup.bash
ros2 launch sick_scan_xd sick_tim_5xx.launch.py hostname:=192.168.0.1
```

**If built from source:**
```bash
cd ~/sick_scan_ws
source ./install/setup.bash
ros2 launch sick_scan_xd sick_tim_5xx.launch.py hostname:=192.168.0.1
```

### Launch with Custom Parameters

```bash
ros2 launch sick_scan_xd sick_tim_5xx.launch.py \
  hostname:=192.168.0.1 \
  port:=2112 \
  cloud_topic:=lidar_cloud \
  frame_id:=laser_frame \
  nodename:=tim561_node \
  min_ang:=-2.35619 \
  max_ang:=2.35619 \
  use_binary_protocol:=true
```

**Common Parameters:**
- `hostname`: IP address of sensor (default: 192.168.0.1)
- `port`: TCP port (default: 2112)
- `cloud_topic`: Point cloud topic name (default: "cloud")
- `frame_id`: TF frame ID (default: "cloud")
- `nodename`: ROS node name
- `min_ang`: Minimum scan angle in radians (default: -2.35619 ≈ -135°)
- `max_ang`: Maximum scan angle in radians (default: 2.35619 ≈ +135°)
- `use_binary_protocol`: Use COLA-B binary protocol (faster)

---

## Verify Data is Streaming

### Check ROS2 Topics

```bash
# List all topics
ros2 topic list
# Expected output:
#   /cloud          (PointCloud2)
#   /scan           (LaserScan)
#   /diagnostics    (DiagnosticArray)
```

### Monitor Topic Data Rate

```bash
# Check point cloud publishing rate
ros2 topic hz /cloud
# Expected: approximately 15 Hz (TiM561 native scan rate)

# Check laser scan rate
ros2 topic hz /scan
# Expected: approximately 15 Hz
```

### Echo Raw Data

```bash
# View point cloud data
ros2 topic echo /cloud

# View laser scan data
ros2 topic echo /scan

# Limit to first message
ros2 topic echo /cloud --once
```

### Topic Information

```bash
# Get topic details
ros2 topic info /cloud
# Type: sensor_msgs/msg/PointCloud2

ros2 topic info /scan
# Type: sensor_msgs/msg/LaserScan

# See full message structure
ros2 interface show sensor_msgs/msg/LaserScan
```

---

## Visualization with RViz2

### Launch RViz2

```bash
rviz2
```

### Configure RViz2

1. **Set Fixed Frame:**
   - In "Global Options" panel
   - Set "Fixed Frame" to `cloud` (or your custom `frame_id`)

2. **Add PointCloud2 Display:**
   - Click **Add** button
   - Select **By topic** → `/cloud` → **PointCloud2**
   - Click **OK**

3. **Configure PointCloud2 Display:**
   - Expand **PointCloud2** in Displays panel
   - **Size (Pixels)**: 3-5 (adjust for visibility)
   - **Style**: Points or Flat Squares
   - **Color Transformer**: Intensity or AxisColor

4. **Add LaserScan Display (Alternative):**
   - Click **Add** → **By topic** → `/scan` → **LaserScan**
   - Useful for 2D visualization

5. **Save Configuration:**
   - File → Save Config As → `~/star_robot_lidar.rviz`
   - Next time: `rviz2 -d ~/star_robot_lidar.rviz`

---

## SLAM Examples and Applications

The `sick_scan_xd` driver includes comprehensive examples for various SLAM frameworks.

### 1. Hector SLAM (Recommended for Testing)

**Install:**
```bash
sudo apt install ros-humble-hector-slam
```

**Run:**
```bash
# Terminal 1: Start TiM561 driver
ros2 launch sick_scan_xd sick_tim_5xx.launch.py hostname:=192.168.0.1

# Terminal 2: Start Hector SLAM
ros2 launch hector_slam_launch tutorial.launch.py

# Terminal 3: Visualize in RViz2
rviz2
```

### 2. SLAM Toolbox (Recommended for Production)

**Install:**
```bash
sudo apt install ros-humble-slam-toolbox
```

**Run:**
```bash
# Terminal 1: Start TiM561 driver
ros2 launch sick_scan_xd sick_tim_5xx.launch.py hostname:=192.168.0.1

# Terminal 2: Start SLAM Toolbox
ros2 launch slam_toolbox online_async_launch.py
```

### 3. Google Cartographer

**Install:**
```bash
sudo apt install ros-humble-cartographer-ros
```

**Configuration:** Requires custom configuration file for TiM561 parameters.

### 4. RTAB-Map (Real-Time Appearance-Based Mapping)

**Install:**
```bash
sudo apt install ros-humble-rtab-ros
```

**Use:** Combines lidar with camera data for RGB-D SLAM.

### 5. OctoMap (3D Occupancy Mapping)

**Install:**
```bash
sudo apt install ros-humble-octomap-server
```

**Use:** Creates 3D voxel-based occupancy maps from lidar data.

---

## Data Recording and Playback

### Record Lidar Data

```bash
# Record all lidar topics
ros2 bag record /cloud /scan /diagnostics

# Record with custom filename
ros2 bag record -o tim561_data_2024 /cloud /scan

# Record for specific duration (60 seconds)
ros2 bag record -d 60 /cloud /scan
```

### Play Back Recorded Data

```bash
# Play recorded data
ros2 bag play tim561_data_2024

# Play at half speed
ros2 bag play --rate 0.5 tim561_data_2024

# Loop playback
ros2 bag play --loop tim561_data_2024
```

### Bag File Information

```bash
# Get bag info
ros2 bag info tim561_data_2024
```

---

## Native API Examples (Without ROS)

The driver provides standalone C++ and Python APIs.

### Minimalistic C++ Example

```bash
cd ~/sick_scan_ws
export LD_LIBRARY_PATH=.:`pwd`/build:$LD_LIBRARY_PATH
./build/minimum_sick_scan_api_client
```

### Minimalistic Python Example

```bash
cd ~/sick_scan_ws/src/sick_scan_xd/test/python/api
python3 minimum_sick_scan_api_client.py
```

### Complete API Test with Visualization

```bash
# C++ version (outputs JPEG images)
./build/sick_scan_xd_api_test ./sick_scan_xd/launch/sick_tim_5xx.launch hostname:=192.168.0.1

# Python version (matplotlib visualization)
python3 sick_scan_xd_api_test.py ./sick_scan_xd/launch/sick_tim_5xx.launch hostname:=192.168.0.1
```

---

## Advanced Features

### Coordinate Transforms

Apply custom transforms to align lidar data with robot frame:

```bash
ros2 launch sick_scan_xd sick_tim_5xx.launch.py \
  hostname:=192.168.0.1 \
  add_transform_xyz_rpy:="0,0,0.5,0,0,1.57"
  # Format: x,y,z,roll,pitch,yaw (meters and radians)
```

**Example: Lidar mounted 0.5m above base, rotated 90°**

### Multiple Sensors

Run multiple TiM561 sensors simultaneously:

```bash
# Terminal 1: Front sensor
ros2 launch sick_scan_xd sick_tim_5xx.launch.py \
  nodename:=tim561_front \
  hostname:=192.168.0.1 \
  cloud_topic:=cloud_front \
  frame_id:=laser_front

# Terminal 2: Rear sensor
ros2 launch sick_scan_xd sick_tim_5xx.launch.py \
  nodename:=tim561_rear \
  hostname:=192.168.0.2 \
  cloud_topic:=cloud_rear \
  frame_id:=laser_rear
```

### Software PLL (Timestamp Synchronization)

Automatically synchronizes sensor timestamps with system time for accurate time-stamped data.

**Enabled by default** in `sick_tim_5xx.launch.py`.

### ROS Services for Configuration

The driver exposes ROS services for runtime configuration:

```bash
# List available services
ros2 service list

# Example: Send COLA command
ros2 service call /ColaMsg sick_scan_xd/srv/ColaMsgSrv \
  "{request: 'sMN SetAccessMode 3 F4724744'}"

# Get device state
ros2 service call /SCdevicestate sick_scan_xd/srv/SCdevicestateSrv "{}"
```

---

## Troubleshooting

### Cannot Connect to Sensor

**Symptoms:** No ping response, `nc` connection fails

**Solutions:**
```bash
# 1. Verify network configuration
ip addr show
# Should show 192.168.0.x address on ethernet interface

# 2. Check cable connection
# Verify ethernet link LED on both sensor and board

# 3. Disable other network interfaces temporarily
sudo ip link set wlan0 down  # Disable WiFi if present

# 4. Test with different static IP
sudo ip addr flush dev eth0
sudo ip addr add 192.168.0.50/24 dev eth0
ping 192.168.0.1

# 5. Check sensor power
# Verify 24V power supply is connected and working
```

### No Topics Published

**Symptoms:** `ros2 topic list` shows no lidar topics

**Solutions:**
```bash
# 1. Check if driver is running
ros2 node list
# Should see: /sick_tim_5xx or your custom nodename

# 2. Check for errors in driver output
# Look for error messages in terminal where driver is running

# 3. Verify sensor connection during driver launch
# Driver should print connection success messages

# 4. Test with verbose logging
ros2 launch sick_scan_xd sick_tim_5xx.launch.py \
  hostname:=192.168.0.1 --log-level debug
```

### RViz2 Shows No Data

**Symptoms:** RViz2 opens but no point cloud visible

**Solutions:**
```bash
# 1. Verify data is publishing
ros2 topic hz /cloud
# Should show ~15 Hz

# 2. Check Fixed Frame setting
# In RViz2 Global Options, Fixed Frame must match frame_id
# Default: "cloud"

# 3. Adjust PointCloud2 display settings
# Size: 3-5 pixels
# Style: Flat Squares
# Color Transformer: AxisColor or Intensity

# 4. Check TF tree
ros2 run tf2_tools view_frames
# Verify frame_id exists in TF tree
```

### Permission Denied Errors

**Symptoms:** "Permission denied" when accessing network

**Solution:**
```bash
# Add user to dialout group (if using serial fallback)
sudo usermod -a -G dialout $USER

# For network permissions (if needed)
sudo usermod -a -G netdev $USER

# Log out and log back in for changes to take effect
```

### Build Errors (Source Install)

**`diagnostic_updater.hpp not found`:**
```bash
sudo apt install ros-${ROS_DISTRO}-diagnostic-updater
sudo apt install ros-${ROS_DISTRO}-diagnostic-msgs
```

**`colcon: command not found`:**
```bash
sudo apt install python3-colcon-common-extensions
```

---

## Integration with STAR Robot

### Custom Launch File for STAR Robot

Create `/home/xilinx/star_ws/src/star_robot/launch/lidar.launch.py`:

```python
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    return LaunchDescription([
        # Declare arguments
        DeclareLaunchArgument(
            'hostname',
            default_value='192.168.0.1',
            description='IP address of TiM561 sensor'
        ),

        # Launch TiM561 driver
        Node(
            package='sick_scan_xd',
            executable='sick_generic_caller',
            name='tim561_lidar',
            parameters=[{
                'hostname': LaunchConfiguration('hostname'),
                'port': 2112,
                'frame_id': 'laser',
                'cloud_topic': 'lidar/cloud',
                'scan_topic': 'lidar/scan',
                'use_binary_protocol': True,
            }],
            output='screen'
        ),
    ])
```

**Usage:**
```bash
ros2 launch star_robot lidar.launch.py
```

### System Integration

For autonomous operation, create a systemd service:

```bash
sudo nano /etc/systemd/system/star-lidar.service
```

```ini
[Unit]
Description=STAR Robot Lidar Driver
After=network.target

[Service]
Type=simple
User=xilinx
Environment="ROS_DOMAIN_ID=0"
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && ros2 launch sick_scan_xd sick_tim_5xx.launch.py hostname:=192.168.0.1"
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl enable star-lidar.service
sudo systemctl start star-lidar.service
sudo systemctl status star-lidar.service
```

---

## Official Resources

### SICK AG Support
- **Official Tutorial Playlist:** https://support.sick.com/sick-knowledgebase/article/?code=KA-09385
- Video tutorials for ROS2 integration, launch file configuration, bag recording

### GitHub Repositories
- **sick_scan_xd:** https://github.com/SICKAG/sick_scan_xd
- **libsick_ldmrs:** https://github.com/SICKAG/libsick_ldmrs

### ROS2 Documentation
- **Humble:** https://docs.ros.org/en/humble/p/sick_scan_xd/
- **Iron:** https://docs.ros.org/en/iron/p/sick_scan_xd/

### Product Documentation
- TiM5xx Operating Instructions (SICK AG)
- COLA-B Protocol Specification

---

## Summary Checklist

### Installation
- [ ] ROS2 Humble installed
- [ ] Network configured (192.168.0.x subnet)
- [ ] Sensor connected and reachable (ping test)
- [ ] `sick_scan_xd` driver installed (binary or source)

### Testing
- [ ] Driver launches without errors
- [ ] Topics publishing (`ros2 topic list`)
- [ ] Data streaming at 15 Hz (`ros2 topic hz /cloud`)
- [ ] RViz2 visualization working

### Integration
- [ ] Custom launch file created
- [ ] SLAM framework tested (Hector SLAM or SLAM Toolbox)
- [ ] Data recording verified
- [ ] System service configured (optional)

---

## Next Steps for STAR Robot

1. **Test lidar with current image** (connect and verify data)
2. **During image customization:** Install ROS2 + sick_scan_xd
3. **Integrate with SLAM:** Choose SLAM Toolbox for production
4. **FPGA acceleration:** Move SLAM computations to FPGA (Phase 4)
5. **Full autonomy:** Integrate with navigation stack (Nav2)

All components are well-documented and actively maintained by SICK AG!
