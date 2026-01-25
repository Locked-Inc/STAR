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
   code /Users/cesarmagana/Documents/GitHub/STAR
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
colcon build --packages-select star_spi_bridge
```

**Build artifacts:**
- `build/` - CMake build directory (gitignored)
- `install/` - Install space with binaries and setup scripts (gitignored)
- `log/` - Build logs (gitignored)

---

## Verifying the Build

This PR contains only infrastructure. To verify the build system works:

```bash
# Open devcontainer
code .

# Build all packages (should succeed with star_bringup skeleton)
colcon build

# Verify build output
ls -la build/ install/

# Expected output:
# build/star_bringup/ (CMake build artifacts)
# install/star_bringup/ (install space with setup scripts)

# Source the workspace
source install/setup.bash

# List available packages
ros2 pkg list | grep star
# Expected output: star_bringup
```

**Application Nodes:**
- `star_spi_bridge` - See [PR #145](https://github.com/Locked-Inc/STAR/pull/145)
- `star_gateway_bridge` - See [PR #146](https://github.com/Locked-Inc/STAR/pull/146)

---

## Testing

```bash
# Test all packages (star_bringup has no tests yet - skeleton only)
colcon test

# View test results
colcon test-result --verbose
```

**Current Status:** This PR contains only infrastructure. Unit tests will be added in application node PRs ([#145](https://github.com/Locked-Inc/STAR/pull/145), [#146](https://github.com/Locked-Inc/STAR/pull/146)).

---

## Performance Baseline Collection

The STAR system includes frame drop diagnostics to monitor communication reliability between ROS2, the Gateway, and the Virtual RX72N simulator. Before integration testing, establish performance baselines to validate system behavior.

### Quick Start

```bash
# Navigate to scripts directory
cd star-ros2/scripts

# Run idle baseline (30 minutes, no commands)
./collect_baseline_metrics.sh 30 idle

# Run active control baseline (30 minutes, 50 Hz commands)
# In separate terminal: ros2 topic pub -r 50 /cmd_vel geometry_msgs/msg/Twist "linear: {x: 0.5}"
./collect_baseline_metrics.sh 30 active_control

# Run stress test (15 minutes, 100 Hz commands + subscribers)
# In separate terminals:
#   ros2 topic echo /odom/unfiltered
#   ros2 topic echo /robot_status
#   ros2 topic pub -r 100 /cmd_vel geometry_msgs/msg/Twist "linear: {x: 1.0}"
./collect_baseline_metrics.sh 15 stress_test
```

### Prerequisites

Before running baseline collection:

1. **Build the Gateway binaries:**
   ```bash
   cd ../star-gateway
   go build ./cmd/star-gateway
   go build ./cmd/virtual_rx72n
   ```

2. **Build ROS2 packages:**
   ```bash
   cd ../star-ros2
   colcon build --packages-select star_gateway_bridge
   source install/setup.bash
   ```

### What Gets Collected

The baseline script automatically:
- Starts Virtual RX72N simulator
- Starts Gateway service
- Starts ROS2 gateway bridge
- Records diagnostics from `/diagnostics` topic
- Collects logs from all components
- Analyzes frame drop statistics
- Generates summary report

**Output Location:** `star-ros2/baselines/<scenario>_<timestamp>/`

**Output Files:**
- `virtual_rx72n.log` - Virtual RX72N simulator output
- `gateway.log` - Gateway service output
- `ros2_bridge.log` - ROS2 bridge output
- `diagnostics.log` - Frame drop statistics
- `SUMMARY.txt` - Analyzed results and performance assessment

### Performance Acceptance Criteria

| Scenario | Metric | Target | Status |
|----------|--------|--------|--------|
| Idle | Telemetry drop rate | < 1% | ✅ Expected |
| Active Control | Teleop drop rate | < 1% | ✅ Critical |
| Active Control | Telemetry drop rate | < 5% | ✅ Acceptable |
| Stress Test | Telemetry drop rate | < 10% | ⚠️ Degraded but acceptable |

### Monitoring During Collection

In a separate terminal, monitor real-time diagnostics:

```bash
# Watch diagnostics (updates every 1 second)
ros2 topic echo /diagnostics

# Watch for frame drops
ros2 topic echo /diagnostics | grep -E "(drop|WARN|ERROR)"
```

### Detailed Documentation

For complete baseline methodology, analysis procedures, and troubleshooting:

**See:** [BASELINE_METRICS.md](./BASELINE_METRICS.md)

---

## Package Status

**This PR (#141) - Infrastructure Only**

| Package | Status | Lines | Description | Pull Request |
|---------|--------|-------|-------------|--------------|
| `star_bringup` | 🟢 Skeleton | 25 | Launch files and system bringup | This PR (#141) |
| `star_spi_bridge` | 🔴 Not Included | - | SPI communication to RX72N | [PR #145](https://github.com/Locked-Inc/STAR/pull/145) |
| `star_gateway_bridge` | 🔴 Not Included | - | gRPC bridge to Go gateway | [PR #146](https://github.com/Locked-Inc/STAR/pull/146) |

**Note:** This PR establishes the foundation (Docker, devcontainer, CI/CD, docs). Application nodes are in separate PRs.

**SLAM Configuration:** Not yet implemented. See [Issue #140](https://github.com/Locked-Inc/STAR/issues/140)

---

## Architecture

**Note:** The diagrams below show the planned system architecture. This PR (#141) contains only infrastructure. Application nodes (`star_spi_bridge`, `star_gateway_bridge`) are in separate PRs ([#145](https://github.com/Locked-Inc/STAR/pull/145), [#146](https://github.com/Locked-Inc/STAR/pull/146)).

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

### Code Quality Tools

**Format Code:**

```bash
# Format all ROS2 C++ files
./scripts/format-ros2.sh

# Check formatting without changes (CI mode)
./scripts/format-ros2.sh --check
```

**Run Code Review:**

```bash
# Automated checklist validation
./scripts/review-ros2.sh

# Generate report to file
./scripts/review-ros2.sh --report review.txt
```

**Install Pre-commit Hook (Recommended):**

```bash
# Run quality checks before every commit
cp scripts/pre-commit-ros2 .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

**Skip Checks (Emergency Only):**

```bash
# Skip pre-commit checks for urgent fixes
SKIP_ROS2_CHECKS=1 git commit -m "hotfix: critical issue"
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

### Devcontainer Issues

**Problem:** Container won't build
```bash
# Solution: Rebuild without cache
Dev Containers: Rebuild Container Without Cache
```

**Problem:** Hardware devices not accessible (RPi5 deployment)
```bash
# Solution: Uncomment device flags in .devcontainer/devcontainer.json
# Devices: /dev/spidev0.0, /dev/ttyUSB0, /dev/video0, /dev/video1
# Note: These are commented out by default for cross-platform compatibility
```

**Problem:** Packages fail to build even though ROS2 is sourced
```bash
# Solution: Ensure rosdep is initialized and updated
bash -c 'if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then sudo rosdep init; fi'
rosdep update --rosdistro jazzy
```

---

## Additional Resources

- **ROS2 Jazzy Documentation:** https://docs.ros.org/en/jazzy/
- **RTAB-Map:** https://introlab.github.io/rtabmap/
- **robot_localization:** https://docs.ros.org/en/noetic/api/robot_localization/html/index.html
- **REP-105 (Coordinate Frames):** https://www.ros.org/reps/rep-0105.html
- **STAR Project Documentation:** [`../docs/star_documentation.pdf`](../docs/star_documentation.pdf)
- **Issue Tracker:** https://github.com/Locked-Inc/STAR/issues

---

## License

See root repository LICENSE file.

---

**Status:** Infrastructure phase complete. Implementation tracked in Issues #137-#140.
