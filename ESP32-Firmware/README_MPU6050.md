# MPU6050 Comprehensive Technical Documentation

## Overview

This directory contains complete technical documentation and C code for implementing a production-grade MPU6050 6-axis IMU driver on ESP32 using the ESP-IDF framework. All documentation is based on official InvenSense datasheets and industry best practices.

## Files Included

### 1. **MPU6050_TECHNICAL_REFERENCE.md** (Main Document)
The primary technical specification document covering:
- Complete register map (0x00-0x75) with bit-level definitions
- I2C protocol specifications and timing
- Sensor sensitivity and range specifications
- DLPF (Digital Low-Pass Filter) configuration
- Sample rate calculation and control
- Interrupt configuration
- FIFO operation and management
- Self-test procedures and calibration
- Security and reliability considerations
- Memory safety for C drivers
- Complete working driver example

**Key Sections**:
- Register Map: Tables showing every register address, R/W status, and function
- Accelerometer/Gyroscope Ranges: ±2g/±4g/±8g/±16g and ±250°/s through ±2000°/s
- Sensitivity Values: LSB/g and LSB/°/s for each range
- DLPF Modes: 7 configurable digital filters (260Hz down to 5Hz)
- Sample Rate: Formulas and tables for calculating output rates

### 2. **mpu6050_registers.h** (Register Definitions)
Complete C header file with all register definitions:
- All register addresses as #define constants
- Bit definitions for configuration registers
- Sensitivity constants (LSB values)
- FIFO configuration constants
- Sample rate divider pre-calculated values
- Utility macros for bit manipulation
- Static inline helper functions for data conversion

**Provides**:
```c
#define MPU6050_REG_ACCEL_XOUT_H    0x3B
#define MPU6050_ACCEL_LSB_2G        16384.0f
#define MPU6050_GYRO_LSB_250        131.0f
#define MPU6050_ASSEMBLE_INT16(msb, lsb) ((int16_t)(((msb) << 8) | (lsb)))
```

### 3. **mpu6050_security_safety.h** (Safety Practices)
Critical security and memory safety guidelines:
- FIFO overflow protection strategies
- Signed vs unsigned data handling
- Invalid range detection
- I2C clock stretching safety
- Interrupt race condition prevention
- Register access bounds checking
- Buffer overflow prevention
- Diagnostic and monitoring structures

**Critical Functions**:
```c
mpu6050_validate_fifo_count()      // Safe FIFO validation
mpu6050_assemble_signed()          // Correct 16-bit assembly
mpu6050_validate_data()            // Detect hardware errors
mpu6050_i2c_bus_hanging()          // Detect bus issues
```

### 4. **MPU6050_QUICK_REFERENCE.md** (Developer Cheat Sheet)
One-page quick reference with:
- Essential register addresses
- Sensitivity values (copy-paste ready)
- Configuration profiles (low-power, balanced, high-performance)
- I2C communication patterns
- Critical data reading patterns
- Common issues and solutions
- Memory safety checklist
- Typical data read patterns
- Configuration code snippets

### 5. **mpu6050_example_usage.c** (Working Implementation)
Complete, production-ready C code example demonstrating:
- I2C initialization with proper timing
- Safe sensor initialization
- Configuration for balanced operation (100Hz, ±4g, ±500°/s)
- Atomic 14-byte burst reads (prevents data tearing)
- Data validation and error detection
- Physical unit conversion
- FIFO operation with overflow protection
- FreeRTOS task example
- Comprehensive error handling

**Includes**:
- Complete I2C wrapper functions
- Safe register read/write with timeout handling
- Sensor data validation logic
- FIFO read with bounds checking
- Example FreeRTOS tasks

## Quick Start Guide

### 1. Copy Files to Your ESP32 Project

```bash
cp mpu6050_registers.h your_project/components/mpu6050/
cp mpu6050_security_safety.h your_project/components/mpu6050/
cp mpu6050_example_usage.c your_project/main/
```

### 2. Basic I2C Setup

```c
// Initialize I2C at 400 kHz with 20ms timeout
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,
};
i2c_param_config(I2C_NUM_0, &conf);
i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
i2c_set_timeout(I2C_NUM_0, 20000);
```

### 3. Configure and Read Sensor

```c
// Configure for 100 Hz with ±4g accelerometer, ±500°/s gyroscope
mpu6050_write_register(port, 0x1A, 0x01);   // DLPF mode 1
mpu6050_write_register(port, 0x19, 9);      // 100 Hz sample rate
mpu6050_write_register(port, 0x1B, 0x08);   // ±500°/s
mpu6050_write_register(port, 0x1C, 0x08);   // ±4g

// Read all 14 bytes atomically
uint8_t buffer[14];
mpu6050_read_registers(port, 0x3B, 14, buffer);

// Parse as signed 16-bit values
int16_t accel_x = (int16_t)((buffer[0] << 8) | buffer[1]);
int16_t temp = (int16_t)((buffer[6] << 8) | buffer[7]);
int16_t gyro_x = (int16_t)((buffer[8] << 8) | buffer[9]);

// Convert to physical units
float accel_g = accel_x / 8192.0f;      // ±4g range
float gyro_dps = gyro_x / 65.5f;        // ±500°/s range
float temp_c = (temp / 340.0f) + 36.53f;
```

## Critical Implementation Details

### 1. Signed Data Handling (Most Common Bug)

**WRONG** (gets unsigned interpretation):
```c
uint16_t value = (buffer[0] << 8) | buffer[1];
float result = value / 16384.0;  // 0-4g instead of -2 to +2g
```

**CORRECT** (gets signed interpretation):
```c
int16_t value = (int16_t)((buffer[0] << 8) | buffer[1]);
float result = value / 16384.0;  // -2 to +2g
```

### 2. FIFO Safety (Prevents Buffer Overflow)

```c
uint16_t fifo_count = ((uint16_t)fifo_h << 8) | (uint16_t)fifo_l;  // uint16_t!
if (fifo_count > 1024 || fifo_count % 14 != 0) {
    // Error: reset FIFO
    mpu6050_write_register(port, 0x6A, 0x84);
    return ESP_ERR_INVALID_STATE;
}
uint16_t samples = fifo_count / 14;
if (samples > buffer_capacity) {
    return ESP_ERR_NO_MEM;  // Don't overrun buffer
}
```

### 3. Atomic Burst Reads (Prevents Data Tearing)

Always read all 14 bytes in a single I2C transaction using burst read:
```
START -> ADDR+R -> [14 bytes with ACKs, last NACK] -> STOP
```

This prevents "tearing" where you read the old accelerometer value mixed with the new gyroscope value.

### 4. I2C Timeout Configuration

MPU6050 may hold SCL low for clock stretching:
```c
i2c_set_timeout(port, 20000);  // 20ms minimum for safety
// or safer:
i2c_set_timeout(port, 200000); // 200ms to be very safe
```

### 5. DLPF and Sample Rate Selection

The relationship is:
- DLPF_CFG = 0: Internal 8kHz, output rate formula: 8000 / (1 + SMPLRT_DIV)
- DLPF_CFG = 1-6: Internal 1kHz, output rate formula: 1000 / (1 + SMPLRT_DIV)

For 100 Hz with DLPF mode 1 (184Hz BW, good for most applications):
```c
CONFIG = 0x01;          // DLPF mode 1
SMPLRT_DIV = 9;         // 1000 / (1+9) = 100 Hz
```

## Sensor Specifications Summary

### Accelerometer
- Ranges: ±2g, ±4g, ±8g, ±16g
- Sensitivity: 16384 LSB/g (±2g) down to 2048 LSB/g (±16g)
- Noise: 220 µg/√Hz
- Bandwidth with DLPF: 5Hz to 260Hz

### Gyroscope
- Ranges: ±250°/s, ±500°/s, ±1000°/s, ±2000°/s
- Sensitivity: 131 LSB/°/s (±250°/s) down to 16.4 LSB/°/s (±2000°/s)
- Noise: 4 m°/s/√Hz
- Bandwidth with DLPF: 5Hz to 256Hz

### Temperature
- Range: -40°C to +85°C
- Sensitivity: 340 LSB/°C
- Reference: 36.53°C at 0 LSB

## Common Issues and Solutions

| Issue | Cause | Solution |
|-------|-------|----------|
| Values 0-65535 instead of -2 to +2g | Unsigned interpretation | Cast to int16_t |
| FIFO overflow errors | Not reading fast enough | Lower sample rate or read more frequently |
| I2C timeout/freezes | Timeout too short | Set timeout to 200ms minimum |
| All zeros read | I2C communication failure | Check address (0x68 vs 0x69), pull-ups |
| Missing gravity on Z-axis | Not compensating for gravity | Normal! Apply calibration offset |
| Temperature reads 150°C | Data interpretation error | Check signed/unsigned handling |

## Testing Checklist

Before deployment:

- [ ] WHO_AM_I reads 0x68
- [ ] Sensor wakes from sleep mode
- [ ] DLPF mode applies (check bandwidth in raw data)
- [ ] Sample rate is correct (check timing with GPIO toggle)
- [ ] Accelerometer reads ~1g on Z-axis (with gravity)
- [ ] Gyroscope reads near 0 when stationary
- [ ] Temperature in reasonable range (-40 to +85°C)
- [ ] FIFO overflow interrupt triggers when FIFO fills
- [ ] Data validation catches bad reads
- [ ] Error rate < 1% after 1000 reads
- [ ] Stack doesn't overflow (no large buffers on stack)
- [ ] I2C bus recovers from timeouts

## Performance Expectations

With recommended balanced configuration (100 Hz, ±4g, ±500°/s):
- I2C transaction time: ~150 µs per register read, ~500 µs for 14-byte burst
- Update latency: 10 ms typical (1/100 Hz)
- FIFO full time: 7.3 seconds (73 samples * 100ms each)
- CPU usage: <1% on dual-core ESP32
- Power consumption: ~3.5 mA active, <1 µA standby

## References

**Official Documentation**:
- MPU-6000/MPU-6050 Product Specification Revision 3.4 (PS-MPU-6000A)
- MPU-6000/MPU-6050 Register Map and Descriptions Revision 4.0 (RM-MPU-6000A)
- Both available from: https://invensense.tdk.com/

**Standards**:
- I2C Bus Specification and User Manual v6 (NXP Semiconductors)
- ESP32 Technical Reference Manual (Espressif)

**Reference Implementations**:
- esp-idf-lib: https://github.com/espressif/esp-idf-lib (Official Espressif)
- i2cdevlib: https://github.com/jrowberg/i2cdevlib (Community standard)
- Adafruit MPU6050: https://github.com/adafruit/Adafruit_MPU6050

## License and Attribution

This documentation is provided for educational and commercial use. References the official InvenSense datasheets and ESP32 technical documentation.

## Support and Troubleshooting

For issues:
1. Check the QUICK_REFERENCE.md for common problems
2. Review TECHNICAL_REFERENCE.md section 9 (Security) and section 10 (Memory Safety)
3. Enable ESP_LOG to DEBUG level to see I2C transactions
4. Verify I2C bus with logic analyzer (SCL/SDA signals)
5. Check for bus hang with repeated timeouts (indicates clock stretching issue)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-20
**Target Platform**: ESP32 with ESP-IDF Framework
**Author Notes**: All code follows C99 standard with safety-first design principles
