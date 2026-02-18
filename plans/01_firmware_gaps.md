# Firmware Gaps: RX72N (e2-studio-star-rx72n-firmware)

## Status Summary

The RX72N firmware has excellent core implementation — all 9 ThreadX tasks are complete, all hardware drivers work, and the communication stack (HARQ/FEC/CRC-32) is production-ready. However, the **integration layer** between the communication task and the application tasks is missing.

| Gap | Severity | Effort | Blocks |
|-----|----------|--------|--------|
| WireMessage dispatcher | CRITICAL | 4-6 hrs | All RPi5↔RX72N commands |
| OTA firmware update handler | HIGH | 2-3 days | Firmware updates in production |
| NVS configuration persistence | HIGH | 4-6 hrs | PID tuning, safety params |
| BMS telemetry integration | MEDIUM | 4-6 hrs | Battery status in UI |
| RPLiDAR C1 integration | LOW | 2-3 days | SLAM/dense mapping |
| Host I2C (RIIC0) application | LOW | 1-2 days | Future sensors |

---

## Gap 1: WireMessage Dispatcher (CRITICAL)

### Problem
The `comm_task.c` receives frames from the SPI/USB transport and decodes them using nanopb, but there is **no dispatch logic** to route the decoded `star_v1_WireMessage` to the correct handler (motor control, config, etc.).

All commands arriving from the RPi5 Gateway are effectively silently dropped after decoding.

### Location
- `e2-studio-star-rx72n-firmware/src/tasks/comm_task.c` (needs new function)
- `e2-studio-star-rx72n-firmware/libs/rx_comm_manager/src/rx_comm_manager.c` (partially has this)

### What Needs to Be Built

```c
/**
 * @brief Route an incoming WireMessage to the correct application handler.
 *
 * @details
 * Called by comm_task after nanopb decoding. Uses the protobuf oneof
 * tag to determine message type and dispatches to registered handlers.
 * All handlers run in calling task context (comm_task).
 *
 * @param[in] msg Decoded WireMessage (must not be NULL)
 *
 * @return rx_err_t
 * @retval k_rx_ok Message routed successfully
 * @retval k_rx_err_null_ptr msg is NULL
 * @retval k_rx_err_not_supported Unknown message type (logged, not fatal)
 *
 * @pre msg must be a valid decoded WireMessage
 * @post Appropriate handler invoked with message payload
 *
 * @note Not thread-safe for registrations. Handlers must be registered
 *       before comm_task starts.
 */
static rx_err_t internal_dispatch_wire_message(
    const star_v1_WireMessage* msg);
```

### Implementation

```c
static rx_err_t internal_dispatch_wire_message(
    const star_v1_WireMessage* msg)
{
    RX_CHECK_NULL_PTR(msg, k_rx_err_null_ptr);

    switch (msg->which_payload) {
    case star_v1_WireMessage_velocity_command_tag:
        return motor_control_handle_velocity_command(
            &msg->payload.velocity_command);

    case star_v1_WireMessage_emergency_stop_command_tag:
        return motor_control_handle_emergency_stop(
            &msg->payload.emergency_stop_command);

    case star_v1_WireMessage_motor_power_command_tag:
        return motor_control_handle_power_command(
            &msg->payload.motor_power_command);

    case star_v1_WireMessage_pid_config_tag:
        return config_handle_pid_update(&msg->payload.pid_config);

    case star_v1_WireMessage_retransmit_config_tag:
        return config_handle_retransmit_update(
            &msg->payload.retransmit_config);

    case star_v1_WireMessage_system_configuration_tag:
        /* System config: full config blob, save to NVS */
        return nvs_handle_system_config(
            &msg->payload.system_configuration);

    default:
        rx_log_warn(k_tag_comm, "Unknown WireMessage payload type: %d",
                    (int)msg->which_payload);
        return k_rx_ok;  /* Not fatal - forward compatibility */
    }
}
```

### Handler Signatures Needed

Each handler must be declared in the appropriate task's header:

```c
/* motor_control_task.h */
rx_err_t motor_control_handle_velocity_command(
    const star_v1_VelocityCommand* cmd);
rx_err_t motor_control_handle_emergency_stop(
    const star_v1_EmergencyStopCommand* cmd);
rx_err_t motor_control_handle_power_command(
    const star_v1_MotorPowerCommand* cmd);

/* config_task.h (new file, or add to shared/config.h) */
rx_err_t config_handle_pid_update(const star_v1_PidConfig* config);
rx_err_t config_handle_retransmit_update(
    const star_v1_RetransmitConfig* config);
rx_err_t nvs_handle_system_config(
    const star_v1_SystemConfiguration* config);
```

### Testing

Write Unity test: `tests/test_comm_task_dispatch.c`
- Test: velocity command routes to motor handler
- Test: E-stop command routes to motor handler with highest priority
- Test: unknown type logs warning but doesn't crash
- Test: NULL msg returns error
- Test: PID config routes to config handler

---

## Gap 2: OTA Firmware Update Handler (HIGH)

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

## Gap 3: NVS Configuration Persistence (HIGH)

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

## Gap 4: BMS Telemetry Integration (MEDIUM)

### Problem
The `bms_monitor_task.c` uses `rx_bq4050` driver to detect battery faults (overcurrent, undervoltage) and trigger E-Stop, but the BMS data is not serialized into the TelemetryData protobuf and sent to the RPi5.

### What's Missing

In `telemetry_task.c`, the `TelemetryData` builder needs to populate:

```c
/* These fields are in telemetry.proto but not populated in firmware */
msg.has_battery_state = true;
msg.battery_state.voltage_mv    = bms_get_voltage_mv();
msg.battery_state.current_ma    = bms_get_current_ma();
msg.battery_state.state_of_charge = bms_get_soc_percent();
msg.battery_state.temperature_deci_celsius = bms_get_temperature();
msg.battery_state.is_charging   = bms_is_charging();
msg.battery_state.fault_flags   = bms_get_fault_flags();
```

### BMS Public API Needed

`bms_monitor_task.h` needs thread-safe getters:

```c
/**
 * @brief Get latest battery voltage in millivolts.
 * @note Thread-safe (reads from shared data under mutex).
 */
uint32_t bms_get_voltage_mv(void);
uint32_t bms_get_current_ma(void);
uint8_t  bms_get_soc_percent(void);
int16_t  bms_get_temperature_deci_celsius(void);
bool     bms_is_charging(void);
uint32_t bms_get_fault_flags(void);
```

### Effort: 4-6 hours

---

## Gap 5: RPLiDAR C1 Integration (LOW)

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

## Gap 6: Host I2C (RIIC0) Application (LOW)

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
├── WireMessage Dispatch  ❌ NOT IMPLEMENTED (critical gap)
├── OTA Firmware Update   ❌ NOT IMPLEMENTED
├── NVS Configuration     ❌ NOT IMPLEMENTED
├── BMS Data Streaming    ⚠️ Partial (faults only, no SOC/voltage)
├── Temperature Sensing   ✅ Complete (DS18B20 @ 1 Hz)
├── Obstacle Detection    ✅ Complete (HC-SR04 IRQ @ 50 Hz)
├── Watchdog Monitor      ✅ Complete (IWDT @ 100 Hz)
├── LED Status            ✅ Complete (LED task @ 20 Hz)
├── RPLiDAR C1            ❌ NOT IMPLEMENTED
└── Host I2C Sensors      ❌ NOT ALLOCATED (reserved for future)
```
