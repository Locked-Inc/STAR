# Firmware Gaps: RX72N (e2-studio-star-rx72n-firmware)

## Status Summary

The RX72N firmware is production-complete — all 9 ThreadX tasks are fully implemented, all hardware drivers work, and the communication stack (HARQ/FEC/CRC-32) is production-ready. The command dispatcher and BMS telemetry were both implemented and are confirmed complete as of the latest main branch.

**Verified complete (previously thought to be missing):**
- ✅ WireMessage dispatcher — `internal_handle_command_frame()` in `comm_task.c` (2,060 lines) handles SetVelocity, E-Stop, SetPIDGains, SetRetransmitConfig
- ✅ BMS telemetry streaming — `telemetry_task.c` (1,870 lines) populates `battery_voltage_v` and `battery_soc_percent` from `shared_data_get_bms()`

| Gap | Severity | Effort | Blocks |
|-----|----------|--------|--------|
| OTA firmware update handler | HIGH | 2-3 days | Firmware updates in production |
| NVS configuration persistence | HIGH | 4-6 hrs | PID tuning persists across reboot |
| RPLiDAR C1 integration | LOW | 2-3 days | SLAM/dense mapping |
| Host I2C (RIIC0) application | LOW | 1-2 days | Future sensors |

---

## ✅ CONFIRMED COMPLETE: Command Dispatcher

`comm_task.c` line 1959 — `internal_handle_command_frame()` handles 4 message types:

| Message Type | Handler | Action |
|-------------|---------|--------|
| `SetVelocityRequest` | `shared_data_set_motor_command()` | Updates 4-motor velocity targets |
| `EmergencyStopRequest` | `shared_data_trigger_estop()` | Triggers POEG hardware E-stop |
| `SetPIDGainsRequest` | `shared_data_set_pid_gains()` | Updates Kp/Ki/Kd for all motors |
| `SetRetransmitConfigRequest` | `rx_comm_manager_set_auto_retransmit()` | Updates HARQ retry config |

Note: `MotorPowerCommand` and `SystemConfiguration` are not yet handled (would log warning and return). The gateway currently only sends the 4 types above.

---

## Gap 1: OTA Firmware Update Handler (HIGH)

### Problem
The Gateway has 10 RPC methods for OTA firmware update, all returning `codes.Unimplemented`. The firmware has no OTA handler at all.

The RX72N has **dual-bank flash** (documented in the linker script PR #305), which enables OTA:
- Bank 0: Current running firmware (16 MB)
- Bank 1: OTA staging area

### State Machine Required

```c
/**
 * @enum ota_state_t
 * @brief OTA update state machine states
 */
typedef enum : uint8_t {
    k_ota_state_idle        = 0, /**< No update in progress */
    k_ota_state_starting    = 1, /**< BeginUpdate received, initializing */
    k_ota_state_receiving   = 2, /**< WriteChunk frames arriving */
    k_ota_state_validating  = 3, /**< Checking CRC-32 + size */
    k_ota_state_ready       = 4, /**< Validated, ready to reboot */
    k_ota_state_error       = 5, /**< Error occurred */
} ota_state_t;

/**
 * @struct ota_context_t
 * @brief OTA session state
 */
typedef struct {
    ota_state_t state;        /**< Current FSM state */
    uint32_t    total_size;   /**< Expected firmware size in bytes */
    uint32_t    bytes_written;/**< Bytes written so far */
    uint32_t    expected_crc; /**< Expected final CRC-32 */
    uint32_t    running_crc;  /**< Running CRC-32 accumulator */
    uint32_t    session_id;   /**< From BeginUpdateRequest */
} ota_context_t;
```

### Files to Create

```
e2-studio-star-rx72n-firmware/
└── libs/
    └── rx_ota/
        ├── inc/
        │   └── rx_ota.h      (state machine + API)
        └── src/
            └── rx_ota.c      (implementation)

e2-studio-star-rx72n-firmware/
└── tests/
    └── test_rx_ota.c         (Unity tests)
```

### Gateway Side (firmware.go)

Once firmware OTA is implemented, `star-gateway/internal/service/firmware.go` must be updated:

- `BeginUpdate`: Send `BeginUpdateRequest` WireMessage, wait for `BeginUpdateResponse`
- `WriteChunk`: Buffer chunks, send via HARQ (reliable delivery required)
- `StreamChunks`: Client-streaming version of WriteChunk
- `FinalizeUpdate`: Send finalize command, wait for CRC validation response
- `AbortUpdate`: Send abort, reset OTA state machine
- `GetUpdateProgress`: Query firmware for progress percentage
- `Reboot`: Trigger firmware reboot into new image
- `Rollback`: Boot back to previous image
- `MarkValid`: Confirm new firmware is stable after reboot

**Note:** Wire.proto needs a `FirmwareChunk` message type in the WireMessage oneof.

---

## Gap 2: NVS Configuration Persistence (HIGH)

### Problem
The firmware receives `PidConfig` and `SystemConfiguration` messages but has nowhere to save them to non-volatile storage (NVS). When the board reboots, all tuned parameters are lost.

### What RX72N Has for NVS
The RX72N has **Data Flash** (64 KB) separate from code flash. This is ideal for NVS configuration.

### Files to Create

```
e2-studio-star-rx72n-firmware/
└── libs/
    └── rx_nvs/
        ├── inc/
        │   └── rx_nvs.h
        └── src/
            └── rx_nvs.c
```

### API Design

```c
/**
 * @brief Write a named configuration blob to Data Flash.
 *
 * @param[in] key     ASCII key name (max 32 chars)
 * @param[in] data    Pointer to data buffer
 * @param[in] len     Length in bytes (max 4096)
 *
 * @return rx_err_t
 * @retval k_rx_ok Saved successfully
 * @retval k_rx_err_invalid_arg key or data NULL, len 0 or too large
 * @retval k_rx_err_io Data Flash write failure
 */
rx_err_t rx_nvs_write(const char* key, const void* data, uint16_t len);

/**
 * @brief Read a named configuration blob from Data Flash.
 */
rx_err_t rx_nvs_read(const char* key, void* data, uint16_t* len);

/**
 * @brief Delete a key from Data Flash.
 */
rx_err_t rx_nvs_delete(const char* key);

/**
 * @brief Erase all Data Flash (factory reset).
 */
rx_err_t rx_nvs_erase_all(void);
```

### Integration Points

- `config_handle_pid_update()`: After updating RAM copy, call `rx_nvs_write("pid_config", ...)`
- `main.c`: On startup, call `rx_nvs_read("pid_config", ...)` to restore saved PID gains

---

## ✅ CONFIRMED COMPLETE: BMS Telemetry Streaming

`telemetry_task.c` line 1833 populates BMS data into `TelemetryData`:
- `battery_voltage_v = bms_state.voltage_mv / 1000.0` (millivolts → volts)
- `battery_soc_percent = bms_state.soc_percent`
- `battery_percent = bms_state.soc_percent` (legacy duplicate field)

Data flows: `bms_monitor_task` → `shared_data_get_bms()` → `telemetry_task` → SPI wire → Gateway

Note: `current_ma`, `is_charging`, and `fault_flags` are not in the current `TelemetryData` proto for the telemetry path (only `battery_voltage_v` and `battery_soc_percent`). The BatteryManagementService gRPC in the gateway has the fuller data model.

---

## Gap 3: RPLiDAR C1 Integration (LOW)

### Problem
RPLiDAR C1 is listed as hardware in the project overview but has:
- No firmware driver library (`libs/rx_rplidar/` does not exist)
- No task to poll it
- No pins allocated in pinout (must determine interface)
- No protobuf messages for lidar scan data

### Interface Investigation Required

The RPLiDAR C1 communicates via UART at 115200 baud. The RX72N has multiple UART channels:
- SCI9 is currently used for debug console
- SCI0-SCI6 are available

### Files to Create

```
libs/rx_rplidar/
├── inc/
│   └── rx_rplidar.h       (driver API)
└── src/
    └── rx_rplidar.c       (UART-based protocol implementation)

src/tasks/
└── lidar_scan_task.c      (10 Hz scan polling)
```

### Proto Changes Required

Need to add lidar scan data to `telemetry.proto` or create `lidar.proto`:

```protobuf
message LidarScan {
    uint64 timestamp_us = 1;    // Microseconds since boot
    repeated LidarPoint points = 2;  // Scan points

    message LidarPoint {
        float angle_deg = 1;     // 0-360 degrees
        float distance_mm = 2;   // 0 = invalid
        uint32 quality = 3;      // Signal quality 0-255
    }
}
```

### Estimated Effort: 2-3 days

This is a significant feature. Consider deferring until SLAM stack is configured in ROS2.

---

## Gap 4: Host I2C (RIIC0) Application (LOW)

### Problem
Pins P12 (SCL) and P13 (SDA) are allocated for "Host I2C" in the hardware pinout, and RIIC0 peripheral is initialized, but no application task uses it.

### Likely Use Case
Future sensors attached to the RPi5 I2C bus:
- IMU (e.g., MPU-6050, ICM-42688)
- Barometric pressure sensor
- Humidity sensor

### Recommendation
**No action required now.** Document that RIIC0 is reserved for future expansion. If an IMU is added to the hardware design, create `libs/rx_imu/` at that time.

---

## Complete Firmware Integration Status

```
firmware/
├── Motor Control         ✅ Complete (PID @ 100Hz, 4 motors)
├── Communication Stack   ✅ Complete (SPI/USB, HARQ, FEC, CRC-32)
├── WireMessage Dispatch  ✅ Complete (SetVelocity, E-Stop, PID, RetransmitConfig)
├── BMS Data Streaming    ✅ Complete (voltage_v, soc_percent in TelemetryData)
├── Temperature Sensing   ✅ Complete (DS18B20 @ 1 Hz)
├── Obstacle Detection    ✅ Complete (HC-SR04 IRQ @ 50 Hz)
├── Watchdog Monitor      ✅ Complete (IWDT @ 100 Hz)
├── LED Status            ✅ Complete (LED task @ 20 Hz)
├── OTA Firmware Update   ❌ NOT IMPLEMENTED
├── NVS Configuration     ❌ NOT IMPLEMENTED (PID gains lost on reboot)
├── RPLiDAR C1            ❌ NOT IMPLEMENTED
└── Host I2C Sensors      ❌ NOT ALLOCATED (reserved for future)
```
