# ROS2 and Java Setup Guide

This guide explains how to build and use the STAR Yocto image with ROS2 Jazzy and OpenJDK 21.

## Overview

The STAR Yocto build now includes:
- **ROS2 Jazzy**: Latest ROS2 distribution compatible with Yocto Scarthgap (5.0)
- **OpenJDK 21**: Latest Long-Term Support (LTS) Java version
- Full ROS2 development tools including colcon build system
- Python 3.12+ for ROS2 nodes and scripts

## Prerequisites

Before adding ROS2 and Java support:
1. Complete the basic Yocto setup (see main README.md)
2. Ensure you have ~150GB free disk space (ROS2 adds significant build size)
3. First build takes 6-12 hours (ROS2 compiles many packages)

## Quick Setup

### Option 1: Automated Setup (Recommended)

```bash
cd yocto-rpi5

# 1. Clone meta-ros layer
./scripts/add-ros2-java.sh

# 2. Setup build environment
source setup-environment.sh

# 3. Configure ROS2 and Java
./scripts/configure-ros2-java.sh

# 4. Build the image
bitbake star-minimal-image
```

### Option 2: Manual Setup

#### Step 1: Clone meta-ros

```bash
cd yocto-rpi5
git clone -b scarthgap https://github.com/ros/meta-ros.git
```

#### Step 2: Add Layers

Edit `build/conf/bblayers.conf` and add these layers to the `BBLAYERS` variable:

```bitbake
BBLAYERS ?= " \
  # ... existing layers ...
  /home/bsikar/Documents/git/STAR/yocto-rpi5/meta-ros/meta-ros-common \
  /home/bsikar/Documents/git/STAR/yocto-rpi5/meta-ros/meta-ros2 \
  /home/bsikar/Documents/git/STAR/yocto-rpi5/meta-ros/meta-ros2-jazzy \
  "
```

#### Step 3: Configure local.conf

Add to `build/conf/local.conf`:

```bitbake
#
# ROS2 Configuration
#
ROS_DISTRO = "jazzy"
DISTRO_FEATURES:append = " systemd"

#
# Java Configuration
#
PREFERRED_VERSION_openjdk-21 = "21%"
```

#### Step 4: Build

```bash
source setup-environment.sh
bitbake star-minimal-image
```

## What's Included

### ROS2 Packages

The image includes:
- `ros-base`: Core ROS2 runtime and libraries
- `ros-core`: Minimal ROS2 installation
- `python3-colcon-common-extensions`: Build tools for ROS2 packages
- `python3-rosdep`: Dependency management
- Full Python 3.12+ support

### Java Packages

- `openjdk-21`: Full JDK for development
- `openjdk-21-jre`: Java Runtime Environment

## Using ROS2 on the Raspberry Pi

### First Boot Setup

After flashing and booting the image:

```bash
# SSH into the Pi
ssh root@192.168.2.100
# Password: star

# Source ROS2 environment
source /opt/ros/jazzy/setup.bash

# Verify ROS2 installation
ros2 --version
```

### Making ROS2 Available by Default

Add to `/root/.bashrc` (or `/home/star/.bashrc`):

```bash
# ROS2 Setup
source /opt/ros/jazzy/setup.bash

# Optional: Set ROS_DOMAIN_ID to avoid interference
export ROS_DOMAIN_ID=42
```

### Testing ROS2

```bash
# Terminal 1: Run a talker node
ros2 run demo_nodes_cpp talker

# Terminal 2 (new SSH session): Run a listener node
ros2 run demo_nodes_cpp listener
```

## Working with LiDAR

### SICK TIM561 Setup

Your SICK TIM561 LiDAR can be integrated with ROS2:

#### Install sick_scan_xd Package

On your development machine (or on the Pi if building from source):

```bash
# Create ROS2 workspace
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

# Clone sick_scan_xd
git clone https://github.com/SICK-AG/sick_scan_xd.git

# Build
cd ~/ros2_ws
colcon build --packages-select sick_scan_xd

# Source the workspace
source install/setup.bash
```

#### Configure for TIM561

Create a launch file `~/tim561_launch.py`:

```python
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='sick_scan_xd',
            executable='sick_generic_caller',
            name='sick_tim561',
            parameters=[{
                'scanner_type': 'sick_tim_5xx',
                'hostname': '192.168.1.1',  # Your LiDAR IP
                'port': 2112,
                'frame_id': 'laser',
            }]
        )
    ])
```

#### Run the LiDAR Node

```bash
ros2 launch tim561_launch.py
```

#### View LiDAR Data

```bash
# In another terminal
ros2 topic echo /scan
```

## Using Java

### Verify Java Installation

```bash
java -version
# Output: openjdk version "21.x.x" ...

javac -version
# Output: javac 21.x.x
```

### Running Java Applications

```bash
# Compile
javac MyProgram.java

# Run
java MyProgram
```

## Development Workflow

### Creating a ROS2 Package on the Pi

```bash
# Create workspace
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

# Create package
ros2 pkg create --build-type ament_python my_robot_pkg

# Build
cd ~/ros2_ws
colcon build

# Source
source install/setup.bash
```

### Cross-Compilation (Advanced)

For faster development, you can cross-compile ROS2 packages on your development machine:

1. Build the SDK:
   ```bash
   bitbake star-minimal-image -c populate_sdk
   ```

2. Install the SDK on your development machine

3. Use the cross-compilation toolchain to build ROS2 packages

## Troubleshooting

### ROS2 Command Not Found

```bash
# Ensure ROS2 is sourced
source /opt/ros/jazzy/setup.bash

# Add to .bashrc to make permanent
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

### Out of Disk Space During Build

ROS2 requires significant disk space:
- Clean build artifacts: `bitbake -c clean star-minimal-image`
- Disable rm_work in local.conf temporarily
- Ensure ~150GB free for ROS2 build

### Network Configuration for ROS2

If running ROS2 nodes across multiple machines:

```bash
# On all machines, set the same ROS_DOMAIN_ID
export ROS_DOMAIN_ID=42

# Ensure network connectivity
ping <other-machine-ip>
```

### Java OutOfMemoryError

If Java applications run out of memory:

```bash
# Increase heap size
java -Xmx512m -jar MyApp.jar
```

## Package List

### Included in packagegroup-star-ros2

- ros-base
- ros-core
- python3
- python3-pip
- python3-colcon-common-extensions
- python3-rosdep
- openjdk-21
- openjdk-21-jre

### Optional (install via opkg)

```bash
opkg update
opkg install ros-dev-tools
opkg install python3-argcomplete
```

## Resources

- [ROS2 Jazzy Documentation](https://docs.ros.org/en/jazzy/)
- [meta-ros Repository](https://github.com/ros/meta-ros)
- [SICK Scan XD](https://github.com/SICK-AG/sick_scan_xd)
- [OpenJDK 21 Documentation](https://openjdk.org/projects/jdk/21/)
- [Colcon Documentation](https://colcon.readthedocs.io/)

## Next Steps

1. **Test ROS2**: Run demo nodes to verify installation
2. **Connect LiDAR**: Configure sick_scan_xd for your SICK TIM561
3. **Develop**: Create custom ROS2 nodes for your robot
4. **Integrate**: Combine LiDAR data with other sensors
5. **Deploy**: Build autonomous navigation stack

## Build Size Comparison

| Configuration | Build Time | Disk Space | Image Size |
|--------------|------------|------------|------------|
| Minimal (no ROS2) | 4-6 hours | ~80GB | ~500MB |
| With ROS2 + Java | 8-12 hours | ~150GB | ~2GB |

## Security Note

The default configuration includes development tools. For production:
1. Remove unnecessary development packages
2. Disable root password login
3. Configure firewall rules for ROS2 ports
4. Use secure ROS2 communications (SROS2)
