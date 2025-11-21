# RPLiDAR C1 DTOF Scanner - Complete Technical Documentation
## ESP32-IDF C Driver Implementation Package

**Documentation Date:** 2025-11-20
**Package Version:** 2.0
**Target Platform:** ESP32-IDF v5.5.1+
**Device:** SLAMTEC RPLiDAR C1 (360-degree DTOF Laser Scanner)
**Baud Rate:** 460800 bps

---

## Package Contents

This comprehensive package includes complete technical documentation and production-ready C driver code for integrating RPLiDAR C1 with ESP32 microcontrollers.

### Documentation Files (Complete Reference)

#### 1. **RPLIDAR_C1_TECHNICAL_SPECIFICATION.md** (Primary Reference - 600+ lines)
Comprehensive technical documentation containing:
- **UART Protocol** - Complete serial communication specification
  - Serial configuration and timing
  - Protocol flow diagrams
  - Packet structure overview

- **Command Structure** - All 8 commands with detailed specifications
  - STOP_SCAN, RESET, SCAN, FORCE_SCAN
  - EXPRESS_SCAN (4-8kHz modes)
  - GET_INFO, GET_HEALTH, SET_PWM
  - Request/response packet formats
  - Checksum calculation

- **Data Packet Format** - Point measurement encoding
  - Standard 5-byte scan data (2kHz)
  - Express 2-byte packed format (4-8kHz)
  - Field decoding (quality, angle, distance)
  - Bit-field layout and validation

- **Motor Control**
  - PWM specifications (0-1023 range)
  - Speed vs. scan frequency table
  - Soft-start/soft-stop sequences
  - Motor race condition prevention
  - Motor health monitoring

- **Measurement Modes**
  - SCAN mode comparison
  - EXPRESS mode configurations
  - FORCE_SCAN single-frame operation
  - Mode transition safety

- **Error Codes and Status**
  - Health status values (Good/Warning/Error)
  - Complete error code bit definitions
  - UART communication errors
  - Error interpretation guide

- **Security Considerations** (Critical)
  - Packet parsing buffer overflow prevention
  - Invalid field value validation
  - Motor race condition mitigation
  - UART frame error recovery
  - Input validation patterns

- **Memory Safety**
  - Circular scan buffer implementation
  - Point cloud management (safe allocation)
  - UART RX overflow prevention
  - Ring buffer synchronization
  - Memory bound checking

- **ESP32-IDF Implementation**
  - Driver architecture overview
  - FreeRTOS integration patterns
  - UART configuration templates

- **Code Examples** - 3 complete usage examples
  - Basic initialization and health check
  - Continuous scanning with data acquisition
  - Motor speed control with gradient ramping

#### 2. **RPLIDAR_C1_QUICK_REFERENCE.md** (Cheat Sheet)
Fast-lookup reference for:
- Hardware specifications table
- UART protocol overview
- Command byte quick list
- Measurement data format (parsing code)
- Health status codes
- Device info response format
- Motor PWM quick table
- Error handling patterns
- Memory safety quick patterns
- Troubleshooting guide
- API quick reference
- Power management notes

#### 3. **README_RPLIDAR_C1.md** (This File)
Package overview, contents, and integration guide.

---

### Source Code Files (Production-Ready)

#### 1. **rplidar_c1_driver.h** (Public API Header - 600+ lines)

Complete public interface with:

**Data Types & Structures:**
```c
// Measurement data
rplidar_measurement_t  // Single point (distance, angle, quality, timestamp)
rplidar_frame_t        // Complete 360° frame (points array + metadata)
rplidar_device_info_t  // Device information response
rplidar_health_t       // Health status and error codes
rplidar_circular_buffer_t // Thread-safe circular frame buffer
rplidar_descriptor_t   // Response packet descriptor
rplidar_config_t       // Driver configuration
rplidar_driver_t       // Driver instance (main handle)
rplidar_stats_t        // Runtime statistics
```

**Enumerations:**
```c
rplidar_command_t      // Command types (SCAN, EXPRESS, etc.)
rplidar_data_type_t    // Response data types
rplidar_express_mode_t // Express mode selection
rplidar_health_status_t // Health status (Good/Warning/Error)
rplidar_state_t        // Driver state machine
rplidar_status_t       // Return status codes
```

**Core API Functions:**
```c
// Driver lifecycle
rplidar_init()         // Initialize driver
rplidar_deinit()       // Cleanup and shutdown

// Scanning control
rplidar_start_scan()   // Start standard 2kHz scan
rplidar_start_express_scan() // Start 4-8kHz express mode
rplidar_force_scan()   // Single frame acquisition
rplidar_stop_scan()    // Stop scanning and motor

// Motor control
rplidar_set_motor_pwm()    // Direct PWM control
rplidar_soft_start()       // Gradual acceleration
rplidar_soft_stop()        // Gradual deceleration

// Status queries
rplidar_get_health()   // Query device health
rplidar_get_info()     // Get device information
rplidar_get_state()    // Get current driver state
rplidar_is_scanning()  // Check if actively scanning
rplidar_get_stats()    // Runtime statistics
rplidar_get_buffer_status() // Buffer occupancy

// Data acquisition
rplidar_read_measurement()  // Read single point
rplidar_read_frame()        // Read completed frame

// Error recovery
rplidar_clear_buffers()     // Resynchronize UART

// Utility functions
rplidar_calculate_checksum()
rplidar_validate_measurement()
rplidar_parse_measurement()
rplidar_pwm_to_percent()
rplidar_percent_to_pwm()
rplidar_error_string()
rplidar_error_code_string()
```

**Advanced Features:**
```c
// Circular buffer management
rplidar_create_frame_buffer()
rplidar_write_frame()
rplidar_read_frame_buffer()
rplidar_destroy_frame_buffer()
```

#### 2. **rplidar_c1_driver.c** (Core Implementation - 800+ lines)

Production-ready implementation with:

**Key Features:**
- **Memory Safety:** Bounds checking on all buffer operations
- **Error Handling:** Comprehensive validation and recovery
- **Thread Safety:** FreeRTOS spinlock protection for shared data
- **UART Robustness:** Automatic resynchronization and overflow handling
- **Packet Parsing:** Secure parsing with checksum validation

**Critical Functions Implemented:**
- Utility functions (checksum, validation, conversion)
- Measurement parsing with full validation
- Packet construction and transmission
- UART communication with timeout handling
- Circular buffer with overflow management
- Driver state machine and synchronization
- All core API functions

**Security Hardening:**
- Buffer overflow protection (size validation before reads)
- Invalid field value detection (angle range, distance range, quality bounds)
- Motor race condition prevention (state-machine based locking)
- UART error handling (frame error, overflow, timeout recovery)
- Measurement validation (sync bits, check bit, field ranges)

#### 3. **rplidar_c1_example_usage.c** (Usage Examples - 500+ lines)

Six complete, compilable examples demonstrating:

**Example 1: Basic Initialization**
```c
void example_basic_initialization()
```
- Device initialization
- Health check
- Device information query
- Error handling

**Example 2: Continuous Scanning**
```c
void example_continuous_scanning()
```
- Scanning startup
- UART event handling
- Measurement parsing
- Data callback implementation

**Example 3: Point Cloud Accumulation**
```c
void example_point_cloud_accumulation()
```
- Frame buffer management
- Point cloud processing task
- Frame completion detection
- Closest point detection

**Example 4: Motor Control**
```c
void example_motor_control()
```
- Soft-start sequences
- Speed ramping
- Motor state management
- Safe transitions

**Example 5: Error Recovery**
```c
void example_error_recovery()
```
- Health monitoring task
- Automatic recovery actions
- Error categorization
- Graceful degradation

**Example 6: Statistics Monitoring**
```c
void example_statistics_monitoring()
```
- Real-time statistics collection
- Buffer status monitoring
- Performance metrics
- Diagnostic output

---

## Quick Start Guide

### 1. Installation

Copy all files to your ESP32 project:
```bash
# Documentation
cp RPLIDAR_C1_TECHNICAL_SPECIFICATION.md <project>/docs/
cp RPLIDAR_C1_QUICK_REFERENCE.md <project>/docs/
cp README_RPLIDAR_C1.md <project>/docs/

# Source code
cp rplidar_c1_driver.h <project>/components/rplidar/include/
cp rplidar_c1_driver.c <project>/components/rplidar/
cp rplidar_c1_example_usage.c <project>/main/
```

### 2. Configure Hardware

**ESP32 Pin Connections:**
```
ESP32          RPLiDAR C1
GPIO_NUM_17    RX (device receives)
GPIO_NUM_16    TX (device sends)
GND            GND
5V supply      VCC (4.8-5.2V)
```

### 3. Basic Integration

```c
#include "rplidar_c1_driver.h"

void lidar_task(void* arg) {
    // Configuration
    rplidar_config_t config = {
        .uart_num = UART_NUM_1,
        .tx_pin = GPIO_NUM_17,
        .rx_pin = GPIO_NUM_16,
        .baud_rate = RPLIDAR_BAUD_RATE,
        .rx_buffer_size = 8192,
        .frame_buffer_size = 10
    };

    rplidar_driver_t driver;

    // Initialize
    if (rplidar_init(&config, &driver) != RPLIDAR_OK) {
        return;
    }

    // Start scanning
    rplidar_start_scan(&driver);

    // Process data
    rplidar_frame_t frame;
    while (1) {
        if (rplidar_read_frame(&driver, &frame)) {
            // Process frame (5000 points max)
            for (int i = 0; i < frame.count; i++) {
                float dist_m = frame.points[i].distance_mm / 1000.0f;
                float angle = frame.points[i].angle_deg;
                // Use data...
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    rplidar_stop_scan(&driver);
    rplidar_deinit(&driver);
}

void app_main(void) {
    xTaskCreate(lidar_task, "lidar", 8192, NULL, 5, NULL);
}
```

### 4. Build Integration

Add to `CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "rplidar_c1_driver.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos)
```

---

## Key Technical Details

### UART Configuration
- **Baud Rate:** 460800 bps (critical - exact value required)
- **Data Bits:** 8
- **Stop Bits:** 1
- **Parity:** None
- **Flow Control:** Disabled
- **Clock Source:** UART_SCLK_REF_TICK (more stable than APB)

### Recommended Buffer Sizes
- **RX Buffer:** 8192 bytes (covers 100ms at 5000 Hz)
- **Frame Buffer:** 10 frames (covers 1 second at 10 Hz)
- **Max Points:** 5000 per frame

### Motor Speed Mapping
- **Default:** 660 PWM (100% nominal, 10 Hz, 5 kHz sample rate)
- **Min Reliable:** 200 PWM (≈20%, motor may stall below)
- **Max Safe:** 1023 PWM (100%, thermal limit consideration)

### Data Rate
- **Standard Mode:** ~25 KB/s (5 bytes * 5000 Hz)
- **Express Mode:** ~30-40 KB/s (2 bytes * 8000 Hz)
- **Throughput limit:** Dependent on processing and ESP32 load

### Performance Metrics
- **Measurement Rate:** 5000 samples/second
- **Frame Rate:** 10 frames/second (3600 DPI equivalent)
- **Latency:** <100 ms typical
- **Angular Resolution:** 0.72° (at 10 Hz) to 0.1° (in express 8 kHz)

---

## Critical Security Considerations

### 1. Buffer Overflow Protection
All read operations validate buffer sizes before copying:
```c
if (data_length > MAX_PACKET_SIZE) {
    return RPLIDAR_ERR_SIZE_EXCEEDED;
}
```

### 2. Packet Validation
Every measurement is validated before use:
```c
// Check sync bits
if (sync_s == sync_s_inv) return false;
// Check C bit
if (check_bit != 1) return false;
// Validate ranges
if (angle > 360.0f) return false;
if (distance < 200 || distance > 48000) return false;
```

### 3. Motor Safety
Race conditions prevented by state machine:
```c
// SAFE: Synchronized transitions
rplidar_stop_scan(&driver);      // Stops motor
vTaskDelay(500 ms);              // Wait for stop
rplidar_start_scan(&driver);     // Start new mode
```

### 4. UART Robustness
Automatic recovery from communication errors:
```c
// On overflow/error, resynchronize
uart_flush_input(uart_num);
// Wait for 0xA5 sync byte
// Resume normal operation
```

---

## Troubleshooting & Common Issues

### Issue: Garbage Data / "Invalid Descriptor" Errors

**Root Cause:** Baud rate mismatch (most common)

**Solution:**
```c
// Verify exact baud rate
uart_config.baud_rate = 460800;  // Must be exact!
uart_config.source_clk = UART_SCLK_REF_TICK;

// Clear buffers and resync
rplidar_clear_buffers(&driver);
```

### Issue: Frame Loss / "Buffer Overflow"

**Root Cause:** Processing slower than data arrival (25 KB/s)

**Solution:**
```c
// Increase buffer size
.frame_buffer_size = 20  // More frames

// Reduce data rate
rplidar_set_motor_pwm(&driver, 500);  // 50% speed

// Check buffer status
uint32_t avail;
float pct;
rplidar_get_buffer_status(&driver, &avail, &pct);
```

### Issue: Motor Won't Start / Power Error

**Root Cause:** Insufficient power supply

**Solution:**
```c
// Ensure 4.8-5.2V supply with:
// - Adequate current capacity (500-1000mA continuous)
// - Clean power with <0.1V ripple
// - Separate ground plane for signal integrity

// Check error code
rplidar_health_t health;
rplidar_get_health(&driver, &health);
// If error code includes 0x0008 = low power
```

### Issue: Incomplete Data / Timeouts

**Root Cause:** UART interrupt congestion

**Solution:**
```c
// Increase RX buffer
.rx_buffer_size = 16384  // 16 KB

// Raise UART task priority
xTaskCreate(..., "uart_task", 8192, arg, 20, NULL);
                                              ^^^ Higher

// Use pattern detection
uart_enable_pattern_det_intr(uart_num, 0xA5, 1, 10000, 10, 10);
```

---

## Performance Optimization Tips

### For Real-Time SLAM
1. Use Standard SCAN mode (lower latency)
2. Process frames at 10 Hz rate
3. Use circular buffer of 5-10 frames
4. Implement octree point cloud for efficient storage

### For High-Resolution Mapping
1. Use EXPRESS_DENSE mode (8kHz, 0.1° resolution)
2. Increase frame buffer to 20+ frames
3. Use larger circular buffer (64+ MB if PSRam available)
4. Process offline or use external computer

### For Battery-Powered Systems
1. Run at 50-70% motor speed (500-700 PWM)
2. Use periodic scanning (e.g., 2 Hz instead of 10 Hz)
3. Implement power-saving mode (motor off, idle state)
4. Monitor battery voltage via ADC

---

## File Summary

| File | Lines | Purpose |
|---|---|---|
| RPLIDAR_C1_TECHNICAL_SPECIFICATION.md | 600+ | Complete technical reference |
| RPLIDAR_C1_QUICK_REFERENCE.md | 400+ | Quick lookup cheat sheet |
| rplidar_c1_driver.h | 600+ | Public API and data structures |
| rplidar_c1_driver.c | 800+ | Core implementation |
| rplidar_c1_example_usage.c | 500+ | 6 practical examples |
| README_RPLIDAR_C1.md | 500+ | This integration guide |
| **TOTAL** | **3400+** | **Complete package** |

---

## Official References

### SLAMTEC Documentation
- **Protocol Specification:** LR001_SLAMTEC_rplidar_S&C_series_protocol_v2.8_en.pdf
- **C1 Datasheet:** SLAMTEC_rplidar_datasheet_C1_v1.0_en.pdf
- **C1 User Manual:** SLAMTEC_rplidarkit_usermanual_C1_v1.0_en.pdf
- **SDK Repository:** https://github.com/Slamtec/rplidar_sdk

### ESP32 Resources
- **ESP-IDF UART Documentation:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html
- **FreeRTOS Integration:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
- **Official Examples:** https://github.com/espressif/esp-idf/tree/master/examples/peripherals/uart

### Community Resources
- **Arduino/ESP32 Port:** https://github.com/thijses/rplidar
- **Python Reference:** https://github.com/SkoltechRobotics/rplidar
- **Full-Featured Python:** https://github.com/Hyun-je/pyrplidar

---

## Support & Maintenance

### For Implementation Help
1. Consult **RPLIDAR_C1_TECHNICAL_SPECIFICATION.md** for protocol details
2. Review **rplidar_c1_example_usage.c** for usage patterns
3. Check **RPLIDAR_C1_QUICK_REFERENCE.md** for quick lookup
4. Verify hardware connections (5V, GND, TX/RX pins)

### For Protocol Issues
1. Verify baud rate is exactly 460800
2. Use logic analyzer to capture UART communication
3. Compare against official SLAMTEC protocol documents
4. Check ESP32 UART clock source stability

### For Integration Issues
1. Test with simple hello-world example first
2. Verify UART communication works (basic RX/TX)
3. Incrementally add complexity
4. Use ESP-IDF logging (ESP_LOGI/ESP_LOGE) for debugging

---

## Version History

**v2.0 (2025-11-20) - Current**
- Complete technical specification (600+ lines)
- Production-ready driver (800+ lines)
- 6 practical examples
- Comprehensive quick reference
- Security hardening and memory safety patterns
- FreeRTOS integration examples

**v1.0 (Baseline)**
- Initial protocol documentation

---

## License & Attribution

This implementation package is provided as technical documentation for the SLAMTEC RPLiDAR C1 device. The code examples and driver implementation follow SLAMTEC's official protocol specification v2.8.

**Proper Usage:**
- Use for personal/educational projects
- Credit SLAMTEC for protocol documentation
- Reference official SLAMTEC SDK when needed
- Follow device operating limits (thermal, power, mechanical)

---

## Summary

This package provides everything needed to integrate a SLAMTEC RPLiDAR C1 360-degree DTOF scanner with an ESP32 microcontroller using ESP-IDF:

✓ **3400+ lines of comprehensive documentation**
✓ **Production-ready C driver implementation**
✓ **Complete API reference with 30+ functions**
✓ **6 practical usage examples**
✓ **Security hardening and memory safety patterns**
✓ **Troubleshooting and optimization guide**
✓ **FreeRTOS task integration patterns**

**Ready for immediate use in robotics, SLAM, obstacle detection, and autonomous systems.**

Generated: 2025-11-20
Platform: ESP32-IDF v5.5.1+
Device: SLAMTEC RPLiDAR C1
Baud Rate: 460800 bps

---

**Questions or issues?** Refer to the comprehensive technical specification or check the quick reference guide for answers. All code examples are compilable and ready for integration.
