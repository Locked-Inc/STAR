# RPLiDAR C1 - Quick Reference Guide

## File Locations

All documentation and source files have been generated in:
```
C:\Users\sikar\CLionProjects\untitled\
```

### Documentation Files
- **RPLIDAR_C1_TECHNICAL_SPECIFICATION.md** - Complete technical reference (comprehensive 600+ line specification)
- **RPLIDAR_C1_QUICK_REFERENCE.md** - This quick reference guide

### Source Code Files
- **rplidar_c1_driver.h** - Public API header with all function prototypes and data structures
- **rplidar_c1_driver.c** - Core driver implementation with memory safety and error handling
- **rplidar_c1_example_usage.c** - 6 practical usage examples

---

## Hardware Specifications

| Parameter | Value |
|---|---|
| **Protocol** | UART TTL 3.3V |
| **Baud Rate** | 460800 bps |
| **Range** | 0.05 - 12 m |
| **Angular Resolution** | 0.72° @ 10Hz |
| **Sample Rate** | 5000 Hz |
| **Scanning Frequency** | 10 Hz (600 rpm default) |
| **Motor PWM Range** | 0-1023 (default: 660) |
| **Data Output** | 2.5D point cloud + reflectivity |
| **IP Rating** | IP54 |
| **Operating Temp** | -15°C to 60°C |

---

## UART Protocol Overview

### Sync Bytes
- Request: `0xA5` (start of command)
- Response: `0xA5 0x5A` (descriptor header)

### Baud Configuration
```c
uart_config_t config = {
    .baud_rate = 460800,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_REF_TICK  // More stable
};
```

### Packet Checksum
All packets use XOR checksum over all bytes except checksum itself:
```c
checksum = byte0 ^ byte1 ^ byte2 ^ ... ^ byteN
```

---

## Command Reference

### Quick Command Byte List

| Command | Hex | Purpose |
|---|---|---|
| STOP_SCAN | 0x25 | Stop scanning & motor |
| RESET | 0x40 | Reset device |
| SCAN | 0x20 | Start standard 2kHz scan |
| FORCE_SCAN | 0x21 | Single frame acquisition |
| EXPRESS_SCAN | 0x82 | High-density 4-8kHz scan |
| GET_INFO | 0x50 | Get device information |
| GET_HEALTH | 0x52 | Get health/error status |
| SET_PWM | 0xF0 | Set motor speed (0-1023) |

### Command Packet Format

**Simple (no payload):**
```
0xA5 [Command] 0x00 [Checksum]
```

**With Payload:**
```
0xA5 [Command] [Length] [Payload...] [Checksum]
```

### Response Descriptor Format
```
0xA5 0x5A [DataLen_LE] [DataType] [SingleResp] [Checksum]
 ^   ^     12 bytes   data type   0=multi    XOR
Sync marker            of data     1=single   check
```

---

## Measurement Data Format

### Standard Scan Mode (5 bytes per point)

```
Byte 0: [S | S̄ | Quality(6)]
Byte 1: [C | Angle_MSB(7)]
Byte 2: [Angle_LSB(8)]
Bytes 3-4: [Distance_LE(16)]

Parsing:
  quality = byte0 >> 2           // 6-bit value (0-63)
  angle_raw = ((byte1 & 0x7F) << 8) | byte2
  angle_deg = angle_raw / 64.0
  distance_raw = (byte4 << 8) | byte3
  distance_mm = distance_raw / 4
  distance_m = distance_mm / 1000.0
  is_new_scan = byte0 & 0x01

Validation:
  ✓ S bit must differ from S̄ bit
  ✓ Check bit (byte1 MSB) must equal 1
  ✓ Quality must be 0-63
  ✓ Angle must be 0-360°
  ✓ Distance must be 200-48000 (raw units)
```

### Health Status Response (3 bytes)

```
Byte 0: Status (0=Good, 1=Warning, 2=Error)
Bytes 1-2: Error Code (16-bit LE)

Error Code Bits:
  0x0001 = Hardware failure
  0x0002 = JTAG malfunction
  0x0004 = FPGA error
  0x0008 = Power supply low
  0x0010 = Motor circuit failure
  0x0040 = Temperature sensor error
  0x0080 = Temperature high
  0x0100 = Optical signal loss
  0x0200 = Light intensity low
```

### Device Info Response (20 bytes)

```
Byte 0: Model number (0xA4 = C1)
Bytes 1-2: Firmware version (Major, Minor)
Byte 3: Hardware version
Bytes 4-19: Serial number (16 bytes, hex)
```

---

## Motor Control

### PWM Values

| PWM | % | RPM | Hz | Use Case |
|---|---|---|---|---|
| 0 | 0% | 0 | 0 | Off |
| 200 | 19.6% | 120 | 2 | Minimum reliable |
| 500 | 48.9% | 240 | 4 | Half speed testing |
| 660 | 64.5% | 600 | 10 | **DEFAULT** |
| 800 | 78.2% | 480 | 8 | High density |
| 1023 | 100% | 615+ | 10+ | Maximum |

### Setting Motor Speed

```c
// Single command (unsafe during scan)
uint8_t payload[2] = {
    (uint8_t)(pwm & 0xFF),      // LSB
    (uint8_t)((pwm >> 8) & 0xFF) // MSB
};
send_command_with_payload(uart_num, 0xF0, payload, 2);

// Soft-start (safe, gradual acceleration)
rplidar_soft_start(&driver, 660, 50, 100);
//  Target=660, Step=50, Delay=100ms

// Soft-stop (safe, gradual deceleration)
rplidar_soft_stop(&driver, 50, 100);
//  Step=50, Delay=100ms
```

---

## Critical Security Issues

### 1. Buffer Overflow Protection
```c
// ALWAYS validate packet size before reading
if (data_length > MAX_PACKET_SIZE) {
    return PKT_ERR_SIZE_EXCEEDED;  // Don't read!
}
```

### 2. Measurement Validation
```c
// ALWAYS validate before using measurement
if (!rplidar_validate_measurement(buf)) {
    return false;  // Skip corrupted data
}

// Check individual fields
if (angle_deg > 360.0f) return false;
if (distance_mm < 200 || distance_mm > 48000) return false;
if (quality > 63) return false;
```

### 3. Motor Race Conditions
```c
// SAFE: Stop, wait, then change mode
rplidar_stop_scan(&driver);
vTaskDelay(500 / portTICK_PERIOD_MS);  // Motor stop time
rplidar_start_scan(&driver);  // New mode
```

### 4. UART Overflow Handling
```c
// Configure large RX buffer
uart_driver_install(uart_num, 8192, 2048, 20, queue, 0);

// Handle overflow events
case UART_FIFO_OVF:
    uart_flush_input(uart_port);  // Clear FIFO
    // Resynchronize to next packet
    break;
```

---

## Memory Safety Patterns

### Circular Frame Buffer
```c
// Create buffer for 10 frames
rplidar_circular_buffer_t* buf = rplidar_create_frame_buffer(10);

// Safe write (from ISR/task)
rplidar_write_frame(buf, &frame);

// Safe read (from main thread)
rplidar_frame_t frame;
if (rplidar_read_frame_buffer(buf, &frame)) {
    // Process frame
}

// Cleanup
rplidar_destroy_frame_buffer(buf);
```

### Point Cloud Management
```c
// Maximum points per frame
#define MAX_POINTS_PER_FRAME 5000

// Validate before adding
if (cloud->count >= cloud->capacity) {
    return false;  // Full
}

// Add with validation
if (point->angle_deg > 360.0f) return false;
if (point->distance_mm < 200) return false;
cloud->points[cloud->count++] = *point;
```

---

## Error Handling Patterns

### Health Check Loop
```c
void health_monitor(rplidar_driver_t* driver) {
    rplidar_health_t health;

    while (1) {
        rplidar_get_health(driver, &health);

        if (health.status == RPLIDAR_HEALTH_ERROR) {
            // Critical error - recovery needed
            rplidar_stop_scan(driver);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            rplidar_reset(driver);
        } else if (health.status == RPLIDAR_HEALTH_WARNING) {
            // Warning - reduce load
            rplidar_set_motor_pwm(driver, 500);
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}
```

### Packet Resynchronization
```c
// On communication error, resynchronize to packet boundary
rplidar_status_t resync_packets(uart_port_t uart_num) {
    uint8_t byte;
    int timeout = 5000;  // ms

    // Look for 0xA5 sync byte
    while (timeout > 0) {
        if (uart_read_bytes(uart_num, &byte, 1, 10) == 1) {
            if (byte == 0xA5) {
                return RPLIDAR_OK;  // Sync found
            }
        }
        timeout -= 10;
    }

    return RPLIDAR_ERR_TIMEOUT;
}
```

---

## Data Throughput

### Baud Rate Calculations
- **Baud Rate:** 460800 bps
- **Data per measurement:** 5 bytes + ~1 byte overhead = 6 bytes
- **Sample rate:** 5000 Hz = 30 KB/s theoretical
- **Practical sustained:** ~25 KB/s with overhead

### Buffer Sizing Recommendations
```c
// Minimum RX buffer for 100ms of data at 5kHz
// 5000 samples/sec * 5 bytes/sample * 0.1 sec = 2500 bytes
// Recommend 4-8KB minimum

// For express mode at 8kHz:
// 8000 samples/sec * 2 bytes/sample * 0.1 sec = 1600 bytes
```

---

## Troubleshooting

### Issue: Garbage Data / Sync Errors

**Cause:** Baud rate mismatch or UART configuration error

**Solution:**
```c
// Verify baud rate is exactly 460800
// Check source clock selection
uart_config.source_clk = UART_SCLK_REF_TICK;  // More stable

// Clear and resync
uart_flush_input(uart_num);
rplidar_clear_buffers(&driver);
```

### Issue: Frame Loss / Buffer Overflow

**Cause:** Processing too slow for 25 KB/s data rate

**Solution:**
```c
// Reduce processing load
// Check buffer status
uint32_t available;
float capacity_pct;
rplidar_get_buffer_status(&driver, &available, &capacity_pct);

if (capacity_pct > 80.0f) {
    // Reduce motor speed or improve processing
    rplidar_set_motor_pwm(&driver, 500);  // Reduce to 50%
}
```

### Issue: Motor Won't Start / Health Error

**Cause:** Power supply, circuit failure, or sensor issue

**Solution:**
```c
// Check error codes
rplidar_health_t health;
rplidar_get_health(&driver, &health);
ESP_LOGE(TAG, "Error: %s", rplidar_error_code_string(health.error_code));

// If power supply issue:
// Ensure 4.8-5.2V supply with adequate current capability (typically 1-2A)

// If motor circuit issue:
// Device requires replacement
```

### Issue: Incomplete Data / Timeouts

**Cause:** UART interrupt congestion or processing delays

**Solution:**
```c
// Use larger buffer and appropriate timeout
uart_driver_install(uart_num, 8192, 2048, 20, &queue, 0);

// Increase priority of UART processing task
xTaskCreate(uart_handler, "uart_task", 8192, arg, 20, NULL);
                                               ^^^ Higher priority

// Enable UART pattern detection for robustness
uart_enable_pattern_det_intr(uart_num, 0xA5, 1, 10000, 10, 10);
```

---

## API Quick Reference

### Initialization
```c
rplidar_config_t config = {...};
rplidar_driver_t driver;
rplidar_init(&config, &driver);
```

### Scanning Control
```c
rplidar_start_scan(&driver);              // 2kHz standard
rplidar_start_express_scan(&driver, 0x02); // 8kHz dense
rplidar_force_scan(&driver);              // Single frame
rplidar_stop_scan(&driver);
```

### Motor Control
```c
rplidar_set_motor_pwm(&driver, 660);      // Set to value
rplidar_soft_start(&driver, 660, 50, 100); // Gradient ramp
rplidar_soft_stop(&driver, 50, 100);      // Gradient stop
```

### Status & Data
```c
rplidar_get_health(&driver, &health);
rplidar_get_info(&driver, &info);
rplidar_read_measurement(&driver, &meas, 500);
rplidar_read_frame(&driver, &frame);
rplidar_get_stats(&driver, &stats);
```

### Cleanup
```c
rplidar_stop_scan(&driver);
rplidar_deinit(&driver);
```

---

## Platform-Specific: ESP32-IDF

### Essential GPIO Pins
```c
#define LIDAR_TX_PIN GPIO_NUM_17    // TX → LIDAR RX (device receives commands)
#define LIDAR_RX_PIN GPIO_NUM_16    // RX ← LIDAR TX (device sends data)
#define UART_NUM UART_NUM_1         // Use UART1 (UART0 for logging)
```

### FreeRTOS Synchronization
```c
// Protect shared data with spinlock
portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

portENTER_CRITICAL(&spinlock);
// Critical section - safe from interrupts
shared_variable = value;
portEXIT_CRITICAL(&spinlock);

// Or use semaphore
SemaphoreHandle_t sem = xSemaphoreCreateBinary();
xSemaphoreTake(sem, portMAX_DELAY);
// Critical section
xSemaphoreGive(sem);
```

### Power Management
```c
// If using deep sleep, ensure:
// 1. LIDAR powered from 4.8-5.2V stable supply (NOT ESP32 internal)
// 2. UART peripheral NOT accessible during deep sleep
// 3. External power hold circuit if needed for startup

// Typical current draw:
// Idle: ~50-100mA
// Scanning at 10Hz: ~500mA
// Express mode 8kHz: ~600mA
```

---

## References

### Official Documentation
- **SLAMTEC Protocol:** LR001_SLAMTEC_rplidar_S&C_series_protocol_v2.8_en.pdf
- **C1 Datasheet:** SLAMTEC_rplidar_datasheet_C1_v1.0_en.pdf
- **C1 User Manual:** SLAMTEC_rplidarkit_usermanual_C1_v1.0_en.pdf

### ESP32 Resources
- **ESP-IDF UART API:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html
- **FreeRTOS:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html

### Reference Implementations
- **SLAMTEC SDK:** https://github.com/Slamtec/rplidar_sdk
- **Arduino Port:** https://github.com/thijses/rplidar
- **Python Reference:** https://github.com/SkoltechRobotics/rplidar

---

## Summary

This quick reference provides immediate access to:
1. Hardware specs and UART configuration
2. Command bytes and packet formats
3. Measurement data parsing algorithms
4. Critical security patterns
5. Memory safety implementations
6. Error handling strategies
7. API quick lookup
8. Troubleshooting guide

For complete details, refer to **RPLIDAR_C1_TECHNICAL_SPECIFICATION.md** and the source code files.

Generated: 2025-11-20
Target Platform: ESP32-IDF v5.5.1+
