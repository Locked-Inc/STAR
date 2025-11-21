# QMC5883L Configuration Guide for ESP32-IDF

**Purpose:** Practical configuration examples for common use cases
**Target:** Application developers working with QMC5883L sensors

---

## Table of Contents

1. [Configuration Presets](#configuration-presets)
2. [Power Consumption Analysis](#power-consumption-analysis)
3. [Use Case Implementations](#use-case-implementations)
4. [Calibration Recipes](#calibration-recipes)
5. [Troubleshooting Guide](#troubleshooting-guide)

---

## Configuration Presets

### Preset 1: Navigation/Compass (Recommended Default)

**Use Case:** Digital compass applications, heading indicators, navigation

```c
const qmc5883l_config_t CONFIG_NAVIGATION = {
    .mode = QMC5883L_MODE_CONTINUOUS,      /* Always measuring */
    .odr = QMC5883L_ODR_10HZ,              /* 10Hz - sufficient for compass */
    .range = QMC5883L_RNG_2G,              /* Max sensitivity for compass */
    .osr = QMC5883L_OSR_512,               /* Minimum noise */
    .enable_interrupt = true,              /* DRDY for synchronization */
    .enable_pointer_rollover = true,       /* Enable auto-increment */
};

/**
 * Description:
 * - 10Hz ODR provides smooth heading updates without excessive CPU usage
 * - ±2G range has maximum sensitivity (~1 mG/LSB) for compass accuracy
 * - OSR=512 minimizes noise in heading calculations
 * - Power: ~400 µA average
 * - Startup: < 100ms
 */
```

### Preset 2: Gaming/Motion Tracking

**Use Case:** VR controllers, motion detection, gesture recognition

```c
const qmc5883l_config_t CONFIG_GAMING = {
    .mode = QMC5883L_MODE_CONTINUOUS,      /* Continuous for real-time response */
    .odr = QMC5883L_ODR_100HZ,             /* 100Hz for responsive tracking */
    .range = QMC5883L_RNG_8G,              /* More robust to interference */
    .osr = QMC5883L_OSR_64,                /* Fastest response (lowest latency) */
    .enable_interrupt = true,              /* DRDY for precise timing */
    .enable_pointer_rollover = true,
};

/**
 * Description:
 * - 100Hz provides ~10ms latency, suitable for real-time gaming
 * - ±8G range tolerates local electromagnetic interference better
 * - OSR=64 minimizes measurement time (fastest response)
 * - Power: ~600 µA average
 * - Startup: < 50ms
 * - Note: Requires calibration for accuracy due to larger range
 */
```

### Preset 3: Low Power / Battery

**Use Case:** IoT devices, battery-powered applications, long-term operation

```c
const qmc5883l_config_t CONFIG_LOW_POWER = {
    .mode = QMC5883L_MODE_SINGLE,          /* Single-shot mode */
    .odr = QMC5883L_ODR_10HZ,              /* Ignored in single-shot (for reference) */
    .range = QMC5883L_RNG_2G,              /* Max sensitivity when triggered */
    .osr = QMC5883L_OSR_256,               /* Balance noise vs speed */
    .enable_interrupt = true,              /* Wake on measurement complete */
    .enable_pointer_rollover = true,
};

/**
 * Description:
 * - Single-shot mode only measures on demand (~5ms per measurement)
 * - Minimal standby power: < 1 µA between measurements
 * - Application manually triggers measurement when needed
 * - Power: 100-200 µA average (depends on trigger frequency)
 * - Startup: < 10ms per measurement
 * - Battery life: Can achieve months/years on button cell
 */
```

### Preset 4: High Performance / Research

**Use Case:** Scientific measurements, data logging, detailed analysis

```c
const qmc5883l_config_t CONFIG_HIGH_PERFORMANCE = {
    .mode = QMC5883L_MODE_CONTINUOUS,      /* Continuous stream */
    .odr = QMC5883L_ODR_200HZ,             /* Maximum update rate */
    .range = QMC5883L_RNG_2G,              /* Maximum sensitivity */
    .osr = QMC5883L_OSR_512,               /* Maximum noise filtering */
    .enable_interrupt = true,              /* Precise timing via DRDY */
    .enable_pointer_rollover = true,
};

/**
 * Description:
 * - 200Hz provides detailed magnetic field waveform capture
 * - OSR=512 provides best noise performance despite high ODR
 * - Requires robust power supply (peak current ~1mA)
 * - Power: ~800 µA average
 * - Startup: < 100ms
 * - Data rate: 200 measurements/sec × 6 bytes = 1.2 kB/sec
 * - Note: Requires careful heat dissipation and power management
 */
```

### Preset 5: Environmental Robustness

**Use Case:** Outdoor applications, strong magnetic field environments

```c
const qmc5883l_config_t CONFIG_ROBUST = {
    .mode = QMC5883L_MODE_CONTINUOUS,      /* Continuous for stability */
    .odr = QMC5883L_ODR_50HZ,              /* Moderate update rate */
    .range = QMC5883L_RNG_8G,              /* Wider range, less susceptible to interference */
    .osr = QMC5883L_OSR_256,               /* Good noise filtering */
    .enable_interrupt = true,
    .enable_pointer_rollover = true,
};

/**
 * Description:
 * - ±8G range provides saturation margin in strong fields
 * - 50Hz updates sufficient for most applications
 * - OSR=256 balances noise and response time
 * - Power: ~450 µA average
 * - Best for environments with permanent magnets nearby
 */
```

---

## Power Consumption Analysis

### Typical Power Profiles

```
Configuration       | Mode       | ODR    | OSR | Power (µA) | Notes
────────────────────────────────────────────────────────────────────
Standby (off)       | Standby    | -      | -   | 0.5-1.0    | Only leakage
Navigation          | Continuous | 10Hz   | 512 | 400        | Recommended
Gaming              | Continuous | 100Hz  | 64  | 600        | Real-time
Low-Power (idle)    | Standby    | -      | -   | 1.0        | Waiting for trigger
Low-Power (measure) | Single     | N/A    | 256 | 2500       | During measurement
High-Performance    | Continuous | 200Hz  | 512 | 800        | Maximum load
```

### Power Optimization Strategies

#### Strategy 1: Duty-Cycled Measurement
```c
/**
 * Measure every 10 seconds, 100Hz for 1 second
 * Total average power: ~0.5 µA (mostly standby)
 */
void power_optimized_task(void *arg)
{
    qmc5883l_device_t *dev = (qmc5883l_device_t *)arg;

    while (1) {
        /* Sleep for 9 seconds */
        vTaskDelay(pdMS_TO_TICKS(9000));

        /* Switch to continuous 100Hz */
        qmc5883l_config_t cfg = CONFIG_GAMING;
        qmc5883l_apply_config(dev, &cfg);

        /* Collect 100 measurements (1 second at 100Hz) */
        for (int i = 0; i < 100; i++) {
            int16_t x, y, z;
            qmc5883l_read_thread_safe(dev, &x, &y, &z);
            process_measurement(x, y, z);
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        /* Switch back to standby */
        qmc5883l_set_mode(dev, QMC5883L_MODE_STANDBY);
    }
}

/**
 * Power calculation:
 * 9 seconds @ 1 µA (standby) = 9 µAs
 * 1 second @ 600 µA (100Hz) = 600 µAs
 * 10 second cycle = 60.9 µAs average = 6.09 µA average current
 * For 1000 mAh battery: ~6,500 hours = 270+ days operation
 */
```

#### Strategy 2: Data-Driven Wakeup
```c
/**
 * Use GPIO interrupt to trigger measurements only when needed
 * E.g., motion detected on external sensor
 */
void motion_triggered_measurement(void *arg)
{
    /* Waiting in standby (< 1 µA) */
    while (1) {
        /* Wait for GPIO interrupt from motion sensor */
        uint32_t motion_notification;
        xQueueReceive(g_motion_queue, &motion_notification,
                     pdMS_TO_TICKS(UINT32_MAX));

        /* Motion detected - take measurements */
        qmc5883l_config_t cfg = CONFIG_NAVIGATION;
        qmc5883l_apply_config(dev, &cfg);

        int16_t x, y, z;
        qmc5883l_read_thread_safe(dev, &x, &y, &z);
        process_heading(x, y, z);

        /* Return to standby */
        qmc5883l_set_mode(dev, QMC5883L_MODE_STANDBY);
    }
}
```

---

## Use Case Implementations

### Use Case 1: Digital Compass with Heading Display

```c
/**
 * Complete digital compass application
 */

typedef struct {
    float heading;          /* Degrees 0-360 */
    float declination;      /* Local magnetic declination */
    float last_heading;
    uint32_t update_count;
} compass_state_t;

static compass_state_t g_compass = {
    .declination = 5.0f,    /* Example: London is ~5° west */
};

/**
 * Initialize compass with recommended configuration
 */
esp_err_t compass_init(qmc5883l_device_t *dev)
{
    /* Apply navigation preset */
    esp_err_t ret = qmc5883l_apply_config(dev, &CONFIG_NAVIGATION);
    if (ret != ESP_OK) {
        ESP_LOGE("COMPASS", "Failed to apply configuration");
        return ret;
    }

    /* Run calibration */
    ESP_LOGI("COMPASS", "Starting figure-8 calibration...");
    qmc5883l_calibration_t calib;
    ret = qmc5883l_figure_8_calibration(dev, 10, &calib);  /* 10 seconds */
    if (ret != ESP_OK) {
        ESP_LOGW("COMPASS", "Calibration failed");
        return ret;
    }

    ESP_LOGI("COMPASS", "Calibration complete: X offset=%d, Y offset=%d",
             calib.x_offset, calib.y_offset);

    return ESP_OK;
}

/**
 * Update heading calculation
 */
esp_err_t compass_update(qmc5883l_device_t *dev, const qmc5883l_calibration_t *calib)
{
    int16_t raw_x, raw_y, raw_z;

    /* Read raw measurements */
    esp_err_t ret = qmc5883l_read_thread_safe(dev, &raw_x, &raw_y, &raw_z);
    if (ret != ESP_OK) {
        ESP_LOGE("COMPASS", "Read failed: 0x%X", ret);
        return ret;
    }

    /* Apply calibration */
    float x_cal = (raw_x - calib->x_offset) * calib->x_scale;
    float y_cal = (raw_y - calib->y_offset) * calib->y_scale;

    /* Convert to physical units (Gauss) */
    float x_gauss = x_cal * QMC5883L_SENS_2G_MG_PER_LSB / 1000.0f;
    float y_gauss = y_cal * QMC5883L_SENS_2G_MG_PER_LSB / 1000.0f;

    /* Calculate heading (0-360 degrees) */
    float heading = atan2f(y_gauss, x_gauss) * 180.0f / M_PI;
    if (heading < 0.0f) {
        heading += 360.0f;
    }

    /* Apply declination correction */
    heading += g_compass.declination;
    if (heading >= 360.0f) {
        heading -= 360.0f;
    }

    /* Exponential smoothing filter (hysteresis) */
    const float ALPHA = 0.1f;  /* Smoothing factor */
    g_compass.heading = ALPHA * heading + (1.0f - ALPHA) * g_compass.last_heading;
    g_compass.last_heading = g_compass.heading;
    g_compass.update_count++;

    return ESP_OK;
}

/**
 * Display compass heading
 */
void compass_display(void)
{
    const char *direction;

    if (g_compass.heading < 22.5f || g_compass.heading >= 337.5f) {
        direction = "N";
    } else if (g_compass.heading < 67.5f) {
        direction = "NE";
    } else if (g_compass.heading < 112.5f) {
        direction = "E";
    } else if (g_compass.heading < 157.5f) {
        direction = "SE";
    } else if (g_compass.heading < 202.5f) {
        direction = "S";
    } else if (g_compass.heading < 247.5f) {
        direction = "SW";
    } else if (g_compass.heading < 292.5f) {
        direction = "W";
    } else {
        direction = "NW";
    }

    printf("Heading: %.1f° (%s) [Updates: %lu]\n",
           g_compass.heading, direction, g_compass.update_count);
}

/**
 * Main compass task
 */
void compass_task(void *arg)
{
    qmc5883l_device_t *dev = (qmc5883l_device_t *)arg;

    /* Initialize */
    if (compass_init(dev) != ESP_OK) {
        ESP_LOGE("COMPASS", "Initialization failed");
        vTaskDelete(NULL);
    }

    /* Calibration data (would be loaded from NVS in production) */
    qmc5883l_calibration_t calib = {
        .x_offset = 0,
        .y_offset = 0,
        .z_offset = 0,
        .x_scale = 1.0f,
        .y_scale = 1.0f,
        .z_scale = 1.0f,
    };

    /* Main loop */
    while (1) {
        if (compass_update(dev, &calib) == ESP_OK) {
            compass_display();
        }

        vTaskDelay(pdMS_TO_TICKS(100));  /* 10Hz display update */
    }
}
```

### Use Case 2: Motion Detection with Low Power

```c
/**
 * Motion detection system
 * Measures when motion sensor triggers
 */

typedef struct {
    uint32_t motion_count;
    uint32_t last_motion_time;
    uint32_t total_measurement_time;
} motion_detector_state_t;

static motion_detector_state_t g_motion_state = {0};

/**
 * Setup low-power measurement on demand
 */
void motion_detector_init(qmc5883l_device_t *dev)
{
    /* Apply low-power preset */
    qmc5883l_apply_config(dev, &CONFIG_LOW_POWER);

    ESP_LOGI("MOTION", "Motion detector initialized (single-shot mode)");
}

/**
 * Called when external motion sensor triggers
 */
void on_motion_detected(void)
{
    g_motion_state.motion_count++;
    g_motion_state.last_motion_time = xTaskGetTickCount();

    ESP_LOGI("MOTION", "Motion event #%lu", g_motion_state.motion_count);
}

/**
 * Measurement task - triggered by motion
 */
void motion_measurement_task(void *arg)
{
    qmc5883l_device_t *dev = (qmc5883l_device_t *)arg;
    motion_detector_init(dev);

    uint32_t last_processed_count = 0;

    while (1) {
        /* Check if new motion event */
        if (g_motion_state.motion_count > last_processed_count) {
            uint32_t start_time = xTaskGetTickCount();

            /* Trigger single measurement */
            int16_t x, y, z;
            esp_err_t ret = qmc5883l_read_thread_safe(dev, &x, &y, &z);

            uint32_t elapsed = xTaskGetTickCount() - start_time;
            g_motion_state.total_measurement_time += elapsed;

            if (ret == ESP_OK) {
                float magnitude = sqrtf(x*x + y*y + z*z);
                float heading = atan2f(y, x) * 180.0f / M_PI;

                printf("Motion %lu: X=%6d, Y=%6d, Z=%6d, Mag=%.1f mG, Heading=%.1f°\n",
                       g_motion_state.motion_count, x, y, z, magnitude / 1000.0f, heading);
            }

            last_processed_count = g_motion_state.motion_count;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

---

## Calibration Recipes

### Recipe 1: Quick 2-Point Calibration

**Time:** < 1 minute, Accuracy: Moderate

```c
/**
 * Quick calibration - point to North and South only
 */
esp_err_t qmc5883l_quick_calibration(qmc5883l_device_t *dev,
                                     qmc5883l_calibration_t *calib)
{
    printf("Quick Calibration - Point North then South\n");
    printf("Press ENTER when ready for North calibration...\n");

    int16_t north_x, north_y, north_z;
    int16_t south_x, south_y, south_z;

    /* Capture North reading */
    for (int i = 0; i < 100; i++) {
        qmc5883l_read_thread_safe(dev, &north_x, &north_y, &north_z);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    printf("North: X=%d, Y=%d, Z=%d\n", north_x, north_y, north_z);

    printf("Now point South. Press ENTER when ready...\n");

    /* Capture South reading */
    for (int i = 0; i < 100; i++) {
        qmc5883l_read_thread_safe(dev, &south_x, &south_y, &south_z);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    printf("South: X=%d, Y=%d, Z=%d\n", south_x, south_y, south_z);

    /* Calculate offsets (midpoint between North and South) */
    calib->x_offset = (north_x + south_x) / 2;
    calib->y_offset = (north_y + south_y) / 2;
    calib->z_offset = (north_z + south_z) / 2;

    /* Calculate scale (both directions should have same magnitude) */
    int16_t north_range = north_x - calib->x_offset;
    int16_t south_range = -(south_x - calib->x_offset);
    calib->x_scale = (north_range + south_range) / 2.0f / north_range;

    calib->y_scale = 1.0f;  /* Simplified */
    calib->z_scale = 1.0f;

    printf("Calibration: X_offset=%d, Y_offset=%d\n",
           calib->x_offset, calib->y_offset);

    return ESP_OK;
}
```

### Recipe 2: Manual Figure-8 Calibration

**Time:** 1-2 minutes, Accuracy: High

```c
/**
 * User performs figure-8 motion while collecting min/max
 * Most practical method for field use
 */
esp_err_t qmc5883l_manual_figure8_calibration(qmc5883l_device_t *dev,
                                              qmc5883l_calibration_t *calib)
{
    printf("\n\n");
    printf("========== MAGNETOMETER CALIBRATION ==========\n");
    printf("\nHold device at arm's length and perform a FIGURE-8 motion\n");
    printf("in all three planes (front-to-back, side-to-side, up-and-down)\n");
    printf("\nCalibration in progress");

    int16_t x_min = INT16_MAX, x_max = INT16_MIN;
    int16_t y_min = INT16_MAX, y_max = INT16_MIN;
    int16_t z_min = INT16_MAX, z_max = INT16_MIN;

    /* Collect for 60 seconds */
    uint32_t samples = 0;
    uint32_t start_time = xTaskGetTickCount();

    while (xTaskGetTickCount() - start_time < pdMS_TO_TICKS(60000)) {
        int16_t x, y, z;

        if (qmc5883l_read_thread_safe(dev, &x, &y, &z) == ESP_OK) {
            if (x < x_min) x_min = x;
            if (x > x_max) x_max = x;
            if (y < y_min) y_min = y;
            if (y > y_max) y_max = y;
            if (z < z_min) z_min = z;
            if (z > z_max) z_max = z;

            samples++;

            if (samples % 500 == 0) {
                printf(".");
                fflush(stdout);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));  /* 50Hz sampling */
    }

    printf("\nCalibration Complete\n");
    printf("\nCollected %lu samples\n", samples);

    /* Calculate offsets */
    calib->x_offset = (x_min + x_max) / 2;
    calib->y_offset = (y_min + y_max) / 2;
    calib->z_offset = (z_min + z_max) / 2;

    /* Calculate scale (normalize to largest range) */
    int16_t range_x = x_max - x_min;
    int16_t range_y = y_max - y_min;
    int16_t range_z = z_max - z_min;

    int16_t max_range = (range_x > range_y) ?
                        ((range_x > range_z) ? range_x : range_z) :
                        ((range_y > range_z) ? range_y : range_z);

    if (max_range > 0) {
        calib->x_scale = max_range / (float)range_x;
        calib->y_scale = max_range / (float)range_y;
        calib->z_scale = max_range / (float)range_z;
    }

    printf("\nCalibration Results:\n");
    printf("  X Range: %d to %d (offset=%d, scale=%.3f)\n",
           x_min, x_max, calib->x_offset, calib->x_scale);
    printf("  Y Range: %d to %d (offset=%d, scale=%.3f)\n",
           y_min, y_max, calib->y_offset, calib->y_scale);
    printf("  Z Range: %d to %d (offset=%d, scale=%.3f)\n",
           z_min, z_max, calib->z_offset, calib->z_scale);

    printf("\nCalibration accuracy: %.1f%%\n",
           100.0f * (min(calib->x_scale, min(calib->y_scale, calib->z_scale)));

    return ESP_OK;
}
```

### Recipe 3: Automatic Soft-Iron Correction

```c
/**
 * Compute soft-iron correction matrix
 * Compensates for magnetic distortion from nearby metals
 */
typedef struct {
    /* 3x3 correction matrix */
    float M[3][3];
    int16_t offset[3];
} soft_iron_calibration_t;

esp_err_t qmc5883l_compute_soft_iron(qmc5883l_device_t *dev,
                                     soft_iron_calibration_t *correction)
{
    /* Simplified: Collect min/max during figure-8 */
    int16_t readings[3][100];
    int count = 0;

    /* Collect 100 samples */
    for (int i = 0; i < 100; i++) {
        int16_t x, y, z;
        qmc5883l_read_thread_safe(dev, &x, &y, &z);

        readings[0][i] = x;
        readings[1][i] = y;
        readings[2][i] = z;

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Compute correlation matrix and eigenvalues
     * (Full implementation would use LAPACK or similar)
     */

    /* For now, simple diagonal scale matrix */
    correction->M[0][0] = 1.0f;
    correction->M[1][1] = 1.0f;
    correction->M[2][2] = 1.0f;

    return ESP_OK;
}
```

---

## Troubleshooting Guide

### Problem 1: Constant Readings

**Symptoms:**
- Same X, Y, Z values every read
- Usually 0x0000 or specific value

**Cause:** Sensor frozen, no new measurements

**Solutions:**

```c
void debug_constant_readings(qmc5883l_device_t *dev)
{
    ESP_LOGE("DEBUG", "Troubleshooting constant readings...");

    /* Test 1: Check DRDY flag */
    uint8_t status;
    i2c_read_byte(dev->port, dev->address, 0x06, &status);
    if (!(status & 0x01)) {
        ESP_LOGE("DEBUG", "DRDY flag not set - sensor not updating!");
        ESP_LOGI("DEBUG", "Check: Is sensor in continuous mode?");
        return;
    }

    /* Test 2: Perform soft reset */
    ESP_LOGI("DEBUG", "Attempting soft reset...");
    i2c_write_byte(dev->port, dev->address, 0x0A, 0x80);
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read again */
    int16_t x, y, z;
    qmc5883l_read_thread_safe(dev, &x, &y, &z);
    ESP_LOGI("DEBUG", "After reset: X=%d, Y=%d, Z=%d", x, y, z);

    /* Test 3: Check control registers */
    uint8_t ctrl1, ctrl2;
    i2c_read_byte(dev->port, dev->address, 0x09, &ctrl1);
    i2c_read_byte(dev->port, dev->address, 0x0A, &ctrl2);
    ESP_LOGI("DEBUG", "CONTROL_1=0x%02X, CONTROL_2=0x%02X", ctrl1, ctrl2);
}
```

### Problem 2: Erratic/Noisy Readings

**Symptoms:**
- Large jumps between consecutive readings
- Value changes wildly without physical motion

**Cause:** Electromagnetic interference, poor calibration

**Solutions:**

```c
void debug_noisy_readings(qmc5883l_device_t *dev)
{
    printf("Analyzing noise pattern...\n");

    /* Collect 100 readings */
    int16_t readings[100];
    for (int i = 0; i < 100; i++) {
        int16_t x, y, z;
        qmc5883l_read_thread_safe(dev, &x, &y, &z);
        readings[i] = x;  /* Analyze X-axis */
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    /* Calculate statistics */
    int32_t sum = 0, min = readings[0], max = readings[0];
    for (int i = 0; i < 100; i++) {
        sum += readings[i];
        if (readings[i] < min) min = readings[i];
        if (readings[i] > max) max = readings[i];
    }

    int16_t mean = sum / 100;
    int32_t variance = 0;
    for (int i = 0; i < 100; i++) {
        int32_t diff = readings[i] - mean;
        variance += diff * diff;
    }
    variance /= 100;
    float std_dev = sqrtf(variance);

    printf("X-axis statistics:\n");
    printf("  Mean: %d\n", mean);
    printf("  Range: %d to %d (span: %d)\n", min, max, max - min);
    printf("  Std Dev: %.1f\n", std_dev);

    if (std_dev > 100) {
        printf("\nERROR: High noise detected (std_dev > 100)\n");
        printf("Possible causes:\n");
        printf("  1. Nearby power cables or electronics\n");
        printf("  2. Magnetic materials (ferrite, magnets)\n");
        printf("  3. Poor power supply (noisy, dirty 3.3V)\n");
        printf("  4. Loose I2C connections\n");
        printf("\nSolutions:\n");
        printf("  - Check power supply quality\n");
        printf("  - Shield sensor from interference\n");
        printf("  - Check all electrical connections\n");
    }
}
```

### Problem 3: Wrong Heading Direction

**Symptoms:**
- Compass points opposite direction
- Heading 180° off from expected

**Cause:** X/Y axis swapped or inverted in calibration

**Solutions:**

```c
void debug_heading_direction(qmc5883l_device_t *dev)
{
    printf("Heading Direction Test\n");
    printf("Point toward North\n");

    int16_t x, y, z;
    qmc5883l_read_thread_safe(dev, &x, &y, &z);

    float heading = atan2f(y, x) * 180.0f / M_PI;
    if (heading < 0) heading += 360.0f;

    printf("Pointing North, reading: X=%d, Y=%d\n", x, y);
    printf("Calculated heading: %.1f°\n", heading);

    if (heading > 180) {
        printf("ERROR: Heading is 180° off!\n");
        printf("Solution: Negate X axis or swap X/Y axes\n");
    } else if (heading > 45 && heading < 135) {
        printf("ERROR: Heading points East instead of North\n");
        printf("Solution: Swap X and Y axes\n");
    }
}
```

---

**Document Complete**

This guide provides practical configurations and troubleshooting procedures for successful QMC5883L deployment in various applications.
