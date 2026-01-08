# ROS2 Integration Backlog

Please create the following issues in the GitHub repository to track the detailed implementation of the ROS2 nodes.

## Issue 1: Implement `star_spi_bridge` Node

**Title:** Implement `star_spi_bridge` for SPI communication
**Labels:** enhancement, ros2, low-level

**Description:**
Implement the `star_spi_bridge` ROS2 node to handle communication with the RX72N motor controller.

**Requirements:**
- [ ] Initialize SPI interface (`/dev/spidev*`) using `ioctl`.
- [ ] Subscribe to `/cmd_vel` (geometry_msgs/Twist) and convert to motor setpoints using kinematic model.
- [ ] Periodically read SPI telemetry (encoders, battery) at 50Hz.
- [ ] Publish `/odom/unfiltered` (nav_msgs/Odometry) based on wheel encoders.
- [ ] Publish `/joint_states` (sensor_msgs/JointState) for wheel positions/velocities.
- [ ] Handle SPI transmission errors and timeouts.

---

## Issue 2: Implement `star_gateway_bridge` Node

**Title:** Implement `star_gateway_bridge` for gRPC integration
**Labels:** enhancement, ros2, gateway

**Description:**
Implement the `star_gateway_bridge` node to bridge the Go Gateway service (gRPC/WebSockets) with the ROS2 ecosystem.

**Requirements:**
- [ ] Subscribe to system status topics (`/robot_status`, `/battery_state`).
- [ ] Forward critical status to the Go Gateway via gRPC for UI display.
- [ ] Implement a `teleop` interface to allow the Gateway (WebSocket/Gamepad) to publish to `/teleop/cmd_vel`.
- [ ] Expose ROS2 services for PID tuning (`/set_pid_gains`) triggered by Gateway requests.

---

## Issue 3: Implement `star_safety_monitor` Node

**Title:** Implement `star_safety_monitor` for platform integrity
**Labels:** enhancement, ros2, safety

**Description:**
Implement the `star_safety_monitor` node to ensure the robot operates within safe limits.

**Requirements:**
- [ ] Monitor battery voltage topic and publish warnings/critical alerts.
- [ ] Monitor motor currents and detect stalls.
- [ ] Implement a heartbeat mechanism; if the RPi5 or RX72N is silent for >500ms, trigger an emergency stop.
- [ ] Publish `/emergency_stop` (std_msgs/Bool) to halt all motion.

---

## Issue 4: Configure Visual-LiDAR Fusion Stack

**Title:** Configure RTAB-Map and EKF for Sensor Fusion
**Labels:** enhancement, ros2, slam

**Description:**
Configure `rtabmap_ros` and `robot_localization` to implement the sensor fusion strategy defined in `docs/sections/10_ros2_integration.tex`.

**Requirements:**
- [ ] Create launch files for `robot_localization` (EKF) to fuse Wheel Odometry (`/odom/unfiltered`) and IMU.
- [ ] Create launch files for `rtabmap_ros` to ingest LiDAR scan (`/scan`) and RGB-D data (`/rgb/image_raw`, `/depth/image_raw`).
- [ ] Configure `tf2` static transforms for `base_link` -> `laser_frame` and `base_link` -> `camera_link`.
- [ ] Verify map generation and loop closure detection.
