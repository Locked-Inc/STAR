# Validation and results

STAR's validation spans two layers: the underlying robotics platform
(SLAM, Nav2, motor control, transport) and the compliance engine built
on top of it.

## Platform validation (done, reproducible)

### Test coverage

Per `star-ros2/IMPLEMENTATION_STATUS.md`, the ROS2 codebase has
**143 tests passing with 0 failures** across four packages
(`star_bringup`, `star_spi_bridge`, `star_gateway_bridge`,
`star_safety_monitor`). Run via `colcon test` on the Pi 5.

### SLAM mapping

slam_toolbox (async mode) has been verified producing a `/map` at 0.5 Hz
on the Raspberry Pi 5 with the full TF chain
`map -> odom -> base_link -> laser_frame` active and stable. Loop closure
has been observed on multi-pass runs of the dress-rehearsal hallway.

### Navigation

Nav2 NavFn A* + DWB controller has been verified executing autonomous
point-to-point goals on the mapped environment, with cost-map
inflation configured for the 32 cm robot envelope.

### Frontier exploration

`m-explore-ros2` has been verified driving the robot into a previously
unmapped hallway and returning to a completed map without manual
intervention - the core capability demonstrated in the live demo.

### Transport reliability

The frame protocol + HARQ + FEC stack on the SPI link has been exercised
under induced noise; measured utilization is 1.6% at 10 Mbps, and the
diagnostic stream `TransportHealthReport` publishes packet-loss and
latency at 1 Hz (see the UI's TransportDiag panel).

### Motor control

Discrete-time PID at 250 Hz on the RX72N has been tuned against the
first-order motor model G(s) = 3.665 / (0.075 s + 1) in
`matlab/motor_model_1st_order.m` and discretized in
`matlab/pid_discretize_250hz.m`. Closed-loop velocity step response
meets the per-motor rise-time and overshoot targets captured in
`BASELINE_METRICS.md`.

## Compliance-engine validation (the new contribution)

### Ramp slope (implemented, measured)

**Ground-truth instrument:** Wixey WR300 digital angle gauge, accuracy
+/- 0.1 degrees.

**Measurement protocol:**
1. Identify an indoor ramp on the TAMU campus.
2. Measure the ramp surface at five positions with the Wixey; compute
   the mean to establish ground truth.
3. Drive STAR onto the ramp. Record the BNO055 pitch stream and the
   LiDAR-plane surface-normal output from the ramp_slope node.
4. Log one row per measurement session into `extras/validation_log.csv`.

**Acceptance criteria:**
- Agreement between BNO055 pitch and LiDAR plane normal within
  0.5 degrees on at least 80% of sessions.
- Mean absolute error between STAR's reported slope and the Wixey
  ground truth within 1 degree. Stretch: within 0.5 degrees.
- Correct flag outcome (violation vs. compliant) on 100% of sessions
  where the ramp is clearly one side or the other of 4.76 degrees.

**Reporting:** The live numbers from the dress rehearsal and demo day
are filled into `validation_log.csv` and summarized in
`extras/STAR_ValidationLog.xlsx`. Any metrics stated on the poster or
deck come from those two artifacts, not from estimates.

### Trip hazard (stretch)

Ground truth via Bosch GLM50 laser rangefinder at a known surface
discontinuity (e.g., a deliberately introduced threshold transition on
the chosen campus route). Same measurement structure as ramp slope:
n = 5 minimum, BNO055 jolt signature cross-validated against LiDAR delta.

### Path width (stretch)

Ground truth via Stanley FatMax 25-foot tape at three constrained
sections of the mapped hallway. Compare against the medial-axis
transform output from the occupancy grid at the same coordinates.

### What we are NOT claiming

- No measurement on door clear width, door threshold height, ramp
  width, or ramp landing - these are architected in Section 4 but not
  implemented for the capstone demo.
- No claim of > 95% true-positive detection rate or 0.3-inch mean
  absolute error across the full ADA check set. The honest measured
  numbers go onto the poster, capped at the checks actually run.

## Pre-demo measurement schedule

- **T-6** (dress rehearsal 1): identify the on-campus ramp, measure
  ground truth with the Wixey, capture the first validation row.
- **T-5**: refine plane-segmentation parameters against the captured
  rosbag; capture a second and third row with any corrections.
- **T-4** (dress rehearsal 2 in the actual demo hallway): record the
  contingency MP4 bag and capture the final set of validation rows.
- **T-3**: freeze the validation table; every poster and deck number
  locks to this snapshot.

The platform validation is already done and does not block any of this.
