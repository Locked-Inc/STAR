# Plan: Star SPI Bridge ROS2 Node

## Phase 1: Package Structure & SPI Driver Core
Goal: Establish package and implement low-level SPI communication.

- [x] Task: Create package directory structure (`star_spi_bridge`) with CMakeLists.txt and package.xml.
- [x] Task: Implement `SpiDriver` class (CRC-32, framing, ioctl wrapper).
    - [x] Sub-task: Implement CRC-32 lookup table and validation.
    - [x] Sub-task: Implement frame encoding/decoding.
    - [x] Sub-task: Implement full-duplex SPI transfer via ioctl.
- [x] Task: Write unit tests for `SpiDriver`.
    - [x] Sub-task: Test CRC-32 with standard vectors.
    - [x] Sub-task: Test frame encode/decode roundtrip.
- [x] Task: Conductor - User Manual Verification 'Phase 1' (Protocol in workflow.md)

## Phase 2: Message Conversion (Kinematics & Odometry)
Goal: Implement bidirectional ROS2 ↔ Protobuf conversion with robot kinematics.

- [x] Task: Implement `SpiMessageConverter::twist_to_velocity_command()`.
    - [x] Sub-task: Implement differential drive kinematics.
    - [x] Sub-task: Implement NaN/Infinity validation.
- [x] Task: Implement `SpiMessageConverter::telemetry_to_odometry()`.
    - [x] Sub-task: Implement encoder tick to pose integration.
- [x] Task: Implement telemetry conversion for JointState and BatteryState.
- [x] Task: Write unit tests for `SpiMessageConverter`.
    - [x] Sub-task: Test kinematics scenarios (rotation, translation).
    - [x] Sub-task: Test odometry integration.
- [x] Task: Conductor - User Manual Verification 'Phase 2' (Protocol in workflow.md)

## Phase 3: ROS2 Lifecycle Node Integration
Goal: Integrate SPI driver and message converter into a ROS2 lifecycle node.

- [x] Task: Implement `StarSpiDriverNode` structure and parameters.
- [x] Task: Implement lifecycle transitions (configure, activate, deactivate, cleanup).
- [x] Task: Implement 100 Hz timer callback for SPI polling loop.
- [x] Task: Implement `/cmd_vel` subscription with safety timeout.
- [x] Task: Create `star_spi_bridge.launch.py`.
- [x] Task: Conductor - User Manual Verification 'Phase 3' (Protocol in workflow.md)

## Phase 4: SPI Hardware Integration & Testing
Goal: Connect to real RX72N hardware and verify end-to-end communication.

- [x] Task: Configure RPi5 SPI permissions (udev rules, user groups).
- [x] Task: Enable real hardware I/O in `SpiDriver`.
- [x] Task: Perform hardware validation tests.
    - [x] Sub-task: Verify loopback communication.
    - [x] Sub-task: Verify motor control and encoder feedback.
    - [x] Sub-task: Measure round-trip latency.- [x] Task: Conductor - User Manual Verification 'Phase 4' (Protocol in workflow.md)
