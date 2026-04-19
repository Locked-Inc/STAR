# STAR ROS2 Integration

ROS2 Jazzy integration for the STAR (Spatial Topography Accessibility Robot) platform, providing high-level orchestration for autonomous navigation, sensor fusion, and robot control.

## Overview

This workspace implements a LiDAR-based SLAM and autonomous navigation architecture using **slam_toolbox** for mapping and **robot_localization** for local state estimation. The system bridges ROS2 with the STAR platform's RX72N motor controller, Go gateway service, and user interface.

**Documentation:** For detailed architecture and design decisions, see [`../docs/sections/10_ros2_integration.tex`](../docs/sections/10_ros2_integration.tex)

### Architecture Highlights

**Sensor Fusion Strategy:**
- **Local Estimation:** `robot_localization` EKF fuses wheel encoder odometry + IMU (yaw + angular rate)
- **Global SLAM:** `slam_toolbox` async mapper produces `/map` and corrects `map->odom` drift
- **Autonomous Navigation:** Nav2 stack (NavFn A* + DWB controller + costmaps) drives to goals
- **Frontier Exploration:** `explore_lite` (m-explore-ros2) detects frontiers, sends Nav2 goals

**Coordinate Frames (REP-105):**

```text
map  (slam_toolbox -- global, loop-closure corrected)
 +- odom  (robot_localization EKF -- local, drift-prone, 50 Hz)
     +- base_link  (robot body center)
         +- laser_frame  (RPLiDAR C1, z = +0.05 m)
         +- imu_link     (RX72N IMU, co-located with base_link)
```

**Node Communication Flow:**

```text
/cmd_vel  --> star_spi_bridge --> /odom/unfiltered  -+
                              +--> /imu/data          +--> EKF --> /odometry/filtered
                              +--> /joint_states       |          +--> odom->base_link TF
                              +--> [SPI @ 10MHz] --> RX72N

/scan (RPLiDAR C1) --> slam_toolbox --> /map + map->odom TF

/map + /odometry/filtered + /scan --> Nav2 --> /cmd_vel (autonomous)

/map + Nav2 costmaps --> explore_lite --> Nav2 goals (frontier exploration)
```

---

## Development Environment

### Prerequisites

- **Docker** (for containerized ROS2 environment) -- or --
- **Raspberry Pi 5** with ROS2 Jazzy natively installed (see [docs/PI_DEPLOYMENT.md](../docs/PI_DEPLOYMENT.md))
- **VS Code** with **Dev Containers** extension (devcontainer path only)

### Setup -- Devcontainer (x86/macOS development)

1. **Open the repository in VS Code**

   ```bash
   code /path/to/STAR
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

### Setup -- Pi Native (Raspberry Pi 5 aarch64)

```bash
# Source ROS2 and workspace
source /opt/ros/jazzy/setup.bash
source /workspaces/STAR/star-ros2/install/local_setup.bash  # after first build

# Set RMW
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp

# Build everything (proto gen + colcon)
cd /workspaces/STAR
./build-ros2.sh
```

**Required apt packages:**

```bash
# Safety monitor lifecycle manager
sudo apt install ros-jazzy-nav2-lifecycle-manager

# SLAM + Nav2 navigation stack
sudo apt install ros-jazzy-slam-toolbox ros-jazzy-navigation2 ros-jazzy-nav2-bringup

# Frontier exploration (m-explore-ros2 -- build from source)
cd /workspaces/STAR/star-ros2/src
git clone https://github.com/robo-friends/m-explore-ros2.git
cd /workspaces/STAR && ./build-ros2.sh
```

See [docs/PI_DEPLOYMENT.md](../docs/PI_DEPLOYMENT.md) for full Pi setup including SPI, SSH keys, and GitHub integration.

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

```bash
# Build all packages
./build-ros2.sh   # from repo root -- handles proto gen + colcon

# Source the workspace
source star-ros2/install/local_setup.bash

# List available packages
ros2 pkg list | grep star
# Expected output:
# star_bringup
# star_gateway_bridge
# star_safety_monitor
# star_spi_bridge

# Run all tests
cd star-ros2
colcon test
colcon test-result --verbose
```

---

## Testing

```bash
# Test all packages
colcon test

# View test results
colcon test-result --verbose
```

**Current Status:** 143 tests, 0 failures across all 4 packages (9 gtest + linting).

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
| Idle | Telemetry drop rate | < 1% | [PASS] Expected |
| Active Control | Teleop drop rate | < 1% | [PASS] Critical |
| Active Control | Telemetry drop rate | < 5% | [PASS] Acceptable |
| Stress Test | Telemetry drop rate | < 10% | [WARN] Degraded but acceptable |

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

| Package | Status | Description |
|---------|--------|-------------|
| `star_bringup` | [GREEN] Built | Launch files, URDF, SLAM + Nav2 config |
| `star_spi_bridge` | [GREEN] Built | SPI to RX72N; publishes `/odom/unfiltered` + `/imu/data` |
| `star_gateway_bridge` | [GREEN] Built | gRPC bridge to Go gateway |
| `star_safety_monitor` | [GREEN] Built | Safety watchdog and diagnostics |
| `sllidar_ros2` | [GREEN] Built | RPLiDAR C1 driver (SDK 2.x, DTOF support) |

**SLAM Stack (working):** slam_toolbox async + robot_localization EKF + RPLiDAR C1 -> `/map` at ~0.5 Hz, TF chain map->odom->base_link->laser_frame.

**Autonomous Exploration:** Nav2 (NavFn + DWB) + m-explore-ros2 frontier exploration. See [Autonomous Exploration](#autonomous-exploration) section below.

---

## Autonomous Exploration

STAR can autonomously explore an unknown indoor environment using frontier-based exploration.
The robot navigates toward the boundary between known and unknown space until no frontiers remain.

### Prerequisites

1. Nav2 installed: `sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup`
2. m-explore-ros2 built from source:

   ```bash
   cd /workspaces/STAR/star-ros2/src
   git clone https://github.com/robo-friends/m-explore-ros2.git
   cd /workspaces/STAR && ./build-ros2.sh
   ```
3. RPLiDAR C1 connected to `/dev/ttyUSB0`
4. RX72N SPI bridge running (`star_spi_bridge` in active lifecycle state)

### Hardware Setup

```bash
sudo systemctl stop ModemManager   # must run before launch (grabs ttyUSB0 on boot)
sudo chmod a+rw /dev/ttyUSB0       # grant serial access (until reboot/udev rule applies)
```

### Launch Sequence

```bash
# Terminal 1: Full SLAM + Nav2 stack
ros2 launch star_bringup slam.launch.py

# Terminal 2: Start frontier exploration (after Terminal 1 is stable)
ros2 launch star_bringup explore.launch.py
```

### Monitoring in RViz

Topics to visualize:
- `/map` (OccupancyGrid) -- SLAM map building in real time
- `/explore/frontiers` (MarkerArray) -- frontier candidates
- `/global_costmap/costmap` -- Nav2 global costmap
- `/local_costmap/costmap` -- Nav2 local costmap (3x3 m rolling)
- TF tree -- confirm map->odom->base_link->laser_frame chain

### Expected Behavior

1. Robot begins scanning; `/map` fills in incrementally
2. `explore_lite` detects frontiers (grey/white boundaries on the map)
3. Nav2 plans a path to the nearest frontier; robot drives autonomously
4. Process repeats until `explore_lite` logs: `"No frontiers found, exploration complete"`

### Known Limitations

- Coverage is not guaranteed 100% (small gaps narrower than `min_frontier_size: 0.75 m` are skipped)
- Exploration pauses if Nav2 recovery behaviors exhaust all options (robot may need manual repositioning)
- Virtual RX72N sends no autonomous telemetry in idle mode; real hardware required for full exploration

---

## SLAM Configuration

**Status: Working** (slam_toolbox async, verified on Raspberry Pi 5)

```bash
# Launch SLAM only (no Nav2)
ros2 launch star_bringup slam.launch.py use_nav2:=false

# Verify scan
ros2 topic hz /scan             # should be ~10 Hz

# Verify map
ros2 topic hz /map              # ~0.5 Hz during active mapping

# View TF tree
ros2 run tf2_tools view_frames  # confirms map->odom->base_link->laser_frame
```

**LiDAR:** RPLiDAR C1 via `sllidar_ros2` (SDK 2.x required for C1's DTOF protocol).
Baud: 460800, scan_mode: Standard, frame_id: laser_frame.

**Troubleshooting:**
- `0x80008002` error -> wrong scan_mode or driver; use `sllidar_ros2`, not `rplidar_ros`
- No scan data -> ModemManager grabbed `/dev/ttyUSB0`; run `sudo systemctl stop ModemManager`
- `dialout` group not active -> run `sudo chmod a+rw /dev/ttyUSB0`

---

## Architecture

### Node Graph

```text
+------------------------------------------------------------------+
|                           ROS2 Graph                             |
+------------------------------------------------------------------+
|                                                                  |
|  /cmd_vel (geometry_msgs/Twist)                                 |
|     |                                                            |
|     +--> star_spi_bridge --> /odom/unfiltered (nav_msgs/Odometry)
|                          +--> /imu/data (sensor_msgs/Imu)       |
|                          +--> /joint_states                      |
|                          +--> [SPI @ 10MHz] --> RX72N            |
|                                                                  |
|  /odom/unfiltered + /imu/data                                   |
|     +--> robot_localization (EKF, 50 Hz)                        |
|              +--> /odometry/filtered (nav_msgs/Odometry)        |
|              +--> odom->base_link TF                            |
|                                                                  |
|  /scan (sllidar_node, 10 Hz)                                    |
|     +--> slam_toolbox (async) --> /map + map->odom TF           |
|                                                                  |
|  /map + /odometry/filtered + /scan                              |
|     +--> Nav2 (planner + controller + costmaps)                 |
|              +--> /cmd_vel (autonomous driving)                  |
|                                                                  |
|  Nav2 costmaps + /map                                           |
|     +--> explore_lite --> Nav2 NavigateToPose goals             |
|                                                                  |
|  robot_state_publisher (URDF: base_link, laser_frame, imu_link) |
|                                                                  |
|  star_gateway_bridge (gRPC <-> Go Gateway)                       |
|     +--> Subscribes: /robot_status                              |
|     +--> Publishes: /teleop/cmd_vel                             |
|                                                                  |
|  star_safety_monitor (Watchdog)                                 |
|     +--> Monitors: Heartbeat, Motor Stall                       |
|     +--> Publishes: /emergency_stop (std_msgs/Bool)             |
|                                                                  |
+------------------------------------------------------------------+
```

### TF Tree

```text
map  (slam_toolbox -- global, loop-closure corrected)
 +- odom  (robot_localization EKF -- local, 50 Hz)
     +- base_link  (robot body center)
         +- laser_frame  (RPLiDAR C1, z = +0.05 m -- static via URDF)
         +- imu_link     (RX72N IMU, co-located -- static via URDF)
```

**URDF:** `star-ros2/src/star_bringup/urdf/star.urdf.xacro` loaded by `robot_state_publisher`.

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
./scripts/ros2/format-ros2.sh

# Check formatting without changes (CI mode)
./scripts/ros2/format-ros2.sh --check
```

**Run Code Review:**

```bash
# Automated checklist validation
./scripts/ros2/review-ros2.sh

# Generate report to file
./scripts/ros2/review-ros2.sh --report review.txt
```

**Install Pre-commit Hook (Recommended):**

```bash
# Run quality checks before every commit
cp scripts/git/pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit
```

**Skip Checks (Emergency Only):**

```bash
# Skip pre-commit checks for urgent fixes
git commit --no-verify -m "hotfix: critical issue"
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

**Status:** All 4 packages built and tested on Raspberry Pi 5 (native aarch64). See [docs/PI_DEPLOYMENT.md](../docs/PI_DEPLOYMENT.md) for Pi setup.
