# Specification: Star SPI Bridge ROS2 Node

## 1. Overview
Implement `star_spi_bridge`, a ROS2 lifecycle node that enables communication between the Raspberry Pi 5 (ROS2) and the Renesas RX72N motor controller via 10 MHz SPI. This critical component bridges the gap between ROS2 navigation commands (`/cmd_vel`) and real-time motor control.

## 2. Architecture
The system follows a layered architecture:
- **ROS2 Navigation**: Publishes `geometry_msgs/Twist` to `/cmd_vel`.
- **StarSpiDriverNode**: A ROS2 Lifecycle Node that manages the SPI connection.
- **SpiMessageConverter**: Handles bidirectional conversion between ROS2 messages and Protocol Buffers.
- **SpiDriver**: Manages low-level SPI I/O via `/dev/spidev0.0` with CRC-32 validation.
- **RX72N Firmware**: Receives velocity commands and returns telemetry via 100 Hz SPI polling.

## 3. Functional Requirements

### 3.1 SPI Communication
- **Protocol**: 100 Hz full-duplex polling.
- **Transport**: SPI Mode 0 at 10 MHz via `/dev/spidev0.0`.
- **Frame Format**: `[SYNC(0x55AA)][SEQ][LEN][TYPE][FLAGS][PAYLOAD][CRC-32]`.
- **Reliability**: CRC-32 validation (IEEE 802.3).
- **Safety**: 500ms timeout detection (triggers zero-velocity safety command).

### 3.2 Message Conversion
- **Twist to VelocityCommand**:
  - Convert `linear.x` and `angular.z` to left/right wheel velocities.
  - Robot parameters: Wheelbase=150mm, Wheel Radius=32.5mm.
  - Validation: Reject NaN/Infinity values.
- **TelemetryData to Odometry**:
  - Convert encoder ticks to wheel displacement.
  - Integrate pose (x, y, theta) using differential drive kinematics.
  - Publish to `/odom/unfiltered` and `/joint_states`.
- **Battery Monitoring**:
  - Extract voltage and SOC from telemetry and publish to `/battery_state`.

### 3.3 Lifecycle Management
- **Configure**: Initialize SPI driver, declare parameters.
- **Activate**: Start 100 Hz timer, enable publishers.
- **Deactivate**: Stop timer, send zero velocity, disable publishers.
- **Cleanup**: Close file descriptors, reset state.

## 4. Verification & Testing
- **Unit Tests**:
  - CRC-32 validation against standard test vectors.
  - Kinematics conversion logic (Twist <-> Wheel Velocities).
  - Odometry integration accuracy.
- **Hardware Tests**:
  - Loopback test with RX72N.
  - Latency measurement (<10ms target).
  - Emergency stop propagation verification.
