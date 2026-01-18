# STAR ROS2 Implementation Roadmap

**Last Updated:** 2026-01-17
**Status:** Phase 3 Complete - Hardware Integration Ready

## Overview

This document tracks the implementation status and roadmap for the STAR platform's ROS2 Jazzy integration. The system bridges ROS2 with the RX72N motor controller via a Go gateway service, enabling autonomous navigation with Visual-LiDAR SLAM.

## Architecture Summary

```
ROS2 Ecosystem (RPi5)
├── star_spi_bridge (ROS2 ↔ RX72N via SPI)
├── star_gateway_bridge (ROS2 ↔ Go Gateway via gRPC)
├── star_safety_monitor (Platform integrity watchdog)
├── rtabmap_ros (Visual-LiDAR SLAM)
└── robot_localization (EKF sensor fusion)

Go Gateway Service (RPi5)
├── GatewayService ✅
├── MotorControlService ✅
├── TelemetryService ✅
├── BatteryManagementService ✅
├── ConfigurationService ✅
└── FirmwareUpdateService ❌ (stub only)

RX72N Motor Controller
└── ThreadX + nanopb + motor control firmware ✅
```

---

## Executive Summary (2026-01-17)

**Overall Completion: 75%** (18/24 major tasks complete)

| Component | Status | Completion |
|-----------|--------|------------|
| **Infrastructure** | ✅ Complete | 100% (8/8 merged PRs) |
| **Gateway Services** | ✅ Nearly Complete | 83% (5/6 services) |
| **Transport Layer** | ✅ Complete | 100% (SPI ready) |
| **ROS2 Nodes** | ⚠️ In Progress | 67% (2/3 nodes) |
| **Safety Systems** | ❌ Not Started | 0% (critical priority) |
| **SLAM Configuration** | ❌ Not Started | 0% (hardware-dependent) |
| **Hardware Testing** | ⚠️ Ready | 0% (pending hardware) |

**Major Accomplishments (Last 3 Days):**
- ✅ PR #184: TelemetryService + ConfigurationService (1072 lines)
- ✅ PR #191: BatteryManagementService (656 lines, 80% test coverage)
- ✅ PR #192: SPI Transport with periph.io (276 lines, production-ready)

**Critical Path to MVP:**
1. Implement `star_safety_monitor` node (Issue #139) - **START HERE**
2. Add E-Stop priority queue (Issue #176)
3. Hardware integration tests on RPi5 (Issue #180)

**Estimated Time to Production MVP:** ~1 week

---

## Current Status

### ✅ Completed & Merged

| Component | PR | Status | Lines | Merged |
|-----------|----|----|-------|--------|
| ROS2 Infrastructure | #141 | ✅ Merged | 6531 | 2026-01-06 |
| `star_gateway_bridge` | #146 | ✅ Merged | 1425 | 2026-01-07 |
| Gateway Bridge Tests | #168 | ✅ Merged | - | 2026-01-12 |
| `star_spi_bridge` | #172 | ✅ Merged | 889 | 2026-01-13 |
| MotorControlService | #174 | ✅ Merged | 318 | 2026-01-14 |
| TelemetryService + ConfigurationService | #184 | ✅ Merged | 1072 (258+814) | 2026-01-15 |
| BatteryManagementService | #191 | ✅ Merged | 656 | 2026-01-16 |
| SPI Transport Layer | #192 | ✅ Merged | 276 | 2026-01-17 |

**Infrastructure Complete:**
- ✅ Dockerfile (ROS2 Jazzy + gRPC + Protobuf)
- ✅ CI/CD workflow (`.github/workflows/ros2.yml`)
- ✅ Documentation (`docs/sections/10_ros2_integration.tex`)
- ⚠️ Devcontainer (removed in `c17e0b17f`, can be restored if needed)

**ROS2 Nodes Complete:**
- ✅ `star_spi_bridge` - Full SPI driver with CRC-32, lifecycle management, 100 Hz polling
- ✅ `star_gateway_bridge` - gRPC client, telemetry forwarding, teleop polling
- ❌ `star_safety_monitor` - Placeholder only (Issue #139)

**Gateway Services Complete (5/6 services, 83%):**
- ✅ `GatewayService` (322 lines) - ForwardTelemetry, GetTeleopCommand
- ✅ `MotorControlService` (318 lines) - SetVelocity, SetMotorPower, SetPIDGains, StreamEncoders, ControlStream, EmergencyStop
- ✅ `TelemetryService` (258 lines) - GetTelemetry, StreamTelemetry, GetSystemStatus
- ✅ `ConfigurationService` (814 lines) - Get/Set/Validate/Save configuration, PID tuning, factory reset
- ✅ `BatteryManagementService` (656 lines) - All 10 methods for BQ7850 battery management
- ❌ `FirmwareUpdateService` (50 lines stub) - OTA updates (deferred, low priority)

**Transport Layer Complete:**
- ✅ `SPI Transport` (276 lines) - periph.io driver, 10 MHz, full-duplex, deadline support

### 🧹 Cleanup Tasks

**Obsolete PRs and Issues:**
- [ ] **Close PR #144** - ROS2 Infrastructure (obsolete, merged in #141)
- [ ] **Close PR #145** - SPI Bridge Node (obsolete, replaced by #172)
- [ ] **Close Issue #138** - star_gateway_bridge (✅ complete in PR #146, #168)
- [ ] **Close Issue #147** - ROS2 Infrastructure (✅ complete in PR #141)
- [ ] **Close Issue #148** - star_spi_bridge (✅ complete in PR #172)
- [ ] **Close Issue #149** - Gateway Bridge (✅ complete in PR #146)

---

## Phase 1: Clean Up ✅ COMPLETE

**Goal:** Close obsolete PRs and verify main branch state

### Completed Tasks

- ✅ **Infrastructure merged** - Dockerfile, CI/CD, documentation (PR #141)
- ✅ **star_spi_bridge merged** - Complete SPI driver with lifecycle (PR #172)
- ✅ **star_gateway_bridge merged** - gRPC client with tests (PR #146, #168)

**Status:** Phase 1 complete. Cleanup PRs/issues pending (non-blocking)

---

## Phase 2: Complete Gateway gRPC Services ✅ COMPLETE

**Goal:** Implement remaining gRPC service stubs in Go gateway

**Final State:**
- ✅ `GatewayService` (2 methods: ForwardTelemetry, GetTeleopCommand) - PR #146
- ✅ `MotorControlService` (6 methods: SetVelocity, SetMotorPower, SetPIDGains, StreamEncoders, ControlStream, EmergencyStop) - PR #174
- ✅ `TelemetryService` (3 methods: GetTelemetry, StreamTelemetry, GetSystemStatus) - PR #184
- ✅ `ConfigurationService` (7 methods: Get/Set/Validate/Save config, PID tuning, factory reset) - PR #184
- ✅ `BatteryManagementService` (10 methods: All BQ7850 battery management operations) - PR #191
- ⚠️ `FirmwareUpdateService` (5 methods - stub only, deferred to future release)

### Task 2.1: Implement TelemetryService ✅ COMPLETE

**Issue:** #181 (part 1)
**Files:** `star-gateway/internal/service/telemetry.go` (258 lines)
**Priority:** 🔥 High
**Status:** ✅ Merged in PR #184 (2026-01-15)

**Methods to Implement:**
```go
// Snapshot of all sensor data (IMU, GPS, encoder ticks, motor state)
GetTelemetry(context.Context, *GetTelemetryRequest) (*GetTelemetryResponse, error)

// Continuous sensor updates at 1-100Hz (server streaming)
StreamTelemetry(*StreamTelemetryRequest, TelemetryService_StreamTelemetryServer) error

// Firmware version, uptime, connection states, error counts
GetSystemStatus(context.Context, *GetSystemStatusRequest) (*GetSystemStatusResponse, error)
```

**Integration Points:**
- Subscribe to dispatcher channels for telemetry messages from RX72N
- Aggregate data from multiple sources (IMU, encoders, motor state)
- Maintain in-memory cache for `GetTelemetry` snapshot
- Stream updates at configurable rate for `StreamTelemetry`

**Testing:**
- Unit tests with mock HARQ and dispatcher
- Integration test with real telemetry frames
- Performance test (100 Hz streaming with 10 concurrent clients)

**Implementation Details:**
- Thread-safe telemetry caching with background goroutine
- Configurable streaming rate (1-100 Hz)
- System status aggregation
- Context-aware with deadline handling

**Testing:** 629 lines of tests, 80%+ coverage

---

### Task 2.2: Implement BatteryManagementService ✅ COMPLETE

**Issue:** #181 (part 2)
**Files:** `star-gateway/internal/service/battery.go` (656 lines)
**Priority:** 🟡 Medium (hardware dependency)
**Status:** ✅ Merged in PR #191 (2026-01-16)

**Methods to Implement:**
```go
// Complete battery snapshot (cells, temps, SOC, current, voltage)
GetBatteryState(context.Context, *GetBatteryStateRequest) (*GetBatteryStateResponse, error)

// Continuous battery monitoring (server streaming)
StreamBatteryState(*StreamBatteryStateRequest, BatteryManagementService_StreamBatteryStateServer) error

// Read OV/UV/OC/OT protection limits from BQ78350
GetProtectionThresholds(context.Context, *GetProtectionThresholdsRequest) (*GetProtectionThresholdsResponse, error)

// Configure protection limits (write to BQ78350 registers)
SetProtectionThresholds(context.Context, *SetProtectionThresholdsRequest) (*SetProtectionThresholdsResponse, error)

// Per-cell passive balancing control
EnableCellBalancing(context.Context, *EnableCellBalancingRequest) (*EnableCellBalancingResponse, error)
DisableCellBalancing(context.Context, *DisableCellBalancingRequest) (*DisableCellBalancingResponse, error)

// Charge/discharge FET enable/disable (emergency cutoff)
ControlFets(context.Context, *ControlFetsRequest) (*ControlFetsResponse, error)

// Manufacturer, serial number, chemistry, firmware versions
GetDeviceInfo(context.Context, *GetDeviceInfoRequest) (*GetDeviceInfoResponse, error)
```

**Integration Points:**
- Communicate with RX72N BQ78350 driver via SPI + protobuf
- Forward SMBus commands to BQ78350 fuel gauge
- Handle safety-critical operations (FET control, protection thresholds)

**Hardware Dependencies:**
- BQ78350-R1A fuel gauge driver on RX72N (Issue #158)
- 3S lithium-ion battery pack

**Testing:**
- Mock BMS responses for unit tests
- Integration test with real BQ78350 hardware
- Safety validation (protection threshold limits, FET control)

**Implementation Details:**
- All 10 RPC methods for BQ7850 battery management
- Li-ion battery safety validation
- Cell balancing bitmask validation (16-cell support)
- Defensive copy pattern for thread safety
- FET control for emergency cutoff

**Testing:** 1058 lines of tests, 80.4% coverage, race-free

---

### Task 2.3: Implement ConfigurationService ✅ COMPLETE

**Issue:** #181 (part 3)
**Files:** `star-gateway/internal/service/configuration.go` (814 lines)
**Priority:** 🟡 Medium
**Status:** ✅ Merged in PR #184 (2026-01-15)

**Methods to Implement:**
```go
// Read current system configuration from RX72N
GetConfiguration(context.Context, *GetConfigurationRequest) (*GetConfigurationResponse, error)

// Apply configuration (optional NVS persist flag)
SetConfiguration(context.Context, *SetConfigurationRequest) (*SetConfigurationResponse, error)

// Restore factory defaults (reset NVS on RX72N)
ResetToDefaults(context.Context, *ResetToDefaultsRequest) (*ResetToDefaultsResponse, error)

// Validate configuration without applying (dry-run)
ValidateConfiguration(context.Context, *ValidateConfigurationRequest) (*ValidateConfigurationResponse, error)

// Persist in-memory config to NVS flash on RX72N
SaveConfiguration(context.Context, *SaveConfigurationRequest) (*SaveConfigurationResponse, error)

// Per-motor PID tuning (Kp, Ki, Kd, output limits)
GetMotorPidConfig(context.Context, *GetMotorPidConfigRequest) (*GetMotorPidConfigResponse, error)
SetMotorPidConfig(context.Context, *SetMotorPidConfigRequest) (*SetMotorPidConfigResponse, error)
```

**Integration Points:**
- Send configuration commands via HARQ to RX72N
- Handle NVS read/write operations on RX72N
- Validate configuration ranges (motor limits, PID gains, etc.)
- Runtime tuning support (no reboot required for PID gains)

**Testing:**
- Configuration validation logic (range checks, type checks)
- NVS persistence verification (read after write)
- PID gains runtime update (no restart required)

**Implementation Details:**
- All 7 configuration management methods
- Per-motor PID tuning with runtime updates
- NVS persistence for power-cycle retention
- Comprehensive validation (range checks, type checks)
- Dry-run validation without applying changes
- Factory reset capability

**Testing:** 1215 lines of tests, 80%+ coverage

---

### Task 2.4: Implement FirmwareUpdateService ⚠️ DEFERRED

**Issue:** #181 (part 4)
**Files:** `star-gateway/internal/service/firmware.go`
**Priority:** 🟢 Low (future enhancement)

**Methods to Implement:**
```go
// Start OTA update process on RX72N
InitiateFirmwareUpdate(context.Context, *InitiateFirmwareUpdateRequest) (*InitiateFirmwareUpdateResponse, error)

// Upload firmware in chunks (client streaming)
UploadFirmwareChunk(*UploadFirmwareChunkRequest, FirmwareUpdateService_UploadFirmwareChunkServer) error

// Progress monitoring (percentage, bytes transferred, ETA)
GetUpdateStatus(context.Context, *GetUpdateStatusRequest) (*GetUpdateStatusResponse, error)

// Validate checksum and finalize update (reboot into new firmware)
ConfirmFirmwareUpdate(context.Context, *ConfirmFirmwareUpdateRequest) (*ConfirmFirmwareUpdateResponse, error)

// Revert to previous firmware version (dual-bank flash)
RollbackFirmware(context.Context, *RollbackFirmwareRequest) (*RollbackFirmwareResponse, error)
```

**Integration Points:**
- Dual-bank flash support on RX72N (bank swap)
- Secure boot verification (signature validation)
- Streaming firmware chunks via HARQ
- Watchdog handling during update (no timeout)

**Hardware Dependencies:**
- Dual-bank flash layout on RX72N
- Bootloader with bank swap support

**Testing:**
- Mock firmware upload (chunked streaming)
- Checksum validation (CRC-32 or SHA-256)
- Rollback functionality (bank swap)

**Estimated Effort:** 4-5 days
**Dependencies:** RX72N bootloader implementation

---

## Phase 3: Hardware Integration ✅ TRANSPORT COMPLETE

**Goal:** Complete SPI transport and enable hardware testing

### Task 3.1: Implement SPI Transport Layer ✅ COMPLETE

**Issue:** #173 (Closed)
**Files:** `star-gateway/internal/transport/spi.go` (276 lines), `spi_test.go`
**Priority:** 🔥 Critical (blocks all hardware testing)
**Status:** ✅ Merged in PR #192 (2026-01-17)

**Implementation Summary:**
- ✅ periph.io library integration for RPi5 SPI
- ✅ Full-duplex SPI Transfer() with /dev/spidev0.0
- ✅ Thread-safe with RWMutex protection
- ✅ Configurable speed (10 MHz), mode (0), timeout (100ms)
- ✅ Deadline support with SetReadDeadline()
- ✅ Custom errors (ErrDeviceNotOpen, ErrTimeout, etc.)
- ✅ Full-duplex Transfer() + half-duplex Send()/Receive()
- ✅ Comprehensive unit tests (11 tests, 100% coverage on testable paths)
- ✅ Hardware integration tests (7 tests, skip gracefully without device)
- ✅ Documentation: `star-gateway/SPI_TRANSPORT_IMPLEMENTATION.md`

**Key Features:**
```go
// periph.io integration (not raw ioctl)
spiConfig := transport.DefaultConfig()
spiTransport := transport.NewSPITransport(spiConfig)
if err := spiTransport.Open(); err != nil {
    log.Fatalf("Failed to open SPI: %v", err)
}
defer spiTransport.Close()

// Full-duplex SPI transfer
rxData, err := spiTransport.Transfer(txData)
```

**Test Coverage:**
- Unit tests: 52.2% overall (100% on non-hardware paths)
- Open/Send/Receive/Transfer/Close: Partial (requires `/dev/spidev0.0`)
- Constructors/Config/Deadline: 100% coverage

**Hardware Validation Status:**
- ⏸️ Loopback test (COPI → CIPO shorted) - deferred until hardware
- ⏸️ RX72N communication test - deferred until hardware
- ⏸️ 100 Hz sustained throughput - deferred until hardware
- ⏸️ Latency <10ms per frame - deferred until hardware

**Dependencies:** periph.io v3.7.0
**Unblocks:** #176 (Priority Queue), #177 (Dispatcher Metrics), #180 (Hardware Tests)

---

### Task 3.2: Enhance Message Dispatcher

**Issue:** #177
**Files:** `star-gateway/internal/dispatcher/dispatcher.go`
**Priority:** 🟡 Medium

**Current State:**
- ✅ Basic routing implemented in PR #174
- ✅ Subscribe/Unsubscribe for message types
- ❌ Metrics for dropped frames
- ❌ Message type statistics

**Enhancements Needed:**
```go
// Add Prometheus metrics
type Metrics struct {
    MessagesReceived   prometheus.Counter
    MessagesDropped    prometheus.Counter
    MessagesByType     *prometheus.CounterVec
    DispatchLatency    prometheus.Histogram
}

// Add statistics tracking
type Statistics struct {
    TotalReceived      uint64
    TotalDropped       uint64
    DropsByType        map[MessageType]uint64
    SubscriberCounts   map[MessageType]int
}

// Add GetStatistics() method
func (d *Dispatcher) GetStatistics() Statistics
```

**Testing:**
- High-load test (1000 msgs/sec, verify no drops)
- Subscriber overflow test (slow consumer, verify backpressure)
- Metrics validation (Prometheus endpoint)

**Estimated Effort:** 1 day
**Dependencies:** PR #174 (✅ merged)

---

### Task 3.3: Type-Safe Message Wrapper

**Issue:** #178
**Files:** `star-proto/proto/star/v1/rx72n_message.proto`
**Priority:** 🟢 Low (code quality improvement)

**Current State:**
- Manual type checking in dispatcher with `frame.FrameType`
- No compile-time type safety

**Proposed Solution:**
```protobuf
message RX72NMessage {
  oneof payload {
    VelocityCommand velocity_command = 1;
    TelemetryData telemetry_data = 2;
    BatteryState battery_state = 3;
    ConfigurationData configuration_data = 4;
    // ... other message types
  }
}
```

**Benefits:**
- Eliminates manual type switching
- Compile-time type safety
- Cleaner dispatcher routing logic
- Self-documenting message types

**Migration:**
- Update protobuf schemas
- Regenerate Go code (`buf generate`)
- Update dispatcher to use `oneof` pattern
- Update all service implementations

**Testing:**
- Verify all message types work with new wrapper
- Performance regression test (no overhead from `oneof`)

**Estimated Effort:** 1 day
**Dependencies:** None (breaking change requires coordination)

---

### Task 3.4: Priority Queue for E-Stop

**Issue:** #176
**Files:** `star-gateway/internal/harq/harq.go`
**Priority:** 🔥 High (safety-critical)

**Current State:**
- HARQ queue is FIFO
- E-Stop commands queued behind normal traffic
- Potential latency during high load

**Required Changes:**
```go
// Add priority parameter to Send
func (h *ChaseCombining) Send(ctx context.Context, payload []byte, priority uint8) error {
    // priority = 0: Normal
    // priority = 1: High
    // priority = 255: Emergency (E-Stop)
}

// Implement priority queue
type priorityQueue struct {
    emergency []*frame.Frame  // Priority 255
    high      []*frame.Frame  // Priority 1
    normal    []*frame.Frame  // Priority 0
}

// Dequeue with priority
func (pq *priorityQueue) Dequeue() *frame.Frame {
    if len(pq.emergency) > 0 {
        return pq.emergency[0]
    }
    if len(pq.high) > 0 {
        return pq.high[0]
    }
    return pq.normal[0]
}
```

**Safety Requirements:**
- E-Stop commands must preempt all other traffic
- Max latency: <50ms (10 MHz SPI = ~1ms per frame)
- No dropped E-Stop frames (infinite retries)

**Testing:**
- Latency test (E-Stop during 100% queue utilization)
- Preemption test (E-Stop bypasses 1000 queued frames)
- Stress test (1000 E-Stops/sec, no drops)

**Estimated Effort:** 1 day
**Dependencies:** None

---

## Phase 4: Safety and Monitoring 🔥 HIGH PRIORITY

**Goal:** Ensure platform integrity and emergency handling

### Task 4.1: Implement star_safety_monitor Node

**Issue:** #139 🔥
**Files:** `star-ros2/src/star_safety_monitor/`
**Priority:** 🔥 High (safety-critical)

**Current State:**
- Package structure exists (placeholder from #141)
- No implementation

**Requirements:**
```cpp
// Monitors (all must be healthy):
- Battery voltage (threshold: 10.5V critical, 11.1V warning)
- Motor current (threshold: 15A fault per motor)
- Heartbeat from RX72N (timeout: 1000ms)
- ROS2 message freshness (cmd_vel timeout: 500ms)

// Actions on failure:
- Publish /emergency_stop (std_msgs/Bool)
- Send E-Stop command via star_spi_bridge service call
- Lifecycle state transition (ACTIVE → ERROR)
- Log detailed diagnostics

// Lifecycle states:
- UNCONFIGURED → INACTIVE → ACTIVE → ERROR
- ERROR → INACTIVE (recovery after manual reset)
```

**Topics:**
- **Subscribed:** `/battery_state`, `/odom`, `/joint_states`, `/cmd_vel`
- **Published:** `/emergency_stop`, `/diagnostics`

**Services:**
- **Provided:** `/reset_safety_monitor` (manual recovery)
- **Called:** `/star_spi_bridge/emergency_stop` (send E-Stop to RX72N)

**Implementation Pattern:**
```cpp
class StarSafetyMonitor : public rclcpp_lifecycle::LifecycleNode {
  // Timers for watchdog checks
  rclcpp::TimerBase::SharedPtr battery_watchdog_;
  rclcpp::TimerBase::SharedPtr heartbeat_watchdog_;
  rclcpp::TimerBase::SharedPtr cmd_vel_watchdog_;

  // Callback for battery monitoring
  void battery_state_callback(const sensor_msgs::msg::BatteryState::SharedPtr msg);

  // Watchdog timeout handlers
  void check_battery_voltage();
  void check_heartbeat_timeout();
  void check_cmd_vel_timeout();

  // Emergency stop trigger
  void trigger_emergency_stop(const std::string & reason);
};
```

**Testing:**
- Battery undervoltage test (simulate 10.4V)
- Heartbeat timeout test (stop publishing telemetry)
- cmd_vel timeout test (stop publishing commands)
- Recovery test (reset after fault)

**Estimated Effort:** 2-3 days
**Dependencies:** star_spi_bridge (✅ merged)

---

### Task 4.2: Telemetry Frame Drop Metric

**Issue:** #166
**Files:** `star-ros2/src/star_gateway_bridge/`
**Priority:** 🟡 Medium

**Current State:**
- No metrics for dropped frames
- No visibility into communication reliability

**Implementation:**
```cpp
// Add to StarGatewayBridgeNode
class StarGatewayBridgeNode : public rclcpp::Node {
private:
  // Metrics tracking
  uint64_t telemetry_frames_received_;
  uint64_t telemetry_frames_dropped_;
  uint64_t last_sequence_number_;

  // Diagnostic publisher
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diag_pub_;

  // Update metrics on each frame
  void update_frame_metrics(uint16_t sequence_number);

  // Publish diagnostics at 1 Hz
  void publish_diagnostics();
};
```

**Metrics:**
- Total frames received
- Total frames dropped (sequence number gaps)
- Drop rate (percentage)
- Latest sequence number

**Diagnostics:**
- ROS2 diagnostics_updater integration
- Prometheus metrics (if using ros2_metrics)
- Logging (throttled warnings for drops)

**Testing:**
- Simulate packet loss (drop every 10th frame)
- Verify metrics accuracy
- Check diagnostic messages

**Estimated Effort:** 1 day
**Dependencies:** None

---

### Task 4.3: PID Gains Service

**Issue:** #165
**Files:** `star-ros2/src/star_gateway_bridge/`, `star-proto/proto/star/v1/`
**Priority:** 🟡 Medium

**Current State:**
- Placeholder service in gateway_bridge (`std_srvs/SetBool`)
- No custom service definition

**Requirements:**
```protobuf
// Define custom service
message SetPIDGainsRequest {
  RequestHeader header = 1;
  MotorID motor_id = 2;  // FRONT_LEFT, FRONT_RIGHT, etc.
  PIDGains gains = 3;     // Kp, Ki, Kd, output_min, output_max
  bool persist_to_nvs = 4;
}

message SetPIDGainsResponse {
  ResponseHeader header = 1;
  bool success = 2;
  string error_message = 3;
}
```

**Integration:**
- Forward to ConfigurationService.SetMotorPidConfig via gRPC
- Gateway forwards to RX72N via HARQ
- RX72N updates PID controller in real-time
- Optional: Persist to NVS for power-cycle retention

**Testing:**
- Runtime tuning test (update gains, measure step response)
- NVS persistence test (power cycle, verify gains retained)
- Invalid gains test (negative Kp, out-of-range values)

**Estimated Effort:** 1 day
**Dependencies:** ConfigurationService (Task 2.3)

---

## Phase 5: SLAM and Navigation

**Goal:** Enable autonomous navigation with Visual-LiDAR SLAM

### Task 5.1: Configure RTAB-Map and EKF

**Issue:** #140
**Files:** `star-ros2/src/star_bringup/launch/`
**Priority:** 🟡 Medium (hardware-dependent)

**Current State:**
- Documentation exists (`docs/sections/10_ros2_integration.tex`)
- No launch files or configuration

**Implementation:**

#### 5.1.1: EKF Configuration

**File:** `star_bringup/config/ekf.yaml`

```yaml
ekf_filter_node:
  ros__parameters:
    frequency: 50.0  # 50 Hz

    # Fuse wheel odometry + IMU
    odom0: /odom/unfiltered
    odom0_config: [false, false, false,  # x, y, z position (don't use wheel odom position)
                   false, false, false,  # roll, pitch, yaw orientation
                   true, true, false,    # x_dot, y_dot, z_dot velocity
                   false, false, true,   # roll_dot, pitch_dot, yaw_dot
                   false, false, false]  # x_ddot, y_ddot, z_ddot acceleration

    imu0: /imu/data
    imu0_config: [false, false, false,  # position
                  true, true, true,     # orientation (use IMU for roll/pitch/yaw)
                  false, false, false,  # velocity
                  false, false, true,   # angular velocity (use yaw rate)
                  true, true, true]     # linear acceleration

    # Output frame
    world_frame: odom
    map_frame: map
    odom_frame: odom
    base_link_frame: base_link
```

#### 5.1.2: RTAB-Map Configuration

**File:** `star_bringup/config/rtabmap.yaml`

```yaml
rtabmap:
  ros__parameters:
    # Frame IDs
    frame_id: base_link
    odom_frame_id: odom
    map_frame_id: map

    # Inputs
    subscribe_depth: true
    subscribe_rgb: true
    subscribe_scan: true

    # SLAM parameters
    RGBD/ProximityBySpace: "true"
    RGBD/OptimizeFromGraphEnd: "false"

    # Loop closure detection
    Mem/IncrementalMemory: "true"
    Mem/InitWMWithAllNodes: "false"
    Kp/MaxFeatures: 400
    Kp/DetectorStrategy: 6  # GFTT/Good Features to Track
```

#### 5.1.3: Launch File

**File:** `star_bringup/launch/slam.launch.py`

```python
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        # EKF for local odometry
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            parameters=[os.path.join(get_package_share_directory('star_bringup'), 'config', 'ekf.yaml')],
        ),

        # RTAB-Map for global SLAM
        Node(
            package='rtabmap_ros',
            executable='rtabmap',
            name='rtabmap',
            parameters=[os.path.join(get_package_share_directory('star_bringup'), 'config', 'rtabmap.yaml')],
            remappings=[
                ('rgb/image', '/camera/rgb/image_raw'),
                ('depth/image', '/camera/depth/image_raw'),
                ('rgb/camera_info', '/camera/rgb/camera_info'),
                ('scan', '/scan'),
                ('odom', '/odom/filtered'),
            ],
        ),
    ])
```

#### 5.1.4: Static TF Tree

**File:** `star_bringup/launch/static_transforms.launch.py`

```python
# Static transforms for sensors
Node(
    package='tf2_ros',
    executable='static_transform_publisher',
    arguments=['0', '0', '0.05', '0', '0', '0', 'base_link', 'laser_frame'],
),
Node(
    package='tf2_ros',
    executable='static_transform_publisher',
    arguments=['0.10', '0', '0.15', '0', '0', '0', 'base_link', 'camera_link'],
),
```

**Hardware Dependencies:**
- RPLiDAR C1 (`/dev/ttyLidar` via udev)
- OAK-D RGB-D camera (USB)
- IMU (from OAK-D or dedicated sensor)

**Testing:**
- Map building test (drive around, verify loop closures)
- Localization test (relocalize in existing map)
- TF tree validation (`ros2 run tf2_tools view_frames`)

**Estimated Effort:** 3-4 days
**Dependencies:** Hardware sensors, star_spi_bridge (✅ merged)

---

## Phase 6: Integration Testing

**Goal:** Validate end-to-end system with hardware

### Task 6.1: Hardware Integration Test Suite

**Issue:** #180
**Files:** `star-gateway/internal/transport/spi_test.go`, `star-ros2/src/star_spi_bridge/test/`
**Priority:** 🔥 High

**Test Scenarios:**

#### 6.1.1: SPI Loopback Test
```bash
# Physical loopback (COPI → CIPO shorted)
go test -v ./internal/transport -run TestSPILoopback
```

**Validates:**
- Full-duplex SPI transfer
- CRC-32 validation
- 10 MHz clock operation

#### 6.1.2: RX72N Communication Test
```bash
# Send VelocityCommand, receive TelemetryData
go test -v ./internal/integration -run TestRX72NRoundTrip
```

**Validates:**
- Protobuf serialization/deserialization
- Frame encoding/decoding
- RX72N firmware communication

#### 6.1.3: HARQ Retransmission Test
```bash
# Inject packet loss, verify retransmissions
go test -v ./internal/harq -run TestRetransmission
```

**Validates:**
- HARQ retry logic (max 3 attempts)
- Chase combining (soft bit accumulation)
- Timeout handling (10ms default)

#### 6.1.4: FEC Error Correction Test
```bash
# Inject bit errors, verify correction
go test -v ./internal/fec -run TestViterbiDecoding
```

**Validates:**
- Convolutional encoding (rate 1/2, K=7)
- Viterbi decoding (soft decision)
- Error correction capability (up to 3 bit errors per frame)

#### 6.1.5: End-to-End Latency Test
```bash
# ROS2 cmd_vel → SPI → RX72N → telemetry → ROS2 odom
ros2 run star_spi_bridge latency_test_node
```

**Validates:**
- Total latency (target: <10ms at 100 Hz)
- Jitter (target: <2ms)
- Throughput (target: 100 frames/sec sustained)

#### 6.1.6: Emergency Stop Response Time
```bash
# Trigger E-Stop, measure response time
ros2 topic pub /emergency_stop std_msgs/Bool "{data: true}" --once
```

**Validates:**
- E-Stop latency (target: <50ms)
- Priority queue preemption
- Motor halt confirmation

**Estimated Effort:** 2-3 days
**Dependencies:** SPI transport (#173), hardware setup

---

### Task 6.2: Concurrency and Leak Tests

**Issue:** #179
**Files:** `star-gateway/internal/service/motor_control_test.go`
**Priority:** 🟡 Medium

**Test Scenarios:**

#### 6.2.1: Goroutine Leak Detection
```bash
go test -v ./internal/service -run TestMotorControlGoRoutineLeak
```

**Validates:**
- All goroutines cleaned up after service shutdown
- No zombie listeners on dispatcher channels

#### 6.2.2: Race Condition Detection
```bash
go test -race ./...
```

**Validates:**
- No data races in concurrent code
- Proper mutex usage

#### 6.2.3: Memory Leak Detection
```bash
go test -memprofile=mem.prof ./internal/service
go tool pprof mem.prof
```

**Validates:**
- No memory growth over time
- Proper cleanup of subscribers

**Estimated Effort:** 1-2 days
**Dependencies:** None

---

## Priority Matrix

| Phase | Task | Priority | Effort | Blocks | Status |
|-------|------|----------|--------|--------|--------|
| 1 | Close PR #145/#144 | 🟢 Low | 10 min | - | ⚠️ Pending |
| 3.1 | **SPI Transport** | 🔥 Critical | 1-2 days | All HW testing | ✅ **DONE** |
| 2.1 | TelemetryService | 🔥 High | 2-3 days | - | ✅ **DONE** |
| 2.2 | BatteryManagementService | 🟡 Medium | 3-4 days | Issue #158 | ✅ **DONE** |
| 2.3 | ConfigurationService | 🟡 Medium | 2-3 days | SPI transport | ✅ **DONE** |
| 4.1 | **Safety Monitor** | 🔥 High | 2-3 days | - | ❌ **NEXT** |
| 3.4 | E-Stop Priority Queue | 🔥 High | 1 day | - | ❌ TODO |
| 3.2 | Dispatcher Metrics | 🟡 Medium | 1 day | - | ❌ TODO |
| 4.2 | Frame Drop Metrics | 🟡 Medium | 1 day | - | ❌ TODO |
| 4.3 | PID Gains Service | 🟡 Medium | 1 day | Config service | ⚠️ Ready |
| 5.1 | RTAB-Map + EKF | 🟡 Medium | 3-4 days | Hardware | ❌ TODO |
| 6.1 | **HW Integration Tests** | 🔥 High | 2-3 days | Hardware | ⚠️ Ready |
| 3.3 | Type-Safe Wrapper | 🟢 Low | 1 day | - | ❌ TODO |
| 2.4 | FirmwareUpdateService | 🟢 Low | 4-5 days | Bootloader | ⚠️ Deferred |
| 5.2 | Multi-Robot Comms | 🟢 Low | 1 day | - | ❌ TODO |
| 6.2 | Concurrency Tests | 🟡 Medium | 1-2 days | - | ❌ TODO |

---

## Dependency Graph

```
Phase 1: Cleanup ──────────────────────────────────────────────── ⚠️ Pending

Phase 2: Gateway Services ─────────────────────────────────────── ✅ COMPLETE
├── TelemetryService ────────────────────────────────────────── ✅ DONE
├── BatteryManagementService ────────────────────────────────── ✅ DONE
├── ConfigurationService ────────────────────────────────────── ✅ DONE
└── FirmwareUpdateService ───────────────────────────────────── ⚠️ Deferred

Phase 3: Hardware Integration ────────────────────────────────── ✅ TRANSPORT DONE
├── SPI Transport ───────────────────────────────────────────── ✅ DONE
├── Dispatcher Metrics ──────────────────────────────────────── ❌ TODO
├── Type-Safe Wrapper ───────────────────────────────────────── ❌ TODO
└── E-Stop Priority Queue ───────────────────────────────────── ❌ TODO

Phase 4: Safety & Monitoring ─────────────────────────────────── ❌ NEXT PHASE
├── Safety Monitor Node ─────────────────────────────────────── ❌ CRITICAL
├── Frame Drop Metrics ──────────────────────────────────────── ❌ TODO
└── PID Gains Service ───────────────────────────────────────── ⚠️ Ready

Phase 5: SLAM Configuration ──────────────────────────────────── ❌ Hardware Dependent
└── RTAB-Map + EKF ──────────────────────────────────────────── ❌ TODO

Phase 6: Integration Testing ─────────────────────────────────── ⚠️ Ready when hardware available
├── Hardware Tests ──────────────────────────────────────────── ⚠️ Ready
└── Concurrency Tests ───────────────────────────────────────── ❌ TODO
```

**Critical Path:** Phase 4.1 (Safety Monitor) → Phase 6.1 (Hardware Tests w/ RPi5+RX72N)

---

## Next Actions (Updated 2026-01-17)

### Immediate Priority (Week of 2026-01-20)

1. **Implement star_safety_monitor Node** (Issue #139) - 2-3 days 🔥 **START HERE**
   - Battery voltage monitoring (10.5V critical, 11.1V warning)
   - Motor current monitoring (15A fault threshold)
   - Heartbeat watchdog (1000ms timeout)
   - Emergency stop publishing to `/emergency_stop`
   - **This is the critical missing piece for production safety**

2. **Add E-Stop Priority Queue** (Issue #176) - 1 day
   - Priority-based HARQ queue implementation
   - Emergency commands bypass normal traffic
   - <50ms latency guarantee for safety-critical commands

3. **Frame Drop Metrics** (Issue #166) - 1 day
   - Track dropped frames in `star_gateway_bridge`
   - Publish diagnostics for monitoring

### Hardware-Dependent Tasks (When RPi5 + RX72N Available)

4. **Hardware Integration Tests** (Issue #180) - 2-3 days
   - SPI loopback test (COPI → CIPO shorted)
   - RX72N round-trip communication
   - End-to-end latency validation (<10ms target)
   - Emergency stop response time (<50ms target)
   - 100 Hz sustained throughput test

5. **SLAM Configuration** (Issue #140) - 3-4 days
   - Configure RTAB-Map for Visual-LiDAR SLAM
   - EKF sensor fusion (wheel odometry + IMU)
   - Launch files and static TF transforms
   - **Requires: RPi5 + RPLiDAR C1 + OAK-D camera**

### Enhancements (Lower Priority)

6. **Dispatcher Metrics** (Issue #177) - 1 day
7. **Type-Safe Message Wrapper** (Issue #178) - 1 day
8. **Concurrency & Leak Tests** (Issue #179) - 1-2 days
9. **PID Gains Service** (Issue #165) - 1 day

---

**Status Summary:**
- ✅ **Phase 1-2 Complete:** Infrastructure + All critical services (5/6 services)
- ✅ **Phase 3 Transport Complete:** SPI layer ready for hardware
- ❌ **Phase 4 Next:** Safety monitoring (critical for production)
- ⏸️ **Phase 5-6:** Blocked on hardware availability

**Total Estimated Effort to Production MVP:** ~1 week (assuming hardware available)

---

## Resources

- **Documentation:** `docs/sections/10_ros2_integration.tex`
- **Style Guide:** `CLAUDE.md` (project root)
- **ROS2 README:** `star-ros2/README.md`
- **Gateway CLAUDE.md:** `star-gateway/CLAUDE.md`
- **Issue Tracker:** https://github.com/Locked-Inc/STAR/issues

---

## Notes

- **Hardware Requirements:** Most tasks require Raspberry Pi 5 + RX72N hardware for testing
- **Development Environment:** Use Docker container (`Dockerfile` in root) or devcontainer
- **Testing:** All implementations must include unit tests (80% coverage minimum)
- **Code Quality:** Run `./scripts/format-ros2.sh` before committing ROS2 C++ code
- **Commit Convention:** Use conventional commits (feat/fix/docs/test)

**Questions?** See `CLAUDE.md` for detailed coding standards and architecture guidance.