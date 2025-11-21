# MPU6050 Quick Reference Guide

## Essential Register Addresses

```
CONFIGURATION
0x19 - SMPLRT_DIV (Sample Rate Divider)
0x1A - CONFIG (DLPF Mode)
0x1B - GYRO_CONFIG (Gyro Range)
0x1C - ACCEL_CONFIG (Accel Range)
0x23 - FIFO_EN (FIFO Enable Control)

INTERRUPTS
0x37 - INT_PIN_CFG (Interrupt Pin Config)
0x38 - INT_ENABLE (Interrupt Control)
0x3A - INT_STATUS (Interrupt Status)

SENSOR DATA
0x3B-0x40 - ACCEL_XOUT_H/L to ACCEL_ZOUT_L (6 bytes)
0x41-0x42 - TEMP_OUT_H/L (2 bytes)
0x43-0x48 - GYRO_XOUT_H/L to GYRO_ZOUT_L (6 bytes)

CONTROL
0x6A - USER_CTRL (FIFO/I2C Master Control)
0x6B - PWR_MGMT_1 (Power Management)
0x72-0x73 - FIFO_COUNTH/L (FIFO Byte Count)
0x74 - FIFO_R_W (FIFO Read/Write)
0x75 - WHO_AM_I (Device ID - should be 0x68)
```

## Sensitivity Values (Critical for Correct Conversion)

### Accelerometer (LSB/g)
```
±2g:    16,384 LSB/g    (accel_g = raw / 16384)
±4g:    8,192  LSB/g    (accel_g = raw / 8192)
±8g:    4,096  LSB/g    (accel_g = raw / 4096)
±16g:   2,048  LSB/g    (accel_g = raw / 2048)
```

### Gyroscope (LSB/°/s)
```
±250°/s:   131    LSB/°/s    (gyro_dps = raw / 131)
±500°/s:   65.5   LSB/°/s    (gyro_dps = raw / 65.5)
±1000°/s:  32.8   LSB/°/s    (gyro_dps = raw / 32.8)
±2000°/s:  16.4   LSB/°/s    (gyro_dps = raw / 16.4)
```

### Temperature
```
Temperature (°C) = (raw_value / 340) + 36.53
```

## Configuration Profiles

### Profile 1: Ultra-Low Power (5 Hz)
```c
CONFIG = 0x06              // DLPF: 5Hz BW
SMPLRT_DIV = 199           // 5Hz output
GYRO_CONFIG = 0x00         // ±250°/s
ACCEL_CONFIG = 0x00        // ±2g
FIFO_EN = 0x78             // Accel + Gyro + Temp
```

### Profile 2: Balanced (100 Hz) - RECOMMENDED
```c
CONFIG = 0x01              // DLPF: 184Hz BW
SMPLRT_DIV = 9             // 100Hz output (1000/(1+9))
GYRO_CONFIG = 0x08         // ±500°/s
ACCEL_CONFIG = 0x08        // ±4g
FIFO_EN = 0x78             // Accel + Gyro + Temp
```

### Profile 3: High Performance (500 Hz)
```c
CONFIG = 0x00              // DLPF: 260Hz BW
SMPLRT_DIV = 15            // 500Hz output (8000/(1+15))
GYRO_CONFIG = 0x18         // ±2000°/s
ACCEL_CONFIG = 0x18        // ±16g
FIFO_EN = 0x78             // Accel + Gyro + Temp
```

## I2C Communication

### Slave Addresses
```
AD0 = GND:   0x68 (default)
AD0 = VDD:   0x69
```

### Timing
```
Bus Speed:           100-400 kHz (400 kHz recommended)
I2C Timeout:         200ms minimum (allow for clock stretching)
Master Wait Time:    1000ms typical
```

### Burst Read Pattern (Most Important)
```
1. START
2. Address byte with READ bit (0x68 | 0x01)
3. Device ACKs
4. Master reads bytes with ACK for all but last
5. Master reads last byte with NACK
6. STOP

Result: Register address auto-increments, preventing data tearing
```

## Critical Data Reading

### Read All 14 Bytes at Once (Prevents Data Tearing)
```
Read from register 0x3B for 14 consecutive bytes:
- Bytes 0-1:   Accel X (int16_t)
- Bytes 2-3:   Accel Y (int16_t)
- Bytes 4-5:   Accel Z (int16_t)
- Bytes 6-7:   Temp (int16_t)
- Bytes 8-9:   Gyro X (int16_t)
- Bytes 10-11: Gyro Y (int16_t)
- Bytes 12-13: Gyro Z (int16_t)
```

### Correct 16-bit Conversion
```c
// CRITICAL: Always use signed type
int16_t value = (int16_t)((buffer[0] << 8) | buffer[1]);

// Then convert to physical units
float accel_g = value / 16384.0;  // For ±2g range
```

## FIFO Operation Summary

### Capacity
```
Total: 1024 bytes
Per sample: 14 bytes (accel 6 + temp 2 + gyro 6)
Max samples: 73
```

### Safe Reading Pattern
```
1. Read FIFO count (registers 0x72-0x73) as uint16_t
2. Validate count <= 1024
3. Calculate samples = count / 14
4. Validate samples <= buffer size
5. Read FIFO data in burst
```

### Enable FIFO
```
1. USER_CTRL = 0x84       // Reset FIFO
2. Wait 10ms
3. USER_CTRL = 0x44       // Enable FIFO
4. FIFO_EN = 0x78         // Enable sensors
5. INT_ENABLE = 0x15      // Enable overflow & data ready
```

## Interrupt Configuration

### Enable Data Ready Interrupt (Most Useful)
```c
INT_ENABLE = 0x01          // DATA_RDY_EN
INT_PIN_CFG = 0x80         // Latch mode, active low
```

### Read Interrupt Status
```c
uint8_t status;
i2c_read_register(0x3A, &status);
if (status & 0x01) {       // DATA_RDY
    // Read sensor data
}
if (status & 0x10) {       // FIFO_OFLOW
    // Reset FIFO
}
```

## Initialization Sequence

```
1. Power on, wait 100ms
2. Read WHO_AM_I (0x75) - should return 0x68
3. PWR_MGMT_1 = 0x00     // Wake up, use gyro clock
4. Wait 50ms
5. Configure CONFIG (DLPF)
6. Configure GYRO_CONFIG (range)
7. Configure ACCEL_CONFIG (range)
8. Configure SMPLRT_DIV (sample rate)
9. Optional: Enable FIFO
10. Optional: Enable interrupts
```

## Common Issues and Solutions

### Issue: Getting values 0-65535 instead of -2 to +2g
**Cause:** Interpreting as unsigned instead of signed
**Fix:** Cast to int16_t: `int16_t val = (int16_t)((buf[0] << 8) | buf[1]);`

### Issue: FIFO overflow constantly
**Cause:** Not reading FIFO fast enough or sample rate too high
**Fix:**
1. Lower sample rate or DLPF setting
2. Read FIFO more frequently
3. Reduce processing delays before FIFO read

### Issue: I2C timeout or bus hang
**Cause:** I2C timeout too short for clock stretching
**Fix:** Set I2C timeout to at least 200ms, lower clock to 100 kHz

### Issue: Temperature reads outside -40°C to +85°C
**Cause:** Sensor failure or I2C communication error
**Fix:** Validate data, check I2C connection, reset sensor

### Issue: All sensors read exactly 0
**Cause:** I2C communication failure
**Fix:**
1. Check I2C address (0x68 vs 0x69)
2. Check pull-up resistors (4.7K typical)
3. Verify SCL/SDA connections

### Issue: Accelerometer missing 1g gravity on Z-axis
**Cause:** Not accounting for gravitational acceleration
**Fix:** This is normal! Z-axis reads -16384 (at ±2g) when flat. Apply calibration offset.

## Memory Safety Checklist

```
[ ] FIFO count read as uint16_t (not uint8_t)
[ ] FIFO count validated < 1024 before reading
[ ] Sample count <= allocated buffer size
[ ] 16-bit values assembled with explicit int16_t cast
[ ] All register addresses validated before I2C access
[ ] I2C timeout >= 200ms
[ ] Burst read used for all 14-byte sensor reads
[ ] FIFO overflow interrupt enabled and handled
[ ] Buffer pointers always checked before write
[ ] Stack buffers sized appropriately (no 1KB arrays on stack)
```

## Typical Data Read Pattern

```c
typedef struct {
    int16_t accel[3];
    int16_t temp;
    int16_t gyro[3];
} mpu6050_data_t;

esp_err_t read_sensor_data(i2c_port_t port, mpu6050_data_t *data) {
    uint8_t buffer[14];

    // 1. Read all 14 bytes in single burst
    if (i2c_read_registers(port, 0x3B, 14, buffer) != ESP_OK) {
        return ESP_FAIL;
    }

    // 2. Parse as signed 16-bit values
    data->accel[0] = (int16_t)((buffer[0] << 8) | buffer[1]);
    data->accel[1] = (int16_t)((buffer[2] << 8) | buffer[3]);
    data->accel[2] = (int16_t)((buffer[4] << 8) | buffer[5]);
    data->temp = (int16_t)((buffer[6] << 8) | buffer[7]);
    data->gyro[0] = (int16_t)((buffer[8] << 8) | buffer[9]);
    data->gyro[1] = (int16_t)((buffer[10] << 8) | buffer[11]);
    data->gyro[2] = (int16_t)((buffer[12] << 8) | buffer[13]);

    // 3. Validate data
    if (data->temp < -26000 || data->temp > 16500) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}
```

## Key ESP32-IDF I2C Configuration

```c
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_21,
    .scl_io_num = GPIO_NUM_22,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,  // 400 kHz
};
i2c_param_config(I2C_NUM_0, &conf);
i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
i2c_set_timeout(I2C_NUM_0, 20000);  // 20ms timeout minimum
```

## References

- **Datasheet**: MPU-6000 and MPU-6050 Register Map, Rev 4.0, InvenSense/TDK
- **I2C Spec**: NXP I2C Bus Specification
- **ESP32-IDF**: https://docs.espressif.com/projects/esp-idf/
- **Reference Libraries**:
  - esp-idf-lib: https://github.com/espressif/esp-idf-lib
  - i2cdevlib: https://github.com/jrowberg/i2cdevlib
  - Adafruit: https://github.com/adafruit/Adafruit_MPU6050
