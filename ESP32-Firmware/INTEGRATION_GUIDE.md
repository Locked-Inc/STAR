# BNO055 + BMP280 Integration Guide for ESP32-IDF
## Practical Implementation for 10-DOF Sensor Fusion

---

## Critical Implementation Patterns

### 1. Quaternion Validation & Normalization

After any quaternion operation, MUST normalize to maintain precision:

```c
void bno055_quaternion_normalize(bno055_quaternion_t *quat) {
    float norm_sq = quat->w*quat->w + quat->x*quat->x +
                    quat->y*quat->y + quat->z*quat->z;

    if (norm_sq < 1e-10f) {
        quat->w = 1.0f; quat->x = 0.0f; quat->y = 0.0f; quat->z = 0.0f;
        return;
    }

    float inv_norm = 1.0f / sqrtf(norm_sq);
    quat->w *= inv_norm;
    quat->x *= inv_norm;
    quat->y *= inv_norm;
    quat->z *= inv_norm;
}
```

### 2. BMP280 Pressure Overflow Prevention

Validate all pressure measurements are in valid range (300-1100 hPa):

```c
#define BMP280_MIN_PRESSURE_HPA 300.0f
#define BMP280_MAX_PRESSURE_HPA 1100.0f

bool bmp280_validate_pressure(float pressure_hpa) {
    return (pressure_hpa >= BMP280_MIN_PRESSURE_HPA &&
            pressure_hpa <= BMP280_MAX_PRESSURE_HPA);
}
```

### 3. BNO055 Calibration Validation

NEVER trust orientation data when calibration status is unstable:

```c
bool bno055_is_calibration_safe(const bno055_calib_status_t *calib) {
    // All sensors must be >= level 2, AND NOT all at dangerous level 3
    return (calib->sys_calib >= 2 && calib->gyro_calib >= 2 &&
            calib->accel_calib >= 2 && calib->mag_calib >= 2 &&
            !calib->is_fully_calibrated);  // Avoid all 3s (firmware bug)
}
```

### 4. BNO055 Mode Transitions

ALWAYS use safe mode transition (go through CONFIG mode):

```c
esp_err_t bno055_safe_mode_transition(bno055_driver_handle_t *handle,
                                     bno055_operation_mode_t target_mode) {
    // Get current mode
    bno055_operation_mode_t current = bno055_read_opr_mode(handle);
    if (current == target_mode) return ESP_OK;

    // Go to CONFIG mode first
    if (current != BNO055_MODE_CONFIGMODE) {
        bno055_write_register(handle, BNO055_OPR_MODE_ADDR,
                             BNO055_MODE_CONFIGMODE);
        vTaskDelay(pdMS_TO_TICKS(25));
    }

    // Then to target mode
    bno055_write_register(handle, BNO055_OPR_MODE_ADDR, target_mode);
    vTaskDelay(pdMS_TO_TICKS(25));

    return ESP_OK;
}
```

### 5. Synchronized Multi-Sensor Read

```c
typedef struct {
    bno055_quaternion_t quat;
    float temperature_c;
    float pressure_hpa;
    float altitude_m;
    uint64_t timestamp_us;
    bool bno_valid;
    bool bmp_valid;
} synchronized_sensor_data_t;

esp_err_t read_synchronized_snapshot(synchronized_sensor_data_t *snap) {
    memset(snap, 0, sizeof(*snap));
    snap->timestamp_us = esp_timer_get_time();

    if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Read BNO055
    snap->bno_valid = (bno055_read_quaternion(&bno055_handle, 
                                              &snap->quat) == ESP_OK);

    // Read BMP280 (TEMPERATURE FIRST, then PRESSURE)
    int32_t t_fine = 0;
    float temp = 0, pressure_pa = 0, pressure_hpa = 0;
    
    if (bmp280_read_temperature(&bmp280_handle, &temp, &t_fine) == ESP_OK) {
        snap->temperature_c = temp;
        if (bmp280_read_pressure(&bmp280_handle, t_fine, 
                                 &pressure_pa, &pressure_hpa) == ESP_OK) {
            snap->pressure_hpa = pressure_hpa;
            snap->altitude_m = bmp280_calculate_altitude_safe(
                pressure_hpa, reference_sea_level_pressure);
            snap->bmp_valid = true;
        }
    }

    xSemaphoreGive(i2c_mutex);

    return (snap->bno_valid && snap->bmp_valid) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
```

---

## Known Hardware Issues & Workarounds

### BNO055 Firmware Bugs

1. **Euler Angle Distortion** (>20-30° from horizontal)
   - Workaround: Use quaternions, convert to Euler locally

2. **3/3 Calibration Instability** (all sensors at level 3)
   - Workaround: Monitor calib_stat, maintain at 2/3 level

3. **Calibration Offset Overwrite**
   - Workaround: Load calibration after entering operational mode

### BMP280 Known Issues

1. **Pressure Read During Measurement**
   - Workaround: Check STATUS.measuring bit before reading

2. **Temperature Must Precede Pressure**
   - Workaround: Always read temperature first for compensation

3. **Altitude Error Without Accurate Reference Pressure**
   - Workaround: Obtain daily sea-level pressure from weather station

---

## Hardware Configuration

```
ESP32 GPIO Configuration:
- GPIO 21 (SDA) connected to both BNO055 and BMP280 SDA
- GPIO 22 (SCL) connected to both BNO055 and BMP280 SCL
- 4.7kΩ pull-up resistors on SDA and SCL
- BNO055 address: 0x29 (COM3 to 3.3V)
- BMP280 address: 0x77 (SDO to 3.3V)
- I2C clock: 400 kHz (Fast mode)
```

---

## Performance Summary

| Metric | Value |
|--------|-------|
| BNO055 read time | 2-3 ms @ 400 kHz |
| BMP280 read time | 1-2 ms @ 400 kHz |
| Total I2C transaction | 5-10 ms |
| BNO055 latency | 100-200 ms (includes fusion) |
| BMP280 latency | 10-20 ms (depends on oversampling) |
| Recommended BNO055 rate | 50 Hz (20 ms) |
| Recommended BMP280 rate | 20 Hz (50 ms) |
| Recommended fusion rate | 10 Hz (100 ms) |

---

## File References

All implementation files located in:
`C:/Users/sikar/CLionProjects/untitled/`

1. **BNO055_BMP280_TECHNICAL_REFERENCE.md** (66 KB)
   - Complete register maps, I2C protocols, algorithms
   - Security considerations and validation patterns
   
2. **bno055_driver.h** (16 KB)
   - Full function declarations
   - Register definitions and constants
   
3. **bmp280_driver.h** (15 KB)
   - Full function declarations
   - Register definitions and constants
   
4. **INTEGRATION_GUIDE.md** (this file)
   - Practical implementation patterns

---

## Testing Checklist

- [ ] I2C bus scan finds both devices (0x29 and 0x77)
- [ ] BNO055 CHIP_ID reads as 0xA0
- [ ] BMP280 CHIP_ID reads as 0x58
- [ ] BNO055 enters NDOF mode successfully
- [ ] BMP280 enters NORMAL mode successfully
- [ ] BNO055 calibration reaches stable state (< 60 seconds)
- [ ] Quaternion magnitude between 0.99-1.01
- [ ] BMP280 pressure readings 300-1100 hPa range
- [ ] Altitude calculations within ±50m of known value
- [ ] I2C bus survives repeated start/stop cycles
- [ ] Continuous operation for 1+ hour error-free
- [ ] Memory usage remains stable

