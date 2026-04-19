# System design

STAR is a distributed ground-robotics platform that separates real-time
motor control from high-level perception and autonomy. A **Raspberry Pi 5**
runs ROS2 Jazzy, the ADA compliance engine, the Go gateway, and the React
UI; a **custom PCB with a Renesas RX72N microcontroller** handles
safety-critical motor PWM, Hall-encoder counting, and low-level sensor
telemetry at hard real-time rates. The two halves communicate over a
10 Mbps SPI link carrying Protocol Buffers with CRC-32 framing, HARQ
re-transmission, and FEC coding. Total hardware bill of materials is
under $2,000.

## Compute and communication

### Raspberry Pi 5 (high-level)

- ROS2 **Jazzy** running on Ubuntu.
- ROS2 packages from `star-ros2/`: `star_bringup` (launch + URDF +
  SLAM/Nav2 configs), `star_spi_bridge` (publishes `/odom/unfiltered`,
  `/imu/data`, `/joint_states` from the SPI link), `star_gateway_bridge`
  (gRPC out to the Go gateway), `star_safety_monitor` (watchdog), and
  `sllidar_ros2` (RPLiDAR C1 driver, SDK 2.x).
- Dual MIPI CSI-2 ports receive the stereo camera feeds.
- USB carries the RPLiDAR C1 feed.

### RX72N motor controller (real-time)

- **R5F572NNHxFB** (144-pin LFQFP, 4 MB Flash, 1 MB SRAM).
- **ThreadX RTOS** with eight concurrent tasks: communication manager,
  motor control (PID at 250 Hz), IMU acquisition, telemetry aggregation,
  obstacle detection (HC-SR04 sonar), LED status, temperature monitoring,
  and watchdog.
- C23 GNU2x with strict NASA Power-of-10 compliance.

### SPI link between Pi 5 and RX72N

- **RSPI2** peripheral at 10 Mbps, DMA double-buffered.
- Frame protocol: SYNC 0x55AA, 16-bit sequence number, CRC-32 IEEE 802.3,
  payload up to 1024 bytes.
- Reliability layer: **HARQ with Chase Combining** plus **convolutional
  FEC (K=7, rate-1/2)**. Peak utilization is 1.6% at 10 Mbps, leaving
  comfortable headroom for future sensor streams.
- Payload serialization: **nanopb** Protocol Buffers (`star.v1.*`).

## Sensors

### Primary geometry: SLAMTEC RPLiDAR C1

360-degree DTOF scanner, 10 Hz, 12 m range, +/- 3 cm accuracy, 460,800
baud. Drives SLAM, accessible-path extraction, and ramp-slope surface
normal estimation. The C1's key strengths for this application - long
indoor range, high angular resolution, and consistent performance
independent of ambient light - make it the backbone of the mapping and
measurement pipeline.

### Stereo depth: Waveshare IMX219-83

- Two **Sony IMX219** 8 MP sensors (3280x2464), mounted at a **60 mm
  baseline** on a common PCB.
- 2.6 mm focal length; 83 degrees diagonal FOV.
- Dual MIPI CSI-2 via a 22-pin ribbon into the RPi5's two camera ports.
- Software-side depth: `libcamera` / `picamera2` capture, OpenCV
  `StereoSGBM` disparity, disparity-to-depth via calibration Q matrix, and
  Open3D point-cloud generation.
- Stretches the platform's visibility into overhead and vertical features
  the LiDAR's horizontal scan plane misses (door frames, thresholds,
  handrails, overhead clearances) - designed into the seven ADA checks as
  the sensor slated for door width and door threshold detection when that
  work is deployed.

### IMUs (two, by design)

- **BNO055** on the RX72N board, I2C on RIIC1 (P20 / P21). 9-DoF fusion
  (Euler, quaternion, accel, gyro, compass). Telemetered over SPI via the
  frame protocol. **Primary for ramp-slope cross-validation**: it is
  mechanically co-located with the drive chassis, so its pitch reading
  directly corresponds to the robot body on the ramp surface.
- **ICM20948** on the IMX219-83 camera board, I2C from the RPi5. 16-bit
  accel / gyro / magnetometer. Mechanically coupled to the stereo cameras,
  which gives the stereo pipeline its own inertial reference for future
  visual-inertial odometry extensions.

### Auxiliary sensors (already wired)

- **4 x HC-SR04** ultrasonic rangefinders. Trigger pins PF5 / PJ5 / PJ3 /
  P33; echo pins P00-P03 (IRQ). Short-range obstacle safety.
- **DS18B20+** 1-Wire temperature sensor on P51. Electronics-bay
  thermal monitoring.
- **BMP280** barometric pressure and temperature.
- Per-motor current sensing on four ADC channels.

## Motor drivetrain

- Four **DFRobot FIT0520** brushed DC gearmotors: 6 V nominal, 210 RPM
  no-load, 34.02:1 gearbox, 341.2 PPR Hall encoders (11,599 counts per
  output-shaft revolution).
- Four **TI DRV8263H** H-bridge drivers with current sensing.
- PWM generation on RX72N **GPTW** (General PWM Timer): 32-bit, 20 kHz.
  Four channels GTIOC0-3, each with A/B complementary outputs for
  sign-magnitude drive.
- Encoder inputs on MTU1 / MTU2 (front wheels, 32-bit quadrature) and
  TPU1 / TPU2 (rear wheels, 16-bit quadrature).
- Control rates: **250 Hz discrete-time PID** on the RX72N (see
  `matlab/pid_discretize_250hz.m` for the discretization derivation);
  **100 Hz velocity commands** published from the Pi 5 over SPI.

## Software stack

- **slam_toolbox** (async mode) is the demo-ready mapper, verified on
  RPi5, emitting `/map` at 0.5 Hz with the full TF chain
  `map -> odom -> base_link -> laser_frame`. **RTAB-Map** fusion of the
  LiDAR scan and the stereo RGB-D stream is the in-progress SLAM path,
  reflected in the repository's most recent commit "Fuse lidar into
  RTAB-Map for 12m range 3D mapping".
- **robot_localization** EKF fuses `/odom/unfiltered` from the SPI bridge
  with `/imu/data` from the BNO055. The EKF's output is the authoritative
  `/odom` topic.
- **Nav2** provides autonomous navigation (NavFn A* planner, DWB
  controller, cost maps).
- **m-explore-ros2** drives frontier exploration so STAR can map an
  unfamiliar hallway without a pre-loaded floor plan.
- The **ADA compliance engine** (new for this capstone) subscribes to
  `/scan`, `/imu/data`, and `/odom`, runs the implemented geometric
  check(s), and writes a CSV log and a PDF audit report. See
  `04_compliance_engine.md` for scope.

## Gateway service

A Go service (`star-gateway`) bridges the UI to ROS2 via **gRPC-Web** on
port 50051 and exposes a WebSocket on port 8080 for real-time telemetry.
Five gRPC services define the API surface: MotorControlService,
TelemetryService, ConfigurationService, FirmwareUpdateService,
BatteryManagementService. The internal link layer implements the same
HARQ + FEC reliability primitives used on the SPI transport, so transport
failover between USB CDC (development) and SPI (production) is
transparent.

## User interface

A **React 19.2 + TypeScript + Vite + Zustand** single-page application
(`star-ui/`) with a tiled dashboard of 20 panels: Teleop, Motor,
Odometry, Battery, Lidar, IMU, SystemHealth, TransportDiag, PidTuning,
Firmware, Nav2Goal, Alerts, TimeSeries, CameraFeed, GpsPanel, and more.
Transport is `@protobuf-ts/grpcweb-transport`, which gives the UI
compile-time type safety against the same `star.v1.*` protobuf schemas
the firmware uses.

## Test status at final-presentation time

Per `star-ros2/IMPLEMENTATION_STATUS.md`: **143 tests, 0 failures** across
the four ROS2 packages. slam_toolbox + Nav2 + frontier exploration have
been run end-to-end on the RPi5. Baseline metrics (frame drops, latency,
CPU utilization) are captured via `star-ros2/scripts/collect_baseline_metrics.sh`
into `BASELINE_METRICS.md`.

## What the compliance engine adds to this stack

The capstone's compliance contribution is one new Python ROS2 node (plus
supporting utilities) that subscribes to the existing sensor and
localization topics, evaluates one ADA geometric check
(ramp slope > 4.76 deg per section 405.2) end-to-end with cross-validated
redundancy, and emits a PDF audit report with an on-map pin and a
measurement table. The remaining six checks are fully architected against
this same platform and sensor stack (Section 4), with two held as
stretch-goal implementations for the seven-day window.
