# Hardware Integration Gaps

## Status Summary

The RX72N firmware has excellent driver coverage for all primary hardware. The main hardware gaps are sensors that enhance capability (RPLiDAR, IMU) rather than blocking core operation.

| Hardware | Status | Firmware Driver | Task | Priority |
|----------|--------|----------------|------|----------|
| 4x DRV8243S Motor Drivers | ✅ Complete | rx_drv8243 | motor_control_task | — |
| 4x Quadrature Encoders (MTU/TPU) | ✅ Complete | rx_encoder | motor_control_task | — |
| 4x HC-SR04 Sonar (IRQ) | ✅ Complete | rx_hcsr04 | obstacle_detect_task | — |
| DS18B20 Temperature Sensor | ✅ Complete | rx_ds18b20 | temp_sensor_task | — |
| BQ4050 BMS Fuel Gauge | ⚠️ Partial | rx_bq4050 | bms_monitor_task | HIGH |
| 6x Status LEDs | ✅ Complete | GPIO | led_status_task | — |
| Host SPI (RSPI2 @ 10 Mbps) | ✅ Complete | rx_spi_comm | comm_task | — |
| USB CDC (3 virtual COM) | ✅ Complete | rx_usb + rx_usb_comm | comm_task | — |
| Debug UART (SCI9) | ✅ Complete | rx_core | All tasks | — |
| E-Stop (GTETRG/POEG) | ✅ Complete | rx_poeg | motor_control_task | — |
| RPLiDAR C1 | ❌ Missing | None | None | MEDIUM |
| Host I2C (RIIC0) | ⚠️ Reserved | rx_hal (low-level) | None | LOW |
| Motor Current Sensing (ADC) | ✅ Complete | rx_hal ADC | motor_control_task | — |
| IWDT Watchdog | ✅ Complete | rx_iwdt | watchdog_monitor_task | — |
| BMS ALERT IRQ (IRQ13) | ✅ Complete | rx_bms_alert | bms_monitor_task | — |

---

## Hardware Integration Scorecard

```
Motor Control System:     100% ████████████████████
Communication Stack:      100% ████████████████████
Obstacle Detection:       100% ████████████████████
Temperature Monitoring:   100% ████████████████████
Safety Systems:           100% ████████████████████
BMS (Battery):             67% █████████████░░░░░░░ (faults only)
SLAM Sensors:               0% ░░░░░░░░░░░░░░░░░░░░ (RPLiDAR missing)
Host Sensors (I2C):         0% ░░░░░░░░░░░░░░░░░░░░ (no app yet)
```

---

## Gap 1: BMS Telemetry Integration (HIGH)

### Current State

The BQ4050 BMS driver (`rx_bq4050`) and `bms_monitor_task` provide:
- ✅ Overcurrent/undervoltage/overtemperature fault detection
- ✅ BMS ALERT IRQ triggers motor E-Stop on critical fault
- ❌ Battery state (SOC, cell voltage, current) NOT sent to RPi5
- ❌ Battery state NOT included in telemetry protobuf

### What's Missing

In `telemetry_task.c`, the telemetry builder doesn't include battery data:

```c
/* Currently missing from telemetry message build: */
// src/tasks/telemetry_task.c → build_telemetry_message()

// Need to add:
if (bms_get_data(&bms_data) == k_rx_ok) {
    telemetry_msg.has_battery_state = true;
    telemetry_msg.battery_state.voltage_mv        = bms_data.voltage_mv;
    telemetry_msg.battery_state.current_ma        = bms_data.current_ma;
    telemetry_msg.battery_state.state_of_charge   = bms_data.soc_percent;
    telemetry_msg.battery_state.temperature_01c   = bms_data.temp_deci_celsius;
    telemetry_msg.battery_state.fault_flags       = bms_data.fault_bits;
    telemetry_msg.battery_state.is_charging       = bms_data.is_charging;
}
```

### Required BMS API

`bms_monitor_task.h` needs thread-safe getter functions:

```c
/**
 * @struct bms_snapshot_t
 * @brief Snapshot of latest BMS data (thread-safe copy)
 */
typedef struct {
    uint32_t voltage_mv;          /**< Pack voltage in millivolts */
    int32_t  current_ma;          /**< Current (positive=discharge) */
    uint8_t  soc_percent;         /**< State of charge 0-100% */
    int16_t  temp_deci_celsius;   /**< Temperature in 0.1°C units */
    bool     is_charging;         /**< True if charging current flowing */
    uint32_t fault_bits;          /**< BQ4050 fault register bits */
} bms_snapshot_t;

/**
 * @brief Get a thread-safe snapshot of the latest BMS data.
 *
 * @param[out] snapshot Destination for BMS data
 *
 * @return rx_err_t
 * @retval k_rx_ok Data copied successfully
 * @retval k_rx_err_null_ptr snapshot is NULL
 * @retval k_rx_err_not_ready BMS task not yet started
 */
rx_err_t bms_get_snapshot(bms_snapshot_t* snapshot);
```

### Implementation Plan

1. Add `bms_snapshot_t` struct and mutex to `bms_monitor_task.c`
2. Update BMS polling loop to populate snapshot under mutex
3. Expose `bms_get_snapshot()` in `bms_monitor_task.h`
4. Call from `telemetry_task.c` in `build_telemetry_message()`
5. Write Unity test for `bms_get_snapshot()`

### Estimated Effort: 4-6 hours

---

## Gap 2: RPLiDAR C1 Integration (MEDIUM)

### Why It's Needed

The RPLiDAR C1 is listed in the project overview as hardware:
> "Lidar: RPLiDAR C1 (12m range, IP54)"

Without lidar:
- Cannot do SLAM (map building + localization)
- Obstacle detection limited to 4 HC-SR04 sonar sensors (limited range/coverage)
- Autonomous navigation impossible

### Interface Investigation

RPLiDAR C1 uses UART at 460800 baud (or 115200 baud in standard mode).

**Pin Assignment Required:**
- Current debug UART uses SCI9 (P B7/PB6) at 115200 baud
- RPLiDAR needs a separate UART channel
- Available UART channels on RX72N: SCI0-SCI6 (several unused)
- Recommended: SCI1 or SCI2

**Hardware Connection:**
```
RPLiDAR C1 → RX72N
RX (Lidar)  → TX (SCI1, pin ?)
TX (Lidar)  → RX (SCI1, pin ?)
GND         → GND
VCC (5V)    → 5V power rail (from STAR_POWER_BOARD)
CTRL        → GPIO (for motor enable/disable)
```

*Must check available SCI pins in pinout document and hardware schematic.*

### Files to Create

```
libs/rx_rplidar/
├── inc/
│   └── rx_rplidar.h
└── src/
    └── rx_rplidar.c

src/tasks/
└── lidar_scan_task.c     (10 Hz scan polling)

tests/
└── test_rx_rplidar.c     (Unity tests with mock UART)
```

### RPLiDAR Protocol Overview

RPLiDAR uses a simple request/response ASCII-like protocol:

```
# Start scan:
TX: 0xA5 0x20  (start express scan command)
RX: 0xA5 0x5A [response descriptor]
RX: [scan data packets...]

# Each scan data packet:
# quality(1B) + angle_q6(2B) + distance_q2(4B)
# angle = angle_q6 / 64.0 degrees
# distance = distance_q2 / 4.0 mm
```

### Driver API Design

```c
/**
 * @brief Initialize RPLiDAR C1 driver.
 *
 * @param[in] config UART and motor GPIO configuration
 *
 * @return rx_err_t
 * @retval k_rx_ok Initialized successfully
 * @retval k_rx_err_io UART initialization failed
 */
rx_err_t rx_rplidar_init(const rx_rplidar_config_t* config);

/**
 * @brief Start the lidar scan motor and data stream.
 */
rx_err_t rx_rplidar_start_scan(void);

/**
 * @brief Stop the scan motor.
 */
rx_err_t rx_rplidar_stop_scan(void);

/**
 * @brief Get the latest complete 360° scan.
 *
 * @param[out] scan   Buffer for scan points
 * @param[in]  max_points  Maximum points to copy
 * @param[out] num_points  Actual points written
 *
 * @return rx_err_t
 * @retval k_rx_ok Scan data copied
 * @retval k_rx_err_not_ready No complete scan available yet
 */
rx_err_t rx_rplidar_get_scan(
    rx_lidar_point_t* scan, uint16_t max_points, uint16_t* num_points);
```

### Proto Changes Required

Add lidar scan data to telemetry or wire protocol:

```protobuf
// Option A: Add to telemetry.proto
message TelemetryData {
    // ... existing fields ...
    repeated LidarPoint lidar_scan = 20;  // Latest 360° scan
}

message LidarPoint {
    float angle_deg = 1;      // 0-360 degrees
    float distance_mm = 2;    // Distance (0 = invalid)
    uint32 quality = 3;       // Signal quality 0-255
}
```

Note: With nanopb static allocation, limit scan to reasonable number of points:
```
# telemetry.options
TelemetryData.lidar_scan: max_count:360  (1 point per degree)
```

At 360 points × (4+4+4) = 4320 bytes per scan — this is significant for SPI transfer.
Consider: Send scan asynchronously at lower rate (2 Hz), separate from 20 Hz telemetry.

### Estimated Effort: 2-3 days

---

## Gap 3: Host I2C (RIIC0) Sensors (LOW)

### Current State

- **Pins:** P12 (SCL, pin 44) and P13 (SDA, pin 45) allocated for "Host I2C"
- **Peripheral:** RIIC0 initialized in `hardware_init.c`
- **Driver:** Low-level RIIC driver in `rx_hal` (shared with RIIC1 for BMS)
- **Application:** None — no firmware task uses RIIC0

### Likely Future Sensors

If the project adds sensors via I2C:

| Sensor | Purpose | IC | I2C Address |
|--------|---------|-----|-------------|
| IMU | Orientation/acceleration | MPU-6050 or ICM-42688 | 0x68 |
| Barometric | Altitude | BMP280 or LPS22HB | 0x77 |
| Humidity | Environment | SHT30 | 0x44 |

### Recommendation

**No action required now.** Document RIIC0 is reserved for future I2C expansion sensors. Update this plan when a specific sensor is chosen.

If an IMU is added:
1. Create `libs/rx_imu/` driver
2. Create `imu_task.c` (100 Hz polling)
3. Add `imu_data` to telemetry proto message
4. Connect to EKF in ROS2 (robot_localization)

---

## Hardware Pinout Verification Checklist

Before first hardware bring-up, verify these connections:

```
Motor Signals (GPTW):
[ ] Motor FL: GTIOCA0/GTIOCB0 → H-bridge inputs confirmed?
[ ] Motor FR: GTIOCA1/GTIOCB1 → H-bridge inputs confirmed?
[ ] Motor BL: GTIOCA2/GTIOCB2 → H-bridge inputs confirmed?
[ ] Motor BR: GTIOCA3/GTIOCB3 → H-bridge inputs confirmed?

Encoder Signals (MTU/TPU):
[ ] Motor FL encoder: MTIOC1A/MTIOC1B (P21/P20)?
[ ] Motor FR encoder: MTIOC2A/MTIOC2B?
[ ] Motor BL encoder: TPU channel?
[ ] Motor BR encoder: TPU channel?

Host Interface:
[ ] RSPI2 CS/SCK/COPI/CIPO connected to RPi5 SPI pins?
[ ] SPI speed set to 10 Mbps in both firmware and RPi5 device tree?

BMS:
[ ] RIIC1 connected to BQ4050?
[ ] BMS ALERT connected to P05/IRQ13?

HC-SR04 Sonar:
[ ] 4x TRIG pins match firmware GPIO config?
[ ] 4x ECHO pins match IRQ8-IRQ11?

Debug:
[ ] SCI9 TX/RX accessible via USB-C debug cable?
[ ] UART settings: 115200 8N1?
```

---

## PCB Manufacturing Status

```
schematic/
├── STAR_MCU.kicad_pro     ✅ Design complete
├── STAR_POWER_BOARD.kicad_pro ✅ Design complete
├── STAR_BMS.kicad_pro     ✅ Design complete
├── TOM_MCU_144Pin.kicad_pro ✅ Design reference
└── FAB OUTPUTS/
    ├── STAR_MCU_gerbers/  ✅ Gerbers generated
    ├── STAR_PWR_gerbers/  ✅ Gerbers generated
    └── STAR_BMS_gerbers/  ✅ Gerbers generated
```

**Manufacturing status unknown.** Check if boards have been fabricated and assembled.
