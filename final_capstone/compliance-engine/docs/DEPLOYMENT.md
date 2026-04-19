# Deployment Guide

Step-by-step setup for bringing the STAR compliance engine up on a
fresh Raspberry Pi 5. Written to be followed in order; skip ahead only
if you know exactly what you're doing.

Target environment: **Raspberry Pi 5 (8 GB)**, **Ubuntu 24.04**,
**ROS 2 Jazzy**, IMX219-83 stereo + RPLiDAR C1 + custom RX72N PCB +
4x HC-SR04.

---

## Pre-flight check (T-5 min)

Confirm the pre-existing STAR stack is up. If SLAM + stereo aren't
already working, fix those first - the compliance engine sits on top.

```bash
# LiDAR
ls /dev/rplidar                        # should exist
ros2 topic hz /scan                    # should be ~10 Hz

# Stereo cameras
ls /dev/media2 /dev/media3             # both should exist
ros2 topic hz /cam0/image_raw          # should be ~15 Hz
ros2 topic hz /stereo/disparity        # should be 5-10 Hz

# RX72N -> ROS 2 bridge
ros2 topic hz /imu/data                # should be ~200 Hz
ros2 topic hz /star/obstacle/front_left  # should be ~10 Hz

# SLAM
ros2 topic hz /map                     # should be ~0.5 Hz
ros2 run tf2_ros tf2_echo map base_link  # should not error
```

If any of these fail, consult `star-ros2/README.md` - the compliance
engine will not start without the sensor graph underneath.

---

## Step 1: clone and branch

```bash
git clone https://github.com/Locked-Inc/STAR.git
cd STAR
git checkout main         # or your working branch
```

The compliance engine lives at `final_capstone/compliance-engine/`.
The compliance messages live at `star-ros2/src/star_compliance_msgs/`.

---

## Step 2: bootstrap

```bash
./bootstrap_pi.sh
```

This does:

1. `apt-get install -y ros-jazzy-cv-bridge ros-jazzy-sensor-msgs-py
   ros-jazzy-launch-testing-ament-cmake ros-jazzy-nav2-costmap-2d
   python3-pip python3-colcon-common-extensions`
2. `pip install --user numpy scikit-image scikit-learn opencv-python
   onnxruntime reportlab`
3. `pip install --user open3d` (the long pole, 5-15 min on arm64)
4. Symlinks `final_capstone/compliance-engine/` -> `star-ros2/src/star_compliance/`
5. `colcon build --packages-select star_compliance_msgs star_compliance
   --symlink-install`
6. `ros2 interface show star_compliance_msgs/msg/DoorwayMeasurement`
   to verify the message was generated
7. `ros2 pkg executables star_compliance` to verify the 7 node
   executables are installed
8. `python3 -c "import star_compliance.detectors..."` to verify every
   module imports cleanly

Re-run `bootstrap_pi.sh` any time after pulling a change that touches
compliance-engine or star_compliance_msgs.

### Troubleshooting

- `open3d install failed`: `pip install` sometimes times out on Pi 5.
  Re-run with `pip install --user open3d -v` and watch for
  wheel-build errors. Fallback: disable Open3D-dependent checks by
  launching with `use_ada_307:=false` (see TUNING.md).
- `colcon build failed`: usually a missing apt package.
  `ls /opt/ros/jazzy/share` and verify `sensor_msgs_py`, `cv_bridge`,
  `launch_testing_ament_cmake` are all present.

---

## Step 3: fire up the stack

```bash
./start.sh
```

The script auto-detects LiDAR + stereo + compliance package and
launches every component that's present. Expected boot sequence:

```
[start] WiFi AP configured
[start] Starting virtual_rx72n simulator ...
[start] Starting star-gateway ...
[start] Starting star_spi_bridge ...
[start] Starting SLAM stack (slam.launch.py + foxglove + gateway_bridge + stereo)...
[start] Stereo camera detected (IMX219-83): use_stereo:=true
[start] Waiting ~8 s for LiDAR/stereo init...
[start] SLAM stack running (PID ...)
[start] Starting compliance engine (ros2 launch star_compliance compliance.launch.py)...
[start] compliance engine running (PID ...)
[start] Starting UI dev server ...

Up!
  gateway          PID ...
  spi_bridge       PID ...
  slam             PID ...   /scan @ 10 Hz + stereo (when connected)
  compliance       PID ...   ADA checks on /compliance/*
  gw_bridge        PID ...
```

Total boot time: ~25 seconds end-to-end.

---

## Step 4: verify compliance nodes are online

```bash
# All 7 compliance nodes should be alive
ros2 node list | grep -E 'star_(ramp_slope|door|protruding|path|dynamic|compliance)'
```

Expected output:

```
/star_compliance_monitor_node
/star_door_clear_width_node
/star_door_threshold_node
/star_dynamic_obstacle_node
/star_path_blockage_node
/star_protruding_objects_node
/star_ramp_slope_node
```

Any missing node is a launch failure - check `/tmp/star-logs/compliance.log`.

```bash
# Every compliance topic should have a publisher
ros2 topic info /compliance/door_clear_width
ros2 topic info /compliance/door_threshold
ros2 topic info /compliance/protruding_objects
ros2 topic info /compliance/path_blockage
ros2 topic info /perception/dynamic_obstacles
ros2 topic info /compliance/monitor/status
```

---

## Step 5: first audit run

Drive or autonomously explore an accessible route. The compliance
engine fires on sensor events, not on a timer:

```bash
# Watch door clearances live
ros2 topic echo /compliance/door_clear_width

# Watch protruding-object flags live
ros2 topic echo /compliance/protruding_objects

# Watch the CPU safety monitor
ros2 topic echo /compliance/monitor/status
```

During a traversal, you should see:

- **Door clear-width fires** whenever the robot approaches a doorway
  narrower than 1.1 m in the LiDAR corridor view.
- **Door threshold fires** if the IMU jolt detector trips inside the
  2 s watch window following a door-clear-width event.
- **Path blockage fires** when a previously-mapped corridor suddenly
  narrows to < 36 in for 3+ consecutive scans.
- **Protruding objects fires** at 1 Hz whenever RTAB-Map's
  `/cloud_map` is updating.

---

## Step 6: collect the audit CSVs

Every flagged measurement appends a row to a CSV file under
`final_capstone/extras/`:

| File | Rows written by |
|---|---|
| `validation_log.csv` | `ramp_slope_node` + `door_clear_width_node` |
| `threshold_log.csv` | `door_threshold_node` |
| `protrusion_log.csv` | `protruding_objects_node` |
| `blockage_log.csv` | `path_blockage_node` |

Tail any of these during a run:

```bash
tail -f final_capstone/extras/validation_log.csv
```

---

## Step 7: generate the PDF audit report

The `pdf_generator.py` helper compiles any of the above CSVs into a
shareable report:

```bash
cd final_capstone/compliance-engine
PYTHONPATH=. python3 -m star_compliance.report.pdf_generator
# writes star_audit_report_sample.pdf in the current directory
```

---

## Step 8: shutdown

```bash
./stop.sh
```

Gracefully terminates every launch group, waits for process groups to
exit, and force-kills any orphans matching the compliance-node names.

---

## Known working configuration (2026-04-17)

- Raspberry Pi 5 (8 GB RAM)
- Ubuntu 24.04 LTS with the Raspberry Pi kernel
- ROS 2 Jazzy Jalisco
- libcamera 0.4.x
- RPLiDAR C1 with sllidar_ros2 SDK 2.x
- IMX219-83 with Waveshare 61 mm baseline (measured)
- Custom RX72N PCB firmware commit >= e5a8d693d
- slam_toolbox 2.x async mode
- Nav2 Jazzy binary
- Open3D 0.18 (Pi 5 arm64 wheel from PyPI)

---

## See also

- **[TUNING.md](TUNING.md)** - parameter-by-parameter guidance
- **[VALIDATION.md](VALIDATION.md)** - ground-truth protocols
- **[TROUBLESHOOTING.md](TROUBLESHOOTING.md)** - symptom -> fix
- `star-ros2/README.md` - the platform layer underneath
- `bootstrap_pi.sh` - source of truth for install steps
