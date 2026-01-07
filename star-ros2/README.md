# STAR ROS2 Integration

ROS2 Jazzy integration for the STAR (Simultaneous Tracking and Robotics) platform, providing high-level orchestration for autonomous navigation, sensor fusion, and robot control.

## Overview

This workspace implements a Visual-LiDAR sensor fusion architecture using **RTAB-Map** for SLAM and **robot_localization** for local state estimation. The system bridges ROS2 with the STAR platform's RX72N motor controller, Go gateway service, and user interface.

**Documentation:** For detailed architecture and design decisions, see [`../docs/sections/10_ros2_integration.tex`](../docs/sections/10_ros2_integration.tex)

### Architecture Highlights

**Sensor Fusion Strategy:**
- **Local Estimation:** `robot_localization` EKF fuses wheel odometry + IMU
- **Global SLAM:** `rtabmap_ros` performs Visual-LiDAR fusion with loop closure detection

**Coordinate Frames (REP-105):**
```
map (RTAB-Map - global, corrected)
  └─ odom (EKF - local, drift-prone)
      └─ base_link (robot center)
          ├─ laser_frame (RPLiDAR C1)
          └─ camera_link (OAK-D RGB-D camera)
```

**Node Communication Flow:**
```
/cmd_vel → star_spi_bridge → /odom/unfiltered
                           → /joint_states
                           → [SPI @ 10MHz] → RX72N

/odom/unfiltered + IMU → robot_localization (EKF) → /odom/filtered

/odom/filtered + /scan + RGB-D → rtabmap_ros → /map
```

---

## Development Environment

### Prerequisites

- **Docker** (for containerized ROS2 environment)
- **VS Code** with **Dev Containers** extension
- **Hardware** (for deployment): Raspberry Pi 5, SPI/USB/video device passthrough

### Setup

1. **Open the repository in VS Code**
   ```bash
   code .
   ```

2. **Reopen in Container**
   - VS Code will prompt: "Folder contains a Dev Container configuration file"
   - Click "Reopen in Container" (or run command: `Dev Containers: Reopen in Container`)

3. **Wait for container build** (~5 minutes first time)
   - Base image: `osrf/ros:jazzy-desktop`
   - Auto-installs: colcon, rosdep, ROS2 tools

4. **Verify environment**
   ```bash
   ros2 doctor  # Should show ROS2 Jazzy setup
   ```

**Hardware Passthrough (for deployment on RPi5):**
- `/dev/spidev*` - RX72N motor controller
- `/dev/ttyUSB*` - RPLiDAR C1 LiDAR
- `/dev/video*` - OAK-D RGB-D camera
- `/dev/bus/usb` - USB devices

---

## Building

```bash
# Navigate to workspace
cd star-ros2

# Build all packages
colcon build --symlink-install

# Source the workspace
source install/setup.bash

# (Optional) Build specific package
colcon build --packages-select star_bringup
```

**Build artifacts:**
- `build/` - CMake build directory (gitignored)
- `install/` - Install space with binaries and setup scripts (gitignored)
- `log/` - Build logs (gitignored)

---

## Running Nodes

**Note:** This PR contains only infrastructure. Application nodes are in separate PRs:
- Motor control: See [PR #145](https://github.com/Locked-Inc/STAR/pull/145) (`star_spi_bridge`)
- Gateway bridge: See [PR #146](https://github.com/Locked-Inc/STAR/pull/146) (`star_gateway_bridge`)

### Verifying the Build

```bash
# List installed packages
ros2 pkg list | grep star

# Expected output:
# star_bringup
```

### Safety Monitor (Watchdog)

```bash
# TODO: Implementation tracked in Issue #139
# ros2 run star_safety_monitor safety_monitor_node
```

**Status:** Placeholder only. See [Issue #139](https://github.com/Locked-Inc/STAR/issues/139).

---

## Testing

```bash
# Test all packages
colcon test

# Test specific package
colcon test --packages-select star_spi_bridge

# View test results
colcon test-result --verbose

# (Coming soon) Run with coverage
# colcon test --pytest-with-coverage
```

**Current Status:** Unit tests deferred to implementation issues (per spec.md scope).

---

## Package Status

| Package | Status | Lines | Description | Pull Request |
|---------|--------|-------|-------------|--------------|
| `star_bringup` | 🟢 Skeleton | 25 | Launch files and system bringup | This PR (#144) |
| `star_spi_bridge` | 🔴 Not Included | - | SPI communication to RX72N | [PR #145](https://github.com/Locked-Inc/STAR/pull/145) |
| `star_gateway_bridge` | 🔴 Not Included | - | gRPC bridge to Go gateway | [PR #146](https://github.com/Locked-Inc/STAR/pull/146) |
| `star_safety_monitor` | 🔴 Planned | - | Battery, current, heartbeat monitor | TBD |

**Note:** This infrastructure PR (#144) establishes the build system, devcontainer, and CI/CD pipeline. Application packages are in separate PRs for easier review.

**SLAM Configuration:** Not yet implemented. See future issue.

### Included in This PR

**✅ Infrastructure:**
- Docker container (Ubuntu 24.04 + ROS2 Jazzy)
- VS Code devcontainer configuration
- GitHub Actions CI/CD pipeline (build + lint)
- Protocol Buffer C++ code generation
- Skeleton `star_bringup` package (validates build system)

**⚠️ Coming in Separate PRs:**
- `star_spi_bridge`: SPI motor control node ([PR #145](https://github.com/Locked-Inc/STAR/pull/145))
- `star_gateway_bridge`: gRPC bridge to Go gateway ([PR #146](https://github.com/Locked-Inc/STAR/pull/146))
- SLAM configuration and launch files (future)

---

## Architecture

### Node Graph

```
┌─────────────────────────────────────────────────────────────┐
│                         ROS2 Graph                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  /cmd_vel (geometry_msgs/Twist)                            │
│     │                                                       │
│     ├──> star_spi_bridge ──> /odom/unfiltered (nav_msgs/Odometry)
│     │                    └──> /joint_states (sensor_msgs/JointState)
│     │                    └──> [SPI] → RX72N Motor Controller
│     │                                                       │
│  /odom/unfiltered + /imu/data                              │
│     │                                                       │
│     └──> robot_localization (EKF)                          │
│              └──> /odom/filtered (nav_msgs/Odometry)       │
│                                                             │
│  /odom/filtered + /scan (sensor_msgs/LaserScan)            │
│                 + /rgb/image_raw + /depth/image_raw        │
│     │                                                       │
│     └──> rtabmap_ros (SLAM)                                │
│              ├──> /map (nav_msgs/OccupancyGrid)            │
│              └──> /rtabmap/grid_map (3D octomap)           │
│                                                             │
│  star_gateway_bridge (gRPC ↔ Go Gateway)                   │
│     ├──> Subscribes: /robot_status, /battery_state         │
│     └──> Publishes: /teleop/cmd_vel                        │
│                                                             │
│  star_safety_monitor (Watchdog)                            │
│     ├──> Monitors: Battery, Current, Heartbeat             │
│     └──> Publishes: /emergency_stop (std_msgs/Bool)        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### TF Tree

```
map (published by rtabmap_ros)
 └─ odom (published by robot_localization)
     └─ base_link (robot center)
         ├─ laser_frame (RPLiDAR C1 mount)
         ├─ camera_link (OAK-D camera mount)
         ├─ wheel_left_link (left wheel)
         └─ wheel_right_link (right wheel)
```

**Static Transforms:** Defined in launch files (TODO: Issue #140)

---

## Contributing

### Coding Standards

This project follows strict coding standards for safety-critical embedded systems:

- **[CLAUDE.md](../CLAUDE.md)** - Project-wide coding conventions
- **[NASA Power of 10](../docs/sections/06_nasa_power_of_10.tex)** - Safety-critical rules
- **[SOLID Principles for C](../CLAUDE.md#solid-principles-for-c-star-implementation)** - Clean architecture

**Key Rules:**
- Zero dynamic allocation (safety-critical)
- All integer constants must be enums (no magic numbers)
- Check all return values (no unchecked errors)
- Use conventional commit messages (feat/fix/docs/test)

### Testing Requirements

All new nodes must include:
- Unit tests (gtest + gmock for mocking)
- Integration tests (hardware-in-the-loop where applicable)
- Minimum 80% code coverage

### Submitting Changes

```bash
# Create feature branch
git checkout -b feature/implement-spi-io

# Make changes, commit with conventional format
git commit -m "feat(spi): implement SPI device initialization with ioctl

Add SPI configuration using ioctl() system calls:
- Set SPI mode (CPOL=0, CPHA=0)
- Configure 10MHz clock speed
- Set 8-bit word size
- Test with loopback mode

Related to #137"

# Push and create PR
git push -u origin feature/implement-spi-io
gh pr create --title "feat(spi): implement SPI device initialization" --body "Closes #137"
```

---

## Troubleshooting

### Build Failures

**Problem:** `CMake Error: Could not find package "rclcpp"`
```bash
# Solution: Install dependencies
rosdep update
rosdep install --from-paths src --ignore-src -r -y
```

**Problem:** `colcon: command not found`
```bash
# Solution: Source ROS2 setup
source /opt/ros/jazzy/setup.bash
```

### Runtime Issues

**Problem:** `star_spi_bridge: Waiting for first cmd_vel message...`
```bash
# Solution: Publish test message
ros2 topic pub /cmd_vel geometry_msgs/Twist '{linear: {x: 0.5}, angular: {z: 0.0}}'
```

**Problem:** `SPI device /dev/spidev0.0 not found`
```bash
# Solution: Check device exists and permissions
ls -l /dev/spidev*
# Add user to spi group
sudo usermod -a -G spi $USER
```

### Devcontainer Issues

**Problem:** Container won't build
```bash
# Solution: Rebuild without cache
Dev Containers: Rebuild Container Without Cache
```

**Problem:** Hardware devices not accessible
```bash
# Solution: Ensure privileged mode in devcontainer.json
# Already configured in this repo
```

---

## Additional Resources

- **ROS2 Jazzy Documentation:** https://docs.ros.org/en/jazzy/
- **RTAB-Map:** https://introlab.github.io/rtabmap/
- **robot_localization:** http://docs.ros.org/en/noetic/api/robot_localization/html/
- **REP-105 (Coordinate Frames):** https://www.ros.org/reps/rep-0105.html
- **STAR Project Documentation:** [`../docs/star_documentation.pdf`](../docs/star_documentation.pdf)
- **Issue Tracker:** https://github.com/Locked-Inc/STAR/issues

---

## License

See root repository LICENSE file.

---

**Status:** Infrastructure phase complete. Implementation tracked in Issues #137-#140.
