# star_bringup

Launch orchestration package for the STAR platform. Provides launch files and configuration for SLAM, sensor fusion, Nav2 navigation, and autonomous frontier exploration.

---

## Launch Files

### `slam.launch.py` -- Full SLAM + Navigation Stack

Starts the complete autonomous navigation stack:
- `robot_state_publisher` with URDF (TF tree: base_link -> laser_frame, base_link -> imu_link)
- `sllidar_node` -- RPLiDAR C1 driver at 460800 baud (`/scan` at 10 Hz)
- `ekf_filter_node` -- robot_localization EKF fusing `/odom/unfiltered` + `/imu/data`
- `slam_toolbox` async -- publishes `/map` and `map->odom` TF
- Nav2 navigation stack -- planner + controller + costmaps + behaviors (optional)

```bash
# Full stack (SLAM + Nav2)
ros2 launch star_bringup slam.launch.py

# SLAM only (no Nav2, e.g. for mapping without autonomous navigation)
ros2 launch star_bringup slam.launch.py use_nav2:=false

# Custom LiDAR port
ros2 launch star_bringup slam.launch.py serial_port:=/dev/ttyUSB1
```

**Prerequisites:**
```bash
sudo systemctl stop ModemManager        # prevent ModemManager grabbing ttyUSB0
sudo chmod a+rw /dev/ttyUSB0            # serial permissions until reboot
sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup
```

---

### `explore.launch.py` -- Autonomous Frontier Exploration

Starts the `explore_lite` node from m-explore-ros2. Must be launched **after**
`slam.launch.py` is fully running (map + costmaps populated).

```bash
# Terminal 1
ros2 launch star_bringup slam.launch.py

# Terminal 2 (once SLAM and Nav2 are active)
ros2 launch star_bringup explore.launch.py
```

**Prerequisites (m-explore-ros2 built from source):**
```bash
cd /workspaces/STAR/star-ros2/src
git clone https://github.com/robo-friends/m-explore-ros2.git
cd /workspaces/STAR && ./build-ros2.sh
```

Exploration stops automatically when no frontiers remain (robot has fully explored the space).

---

### `static_transforms.launch.py` -- URDF + TF

Starts `robot_state_publisher` with `star.urdf.xacro`. Publishes the static TF chain:
- `base_link -> laser_frame` (z = +0.05 m, RPLiDAR C1 mount)
- `base_link -> imu_link` (coincident with base_link, RX72N IMU)

This launch is included by `slam.launch.py` and does not need to be started separately.

---

### `safety_monitor.launch.py` -- Safety Watchdog

Starts `star_safety_monitor` in lifecycle-managed mode via `nav2_lifecycle_manager`.
Monitors heartbeat and motor stall; publishes `/emergency_stop` and `/diagnostics`.

```bash
ros2 launch star_bringup safety_monitor.launch.py
```

---

## Configuration Files

### `config/ekf.yaml`
robot_localization EKF configuration. Fuses:
- `odom0: /odom/unfiltered` -- wheel encoder odometry from `star_spi_bridge`
- `imu0: /imu/data` -- IMU orientation + angular velocity from `star_spi_bridge`

Publishes `/odometry/filtered` and the `odom->base_link` TF at 50 Hz.

### `config/slam_toolbox.yaml`
slam_toolbox async mapper configuration. Key params:
- `mode: mapping` (async online)
- Publishes `/map` (OccupancyGrid) and `map->odom` TF

### `config/nav2_params.yaml`
Nav2 navigation stack configuration for STAR differential-drive robot:
- `planner_server`: NavFn A* global planner
- `controller_server`: DWB local planner, max 0.5 m/s, max 1.5 rad/s
- `local_costmap`: 3x3 m rolling window fed by `/scan`
- `global_costmap`: full map extent, fed by `/map` + `/scan`
- `behavior_server`: spin, back_up, drive_on_heading, wait recovery behaviors

### `config/explore_params.yaml`
m-explore-ros2 frontier exploration configuration:
- `planner_frequency: 0.5` Hz frontier re-evaluation
- `min_frontier_size: 0.75` m (ignore tiny openings)
- `progress_timeout: 30.0` s (give up on unreachable frontiers)

### `urdf/star.urdf.xacro`
Robot URDF defining the sensor frame tree. Loaded by `robot_state_publisher`.

---

## TF Tree

```
map  <----- published by slam_toolbox
 +- odom  <----- published by robot_localization EKF (50 Hz)
     +- base_link  <----- robot body origin
         +- laser_frame   (z = +0.05 m -- RPLiDAR C1)
         +- imu_link      (z = 0.0 m  -- RX72N IMU)
```

---

## Verification

```bash
# Check TF tree
ros2 run tf2_tools view_frames

# Verify topics
ros2 topic hz /scan           # ~10 Hz
ros2 topic hz /odometry/filtered   # ~50 Hz
ros2 topic hz /map            # ~0.5 Hz during active mapping
ros2 topic hz /imu/data       # ~100 Hz

# Check Nav2 costmaps (after slam.launch.py)
ros2 topic list | grep costmap

# Check exploration frontiers (after explore.launch.py)
ros2 topic echo /explore/frontiers --once
```
