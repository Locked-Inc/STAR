# IMX219-83 Stereo Camera - Quick Start Guide for ESP32-IDF

## File Overview

This package contains a complete, production-ready driver for the IMX219-83 dual 8MP stereo camera module on ESP32-IDF.

### Files Included

| File | Purpose | Lines |
|------|---------|-------|
| `imx219_stereo.h` | Header with API definitions | 400+ |
| `imx219_stereo.c` | Secure implementation | 800+ |
| `IMX219_83_TECHNICAL_REFERENCE.md` | Complete technical specifications | 2000+ |
| `IMX219_SECURITY_GUIDE.md` | Security hardening guide | 1000+ |
| `IMX219_QUICKSTART.md` | This file |  |

---

## Quick Integration Steps

### Step 1: Add Files to Your Project

```bash
# Copy files to your ESP32-IDF components directory
cp imx219_stereo.h /path/to/project/components/imx219/include/
cp imx219_stereo.c /path/to/project/components/imx219/
```

### Step 2: Create Component Structure

```bash
mkdir -p components/imx219/include
mkdir -p components/imx219/src

# Create CMakeLists.txt
cat > components/imx219/CMakeLists.txt << 'EOF'
idf_component_register(
    SRCS "imx219_stereo.c"
    INCLUDE_DIRS "include"
    REQUIRES driver i2c esp_camera
)
EOF
```

### Step 3: Update Main Application

```c
#include "imx219_stereo.h"
#include "esp_log.h"

static const char *TAG = "APP";
static imx219_driver_t g_camera_driver = {0};

void app_main(void) {
    /* 1. Initialize I2C */
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, i2c_conf.mode, 0, 0, 0);

    /* 2. Initialize camera */
    if (imx219_stereo_init(&g_camera_driver, I2C_NUM_0, GPIO_NUM_5) != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed");
        return;
    }

    /* 3. Verify sensor presence */
    if (imx219_verify_chip_id(&g_camera_driver) != ESP_OK) {
        ESP_LOGE(TAG, "Sensor not detected");
        return;
    }

    /* 4. Set resolution */
    const imx219_mode_t *mode = imx219_get_mode(IMX219_RES_1080P);
    if (imx219_set_mode(I2C_NUM_0, mode) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set resolution");
        return;
    }

    /* 5. Configure exposure and gain */
    imx219_set_exposure(I2C_NUM_0, 1000);  /* 1000 lines */
    imx219_set_analog_gain(I2C_NUM_0, 2.0f);  /* 2x */

    /* 6. Start capturing */
    if (imx219_start_streaming(&g_camera_driver) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start streaming");
        return;
    }

    ESP_LOGI(TAG, "Camera ready!");

    /* Main loop */
    while (1) {
        /* Capture and process frames */
        vTaskDelay(33 / portTICK_PERIOD_MS);  /* ~30fps */
    }
}
```

---

## Common Use Cases

### Use Case 1: Single Resolution Capture at Fixed Exposure

```c
esp_err_t capture_fixed_exposure(imx219_driver_t *driver) {
    /* Stop any current capture */
    imx219_stop_streaming(driver);

    /* Set to 1920x1080 @ 30fps */
    const imx219_mode_t *mode = imx219_get_mode(IMX219_RES_1080P);
    ESP_RETURN_ON_ERROR(
        imx219_set_mode(driver->i2c_port, mode),
        TAG, "Failed to set 1080p mode"
    );

    /* Fixed settings: exposure=500 lines, gain=1.5x */
    ESP_RETURN_ON_ERROR(
        imx219_set_exposure(driver->i2c_port, 500),
        TAG, "Failed to set exposure"
    );

    ESP_RETURN_ON_ERROR(
        imx219_set_analog_gain(driver->i2c_port, 1.5f),
        TAG, "Failed to set gain"
    );

    /* Start streaming */
    return imx219_start_streaming(driver);
}
```

### Use Case 2: Automatic Exposure Control

```c
esp_err_t enable_auto_exposure(imx219_driver_t *driver) {
    ae_config_t ae = {0};

    /* Initialize AE for 30fps capture */
    ae_initialize(&ae, 30);

    /* Main AE loop (would run in separate task) */
    uint8_t current_brightness = 100;  /* Read from image histogram */
    uint16_t exposure_adjust = 0;

    ae_compute_exposure(&ae, current_brightness, &exposure_adjust);

    /* Apply adjusted exposure */
    return imx219_set_exposure(driver->i2c_port, exposure_adjust);
}
```

### Use Case 3: Stereo Depth Calculation

```c
esp_err_t compute_stereo_depth(imx219_driver_t *driver,
                              uint8_t *left_image,
                              uint8_t *right_image) {
    /* Allocate disparity map */
    stereo_disparity_map_t disparity = {0};
    ESP_RETURN_ON_ERROR(
        allocate_disparity_map(&disparity, 1920, 1080),
        TAG, "Failed to allocate disparity map"
    );

    /* Compute disparity using block matching */
    ESP_RETURN_ON_ERROR(
        compute_disparity_block_match(left_image, right_image,
                                      1920, 1080, 7, &disparity),
        TAG, "Failed to compute disparity"
    );

    /* Filter to valid depth range (500mm - 5000mm) */
    ESP_RETURN_ON_ERROR(
        filter_disparity_map(&disparity, 500.0f, 5000.0f, &driver->calib),
        TAG, "Failed to filter disparity"
    );

    /* Calculate depth at pixel (960, 540) */
    uint16_t disp_value = disparity.disparity_map[540 * 1920 + 960];
    float depth_mm = calculate_depth_mm(&driver->calib, (float)disp_value);

    ESP_LOGI(TAG, "Depth at center: %.1f mm", depth_mm);

    free_disparity_map(&disparity);
    return ESP_OK;
}
```

### Use Case 4: Buffer Management for Image Processing

```c
esp_err_t process_image_data(uint16_t width, uint16_t height) {
    image_buffer_t buf = {0};

    /* Allocate image buffer with safety checks */
    ESP_RETURN_ON_ERROR(
        image_buffer_allocate(&buf, width, height, 2),  /* 16-bit data */
        TAG, "Buffer allocation failed"
    );

    /* Process image with bounds checking */
    for (uint16_t y = 0; y < buf.height; y++) {
        uint32_t row_offset = y * buf.stride;

        ESP_RETURN_ON_ERROR(
            image_buffer_validate_access(&buf, row_offset, buf.stride),
            TAG, "Invalid row access"
        );

        /* Safe to process row */
        for (uint16_t x = 0; x < buf.width; x++) {
            uint32_t offset = row_offset + (x * 2);
            uint16_t pixel = *(uint16_t *)(buf.buffer + offset);
            /* Process pixel... */
        }
    }

    image_buffer_free(&buf);
    return ESP_OK;
}
```

---

## Troubleshooting

### Issue: Camera Not Detected

**Symptom**: `imx219_verify_chip_id()` fails

**Solutions**:
1. Check I2C bus:
   ```c
   uint8_t data;
   esp_err_t ret = i2c_master_read_from_slave(I2C_NUM_0, IMX219_I2C_ADDR, &data, 1);
   ESP_LOGI(TAG, "I2C read result: %d", ret);
   ```

2. Check reset GPIO is toggling:
   ```c
   gpio_set_level(GPIO_NUM_5, 0);
   esp_rom_delay_us(100000);
   gpio_set_level(GPIO_NUM_5, 1);
   ```

3. Verify I2C clock speed (should be 400kHz or lower)

### Issue: Distorted Images

**Symptom**: Images appear compressed or with artifacts

**Solutions**:
1. Verify CSI-2 lane configuration matches hardware
2. Check clock synchronization: `verify_clock_synchronization(I2C_NUM_0)`
3. Adjust gain/exposure: very high gain causes noise
4. Check sensor temperature (>60°C degrades quality)

### Issue: Memory Allocation Failures

**Symptom**: `image_buffer_allocate()` returns `ESP_ERR_NO_MEM`

**Solutions**:
1. Check available heap:
   ```c
   ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
   ```

2. Reduce image resolution or buffer count
3. Use PSRAM if available:
   ```c
   buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
   ```

### Issue: I2C Bus Hangs

**Symptom**: I2C operations timeout repeatedly

**Solutions**:
1. Check bus voltage (should be 3.3V)
2. Verify pullup resistors (typically 4.7kΩ to 10kΩ)
3. Reduce clock speed:
   ```c
   i2c_conf.master.clk_speed = 100000;  /* 100kHz instead of 400kHz */
   ```

4. Reinitialize bus if stuck:
   ```c
   i2c_driver_delete(I2C_NUM_0);
   i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
   ```

---

## Performance Metrics

### Data Rate Requirements

| Resolution | Format | FPS | Data Rate | Min CSI Lanes |
|-----------|--------|-----|-----------|--------------|
| 3280x2464 | RAW10 | 21.19 | 2.68 Gbps | 4 lanes |
| 1920x1080 | RAW10 | 47.57 | 0.987 Gbps | 2 lanes |
| 1640x1232 | RAW10 | 41.85 | 0.862 Gbps | 2 lanes |
| 640x480 | RAW10 | 206.65 | 0.206 Gbps | 2 lanes |

### Memory Requirements

| Item | 3280x2464 | 1920x1080 | 640x480 |
|------|-----------|-----------|---------|
| Single Frame (RAW10) | 10.1 MB | 3.1 MB | 0.4 MB |
| Single Frame (RAW8) | 8.1 MB | 2.5 MB | 0.3 MB |
| Disparity Map | 8.1 MB | 2.5 MB | 0.3 MB |

### Timing Characteristics

| Operation | Duration |
|-----------|----------|
| Sensor Reset | ~200ms |
| Mode Change | ~100ms |
| Frame at 21.19 fps | 47.2ms |
| Frame at 47.57 fps | 21.0ms |
| I2C Register Write | ~1ms |

---

## Production Deployment Checklist

- [ ] All input validation enabled in Kconfig
- [ ] Register write verification enabled
- [ ] DMA buffers properly aligned and allocated
- [ ] Stereo frame synchronization verified
- [ ] Security audit logging enabled
- [ ] Stack size set appropriately (minimum 4KB)
- [ ] Heap fragmentation monitoring enabled
- [ ] I2C bus health checks implemented
- [ ] Temperature monitoring (if possible)
- [ ] Over-current protection verified
- [ ] Watchdog timer configured
- [ ] Error recovery mechanisms tested
- [ ] Security test suite passed
- [ ] Code review completed
- [ ] Documentation updated

---

## Support & Resources

### Online Resources
- **Official IMX219 Datasheet**: Open Source Instruments
- **Waveshare Wiki**: https://www.waveshare.com/wiki/IMX219-83_Stereo_Camera
- **ESP-IDF Documentation**: https://docs.espressif.com/projects/esp-idf/
- **OpenCV Stereo Vision**: https://docs.opencv.org/master/d9/d0c/group__calib3d.html

### Community
- **GitHub Issues**: Report bugs and request features
- **ESP32 Forum**: https://esp32.com/
- **Stack Overflow**: Tag `[imx219]` and `[esp32]`

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-20 | Initial release with complete dual-sensor support |

---

## License

This driver implementation is provided as-is for educational and commercial use.
Ensure compliance with local regulations regarding camera devices and privacy laws.

**Key Security Features Implemented**:
- Integer overflow detection
- Buffer bounds checking
- Register write validation
- DMA buffer security
- Stereo alignment validation
- Memory safe operations
- Input sanitization
- Error handling & recovery

---

**For complete technical details, see `IMX219_83_TECHNICAL_REFERENCE.md`**

**For security hardening guidelines, see `IMX219_SECURITY_GUIDE.md`**

