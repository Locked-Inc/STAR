# ROS2 + Java Quick Start

Quick reference for adding ROS2 and Java support to STAR Yocto build.

## Prerequisites

- Completed basic Yocto setup
- ~150GB free disk space
- 8-12 hours for first build

## Setup Steps

### 1. Clone meta-ros

```bash
cd yocto-rpi5
./scripts/add-ros2-java.sh
```

### 2. Configure Build

```bash
source setup-environment.sh
./scripts/configure-ros2-java.sh
```

### 3. Build Image

```bash
bitbake star-minimal-image
```

This will take 8-12 hours on first build.

### 4. Flash and Boot

```bash
sudo ./scripts/flash-sd.sh /dev/sdX
```

## First Boot

```bash
# SSH to Pi
ssh root@192.168.2.100
# Password: star

# Source ROS2
source /opt/ros/jazzy/setup.bash

# Test ROS2
ros2 --version

# Test Java
java -version
```

## Make ROS2 Default

```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

## Test ROS2

```bash
# Terminal 1
ros2 run demo_nodes_cpp talker

# Terminal 2 (new SSH session)
ros2 run demo_nodes_cpp listener
```

## LiDAR Setup (SICK TIM561)

```bash
# Create workspace
mkdir -p ~/ros2_ws/src
cd ~/ros2_ws/src

# Clone sick_scan_xd
git clone https://github.com/SICK-AG/sick_scan_xd.git

# Build
cd ~/ros2_ws
colcon build

# Source
source install/setup.bash

# Run LiDAR node (configure IP first)
ros2 launch sick_scan_xd sick_tim_5xx.launch.py
```

## What's Included

- ROS2 Jazzy (latest stable)
- OpenJDK 21 (LTS)
- Python 3.12+
- Colcon build tools
- All ROS2 core packages

## Detailed Documentation

See [docs/ROS2_JAVA_SETUP.md](docs/ROS2_JAVA_SETUP.md) for:
- Manual setup instructions
- LiDAR integration details
- Development workflow
- Troubleshooting guide
- Cross-compilation setup

## Default Credentials

| User  | Password | Notes                    |
|-------|----------|--------------------------|
| root  | star     | Full system access       |
| star  | star     | Regular user (UID 1000)  |

## Network

| Interface | IP Address      |
|-----------|-----------------|
| eth0      | 192.168.2.100   |
| hostname  | star-pi5        |

## Build Comparison

| Configuration | Build Time | Image Size |
|--------------|------------|------------|
| Minimal      | 4-6 hrs    | ~500MB     |
| + ROS2/Java  | 8-12 hrs   | ~2GB       |

## Need Help?

1. Check [docs/ROS2_JAVA_SETUP.md](docs/ROS2_JAVA_SETUP.md)
2. Check build logs in `build/tmp/work/`
3. Search [ROS Answers](https://answers.ros.org/)
4. Check [meta-ros issues](https://github.com/ros/meta-ros/issues)
