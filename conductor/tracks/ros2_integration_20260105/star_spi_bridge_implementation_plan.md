# Star SPI Bridge ROS2 Node - Implementation Plan

## Overview

Implement `star_spi_bridge`, a ROS2 lifecycle node that enables communication between the Raspberry Pi 5 (ROS2) and the Renesas RX72N motor controller via 10 MHz SPI. This is **Issue #137** - the critical blocker for enabling the `/cmd_vel` → SPI → Motors pipeline.

**Key Achievement**: Bridge the gap between ROS2 navigation commands and real-time motor control.

## Current State

- **star_spi_bridge package**: Does NOT exist (needs creation from scratch)
- **star_gateway_bridge package**: Fully implemented, provides excellent reference pattern
- **RX72N firmware**: Complete SPI communication stack ready (100 Hz polling, Protocol Buffers with nanopb)
- **SPI protocol**: Fully specified in docs/sections/01_nanopb_protocol.tex

## Architecture

```
ROS2 Navigation        star_spi_bridge          RX72N Firmware
----------------       ----------------         ---------------
/cmd_vel (Twist)  →→→  StarSpiDriverNode   →→→  comm_manager.c
                         ↓ ↑                     ↓ ↑
/odom/unfiltered  ←←←  SpiDriver (ioctl)   ←←←  Motor_Controller
/joint_states                                    (4x PID @ 250 Hz)
/battery_state
```

### Component Breakdown

1. **SpiDriver** (`spi_driver.cpp`): Low-level SPI I/O
   - Frame encoding: `[SYNC(0x55AA)][SEQ][LEN][TYPE][FLAGS][PAYLOAD][CRC-32]`
   - CRC-32 validation (IEEE 802.3 polynomial)
   - Full-duplex transfer via `/dev/spidev0.0` ioctl
   - HARQ retry logic (3 attempts, 3ms timeout)

2. **SpiMessageConverter** (`spi_message_converter.cpp`): ROS2 ↔ Protobuf
   - Twist → VelocityCommand (4-wheel differential drive kinematics)
   - TelemetryData → Odometry (dead reckoning from encoder ticks)
   - TelemetryData → JointState (wheel positions/velocities)
   - TelemetryData → BatteryState (voltage, SOC)

3. **StarSpiDriverNode** (`star_spi_driver_node.cpp`): ROS2 Lifecycle Node
   - 100 Hz timer (critical: prevents 500ms E-STOP timeout on RX72N)
   - Subscribes to `/cmd_vel` with timeout detection
   - Publishes `/odom/unfiltered`, `/joint_states`, `/battery_state`
   - Lifecycle management (configure → activate → deactivate → cleanup)

## Protocol Specifications

### SPI Frame Format

```
[SYNC: 0x55AA (2B, BE)]
[SEQ: sequence number (2B, BE)]
[LEN: payload length (2B, BE)]
[TYPE: frame type (1B)]
[FLAGS: control flags (1B)]
[PAYLOAD: protobuf message (0-1024B)]
[CRC-32: IEEE 802.3 (4B, LE)]
```

### Communication Parameters

- **Rate**: 100 Hz (10ms period) - MUST maintain to prevent E-STOP
- **SPI Speed**: 10 MHz
- **SPI Mode**: Mode 0 (CPOL=0, CPHA=0)
- **Device**: `/dev/spidev0.0` on Raspberry Pi 5
- **Timeout**: 500ms on RX72N triggers emergency stop

### Robot Configuration

- **Motors**: 4-wheel differential drive (FL, FR, BL, BR)
- **Encoders**: 341 PPR Hall encoders × 34.02 gear ratio = 11,599 ticks/revolution
- **Wheels**: 65mm diameter (0.0325m radius)
- **Wheelbase**: 150mm (0.150m)
- **Velocity Limits**: ±2.0 m/s per wheel

## Implementation Phases

### Phase 1: Package Structure & SPI Driver Core (Critical Path)

**Goal**: Establish package and implement low-level SPI communication.

**Tasks**:

1. Create package directory structure:

   ```
   star-ros2/src/star_spi_bridge/
   ├── CMakeLists.txt
   ├── package.xml
   ├── include/star_spi_bridge/
   │   ├── spi_driver.hpp
   │   ├── spi_message_converter.hpp
   │   └── star_spi_driver_node.hpp
   ├── src/
   │   ├── spi_driver.cpp
   │   ├── spi_message_converter.cpp
   │   ├── star_spi_driver_node.cpp
   │   └── main.cpp
   ├── test/
   │   ├── test_spi_driver.cpp
   │   └── test_spi_message_converter.cpp
   └── launch/
       └── star_spi_bridge.launch.py
   ```

2. Implement `SpiDriver` class with:
   - CRC-32 lookup table initialization (IEEE 802.3 polynomial: 0x04C11DB7)
   - Frame encoding: sync word, sequence, length, type, flags, payload, CRC
   - Frame decoding: validate sync word, verify CRC-32, extract payload
   - SPI ioctl wrapper for `/dev/spidev0.0`
   - Full-duplex transfer (simultaneous TX command + RX telemetry)

3. Write unit tests:
   - CRC-32 test vector: `"123456789"` → `0xCBF43926` (must match Go implementation)
   - Frame encode/decode roundtrip
   - Sync word detection
   - Sequence number tracking

**Reference Files**:

- `/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_gateway_bridge/CMakeLists.txt` - Build pattern
- `/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_gateway_bridge/package.xml` - Dependencies
- `/Users/cesarmagana/Documents/GitHub/STAR/docs/sections/01_nanopb_protocol.tex` - Protocol spec

**Success Criteria**:

- [ ] Package builds with `colcon build --packages-select star_spi_bridge`
- [ ] Unit tests pass: `colcon test --packages-select star_spi_bridge`
- [ ] CRC-32 matches expected test vector

---

### Phase 2: Message Conversion (Kinematics & Odometry)

**Goal**: Implement bidirectional ROS2 ↔ Protobuf conversion with robot kinematics.

**Tasks**:

1. Implement `SpiMessageConverter::twist_to_velocity_command()`:
   - Extract `linear.x` and `angular.z` from Twist
   - Validate for NaN/infinity (reject if invalid)
   - Apply differential drive kinematics:
     ```
     left_vel = linear.x - (angular.z * wheelbase / 2)
     right_vel = linear.x + (angular.z * wheelbase / 2)
     ```
   - Populate 4-wheel VelocityCommand: `FL=BL=left_vel`, `FR=BR=right_vel`
   - Clamp to ±2.0 m/s
   - Add sequence number and timestamp

2. Implement `SpiMessageConverter::telemetry_to_odometry()`:
   - Extract 4 encoder tick deltas from TelemetryData
   - Convert ticks to wheel displacements:
     ```
     meters_per_tick = (2π × wheel_radius) / ticks_per_rev
     disp_left = (disp_FL + disp_BL) / 2
     disp_right = (disp_FR + disp_BR) / 2
     ```
   - Calculate pose deltas:
     ```
     delta_x = (disp_left + disp_right) / 2
     delta_theta = (disp_right - disp_left) / wheelbase
     ```
   - Integrate pose: `x += delta_x * cos(θ)`, `y += delta_x * sin(θ)`, `θ += delta_theta`
   - Populate Odometry message with pose and twist
   - Add covariance estimates (error grows with distance)

3. Implement `SpiMessageConverter::telemetry_to_joint_state()`:
   - Convert encoder ticks to wheel angles (radians)
   - Populate joint names: `["front_left_wheel", "front_right_wheel", "back_left_wheel", "back_right_wheel"]`

4. Implement `SpiMessageConverter::telemetry_to_battery_state()`:
   - Extract voltage_v, soc_percent from TelemetryData
   - Populate BatteryState message

**Unit Tests**:

- Pure rotation: `linear.x=0, angular.z=1.0` → `left=-wheelbase/2, right=+wheelbase/2`
- Pure translation: `linear.x=1.0, angular.z=0` → `left=right=1.0`
- NaN rejection: `linear.x=NaN` → conversion fails
- Odometry integration: Simulate encoder ticks, verify pose accumulation

**Reference Files**:

- `/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_gateway_bridge/src/message_converter.cpp` - Conversion pattern
- `/Users/cesarmagana/Documents/GitHub/STAR/star-proto/gen/cpp/star/v1/motor_control.pb.h` - VelocityCommand
- `/Users/cesarmagana/Documents/GitHub/STAR/star-proto/gen/cpp/star/v1/telemetry.pb.h` - TelemetryData

**Success Criteria**:

- [ ] Unit tests pass for kinematics (all Twist → VelocityCommand scenarios)
- [ ] Unit tests pass for odometry integration
- [ ] NaN/infinity validation works

---

### Phase 3: ROS2 Lifecycle Node Integration

**Goal**: Integrate SPI driver and message converter into a ROS2 lifecycle node.

**Tasks**:

1. Implement `StarSpiDriverNode` lifecycle transitions:
   - **on_configure()**:
     - Validate `/dev/spidev0.0` exists
     - Initialize SPI driver (10 MHz, mode 0, 8 bits)
     - Declare ROS2 parameters (wheel_base, wheel_radius, ticks_per_rev, spi_rate_hz)
     - Create publishers (lifecycle, not activated)
     - Create `/cmd_vel` subscription
   - **on_activate()**:
     - Activate lifecycle publishers
     - Start 100 Hz timer (`create_wall_timer(10ms)`)
     - Send initial zero velocity command
     - Reset encoder tick tracking
   - **on_deactivate()**:
     - Stop 100 Hz timer
     - Send zero velocity command (safety)
     - Deactivate lifecycle publishers
   - **on_cleanup()**:
     - Close SPI file descriptor
     - Clear all state

2. Implement `spi_timer_callback()` (100 Hz):
   - Check `/cmd_vel` age (timeout if >500ms)
   - Use latest command or zero velocity (safety)
   - Convert Twist → VelocityCommand
   - Serialize protobuf to bytes
   - Call `spi_driver_->send_velocity_command(cmd_payload, telemetry_payload)`
   - Deserialize telemetry protobuf
   - Check `telemetry.emergency_stop()` flag
   - Publish odometry, joint states, battery state

3. Implement `/cmd_vel` subscription callback:
   - Cache latest Twist message (mutex-protected)
   - Record timestamp for timeout detection

4. Create `launch/star_spi_bridge.launch.py`:
   - Launch node with parameters
   - Configure lifecycle manager (optional)

**Parameters**:

```python
parameters=[{
    'spi_device_path': '/dev/spidev0.0',
    'spi_rate_hz': 100,
    'cmd_vel_timeout_ms': 500,
    'wheel_base': 0.150,  # meters
    'wheel_radius': 0.0325,  # meters
    'ticks_per_rev': 11599
}]
```

**Reference Files**:

- `/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_gateway_bridge/src/star_gateway_bridge_node.cpp` - Node pattern
- `/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_gateway_bridge/src/main.cpp` - Entrypoint

**Success Criteria**:

- [ ] Node transitions through lifecycle states: `ros2 lifecycle set /star_spi_driver configure`
- [ ] 100 Hz timer runs: `ros2 topic hz /odom/unfiltered` shows ~100 Hz
- [ ] `/cmd_vel` timeout triggers zero velocity (test by not publishing for >500ms)
- [ ] Publishers activate/deactivate with lifecycle

---

### Phase 4: SPI Hardware Integration & Testing

**Goal**: Connect to real RX72N hardware and verify end-to-end communication.

**Tasks**:

1. Enable real SPI I/O in `SpiDriver::spi_transfer()`:

   ```cpp
   struct spi_ioc_transfer xfer{};
   xfer.tx_buf = reinterpret_cast<uintptr_t>(tx_frame.data());
   xfer.rx_buf = reinterpret_cast<uintptr_t>(rx_frame.data());
   xfer.len = k_max_frame_size;
   xfer.speed_hz = k_spi_speed_hz;
   int ret = ioctl(spi_fd_, SPI_IOC_MESSAGE(1), &xfer);
   ```

2. Configure Raspberry Pi 5 for SPI access:
   - Enable SPI: `sudo raspi-config` → Interface Options → SPI → Enable
   - Add user to spi group: `sudo usermod -a -G spi $USER`
   - Create udev rule: `/etc/udev/rules.d/50-spi.rules`
     ```
     SUBSYSTEM=="spidev", GROUP="spi", MODE="0660"
     ```
   - Reboot: `sudo reboot`

3. Hardware validation tests:
   - **Loopback test**: Verify SPI frames exchange with RX72N
   - **Encoder verification**: Manually rotate motors, check encoder counts in `/joint_states`
   - **Velocity control**: Publish `/cmd_vel`, measure wheel speed with tachometer
   - **Emergency stop**: Trigger E-STOP on RX72N, verify `telemetry.emergency_stop() == true`
   - **Latency measurement**: Measure round-trip time (target <10ms)

4. Error handling:
   - CRC mismatch → retry (up to 3 attempts)
   - SPI timeout → log warning, continue
   - Emergency stop → stop sending commands, log error

**Success Criteria**:

- [ ] SPI communication works (frames exchanged with RX72N at 100 Hz)
- [ ] Encoder data in `/joint_states` matches physical rotation
- [ ] Motors respond to `/cmd_vel` commands
- [ ] Emergency stop detected and propagated
- [ ] Round-trip latency <10ms (99th percentile)
- [ ] Frame success rate >99% at 100 Hz

---

### Phase 5: HARQ Retry & Optimization (Future)

**Goal**: Robust communication with automatic retry and performance tuning.

**Tasks** (lower priority, can be deferred):

1. Implement Stop-and-Wait HARQ:
   - Retry on CRC error (up to 3 attempts, 3ms timeout each)
   - Track ACK/NACK frames
   - Measure retry statistics

2. Optimize performance:
   - Profile SPI transfer latency (use `rclcpp::Clock::now()`)
   - Optimize protobuf serialization (consider arena allocator)
   - Add diagnostics publishing (`diagnostic_msgs/DiagnosticArray`)

3. Chase Combining (advanced error recovery):
   - Soft bit storage from failed attempts
   - Viterbi decoding with FEC (rate-1/2 convolutional code)

**Success Criteria**:

- [ ] Retry logic improves frame success rate to >99.9%
- [ ] CPU usage <10% on RPi5
- [ ] No missed 100 Hz deadlines

---

## Critical File Paths

### Source Files (to create)

```
/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_spi_bridge/
├── CMakeLists.txt                          # Build configuration
├── package.xml                              # ROS2 dependencies
├── include/star_spi_bridge/
│   ├── spi_driver.hpp                      # SPI I/O + framing
│   ├── spi_message_converter.hpp           # ROS2 ↔ Protobuf
│   └── star_spi_driver_node.hpp            # Lifecycle node
├── src/
│   ├── spi_driver.cpp                      # Core SPI implementation
│   ├── spi_message_converter.cpp           # Message conversion
│   ├── star_spi_driver_node.cpp            # Node implementation
│   └── main.cpp                             # Entrypoint
├── test/
│   ├── test_spi_driver.cpp                 # Frame/CRC tests
│   └── test_spi_message_converter.cpp      # Kinematics tests
└── launch/
    └── star_spi_bridge.launch.py           # Launch configuration
```

### Reference Files (existing)

```
/Users/cesarmagana/Documents/GitHub/STAR/star-ros2/src/star_gateway_bridge/
├── CMakeLists.txt                          # Build pattern reference
├── package.xml                              # Dependency reference
├── src/star_gateway_bridge_node.cpp        # Node pattern reference
└── src/message_converter.cpp               # Conversion pattern reference

/Users/cesarmagana/Documents/GitHub/STAR/star-proto/gen/cpp/star/v1/
├── motor_control.pb.h                      # VelocityCommand protobuf
├── telemetry.pb.h                          # TelemetryData protobuf
└── common.pb.h                             # Common message types

/Users/cesarmagana/Documents/GitHub/STAR/docs/sections/
├── 01_nanopb_protocol.tex                  # SPI protocol spec
└── 03_hardware_pinout.tex                  # GPIO/SPI pinout
```

## Coding Standards

Follow `/Users/cesarmagana/Documents/GitHub/STAR/CLAUDE.md`:

### ROS2 C++ Style

- **Classes**: CamelCase (`StarSpiDriverNode`, `SpiMessageConverter`)
- **Methods**: snake_case (`twist_to_velocity_command()`, `spi_timer_callback()`)
- **Member variables**: snake*case with trailing underscore (`spi_driver*`, `wheel*base*`)
- **Constants**: ALL_CAPITALS or enums (`k_max_frame_size`, `k_spi_speed_hz`)
- **Line limit**: 120 characters (star-ros2/.clang-format)

### Key Principles

- **Inclusive terminology**: Controller/Peripheral (NOT master/slave), COPI/CIPO (NOT MOSI/MISO)
- **NO magic numbers**: All numeric literals must be named enums
- **Input validation**: Check for NaN/infinity in all ROS2 → Protobuf conversions
- **Safety first**: Send zero velocity on timeout, emergency stop, or communication failure
- **No dynamic allocation**: Use stack buffers and preallocated vectors in critical paths

## Verification Steps

### Unit Tests

```bash
cd /Users/cesarmagana/Documents/GitHub/STAR/star-ros2
colcon build --packages-select star_spi_bridge
colcon test --packages-select star_spi_bridge
colcon test-result --verbose
```

Expected test coverage:

- Frame encoding/decoding (CRC-32 validation)
- Kinematics (Twist → 4-wheel velocities)
- Odometry integration (encoder ticks → pose)
- NaN/infinity rejection

### Manual Testing (Hardware Required)

```bash
# Terminal 1: Launch node
ros2 launch star_spi_bridge star_spi_bridge.launch.py

# Terminal 2: Configure lifecycle
ros2 lifecycle set /star_spi_driver configure
ros2 lifecycle set /star_spi_driver activate

# Terminal 3: Monitor topics
ros2 topic hz /odom/unfiltered     # Should show ~100 Hz
ros2 topic echo /joint_states      # Verify encoder counts
ros2 topic echo /battery_state     # Check voltage/SOC

# Terminal 4: Send test command
ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.5}, angular: {z: 0.0}}" --once

# Verify motors move forward at 0.5 m/s
```

### Integration Tests

1. **Communication**: Verify SPI frames at 100 Hz without CRC errors
2. **Kinematics**: Publish `/cmd_vel` with pure rotation, verify left/right wheels opposite
3. **Odometry**: Drive 1 meter forward, verify `/odom/unfiltered` shows ~1m displacement
4. **Emergency Stop**: Trigger E-STOP, verify node stops sending commands
5. **Timeout**: Stop publishing `/cmd_vel`, verify zero velocity after 500ms

## Risks & Mitigations

| Risk                               | Impact                  | Mitigation                                                  |
| ---------------------------------- | ----------------------- | ----------------------------------------------------------- |
| `/dev/spidev0.0` permission denied | Node crashes on startup | Add user to spi group, document udev rule                   |
| CRC-32 endianness mismatch         | All frames rejected     | Unit test with known vector, verify with RX72N              |
| 100 Hz timer jitter                | E-STOP triggered        | Use `create_wall_timer()`, monitor with `ros2 topic hz`     |
| Protobuf serialization overhead    | Missed deadlines        | Profile with `rclcpp::Clock`, use arena allocator if needed |
| Encoder overflow                   | Incorrect odometry      | Use int64 (practically impossible to overflow)              |
| Odometry drift                     | Poor localization       | Document need for sensor fusion (EKF with IMU)              |

## Success Metrics

**Minimum Viable Product (MVP)**:

- [ ] Package builds without errors
- [ ] Unit tests pass (CRC-32, kinematics, odometry)
- [ ] Node completes lifecycle transitions
- [ ] 100 Hz timer runs consistently
- [ ] SPI frames exchange with RX72N
- [ ] Motors respond to `/cmd_vel` commands
- [ ] Odometry published at 100 Hz

**Production Ready**:

- [ ] Frame success rate >99% at 100 Hz
- [ ] Round-trip latency <10ms (99th percentile)
- [ ] Emergency stop detection works
- [ ] Graceful degradation on communication failure
- [ ] CPU usage <10% on RPi5
- [ ] No memory leaks (run with valgrind)
- [ ] Code reviewed for NASA Power of 10 compliance

## Branch & PR Strategy

1. Create feature branch: `git checkout -b feat/star-spi-bridge main`
2. Implement phases 1-3 (package + ROS2 integration)
3. Commit incrementally with descriptive messages
4. Open PR to `main` branch with description:

   ```
   ## Summary
   Implements star_spi_bridge ROS2 node for SPI communication with RX72N motor controller.

   ## Changes
   - Created star_spi_bridge package structure
   - Implemented SpiDriver with frame encoding/CRC-32 validation
   - Implemented SpiMessageConverter with differential drive kinematics
   - Implemented StarSpiDriverNode with lifecycle management
   - Added unit tests for frame protocol and message conversion

   ## Testing
   - Unit tests pass: `colcon test --packages-select star_spi_bridge`
   - Hardware integration verified on RPi5 + RX72N
   - 100 Hz communication rate maintained
   - Odometry published correctly

   Closes #137
   ```

5. Hardware testing (Phase 4) done after PR merged

---

## Next Steps

1. Create feature branch `feat/star-spi-bridge`
2. Start with Phase 1: Package structure and SPI driver core
3. Follow reference patterns from `star_gateway_bridge`
4. Run unit tests after each component
5. Hardware testing only after software validation passes
