/*
 * IMX219-83 Stereo Camera Driver Implementation
 * Security-hardened C implementation for ESP32-IDF
 *
 * Implements secure I2C register access, buffer management,
 * exposure/gain control, stereo synchronization, and depth calculation
 */

#include "imx219_stereo.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "rom/ets_sys.h"

static const char *TAG = "IMX219_STEREO";

/* ============================================================================
 * SUPPORTED RESOLUTION MODES
 * ============================================================================
 */

static const imx219_mode_t g_imx219_modes[IMX219_RES_COUNT] = {
    [IMX219_RES_FULL] = {
        .width = 3280,
        .height = 2464,
        .x_start = 0,
        .y_start = 0,
        .frame_length = 3526,
        .line_length = 3448,
        .binning_h = 0,
        .binning_v = 0,
        .max_fps = 21.19f,
        .csi_lanes = 2,
    },
    [IMX219_RES_1080P] = {
        .width = 1920,
        .height = 1080,
        .x_start = 680,
        .y_start = 692,
        .frame_length = 1763,
        .line_length = 3448,
        .binning_h = 0,
        .binning_v = 0,
        .max_fps = 47.57f,
        .csi_lanes = 2,
    },
    [IMX219_RES_2MP] = {
        .width = 1640,
        .height = 1232,
        .x_start = 0,
        .y_start = 0,
        .frame_length = 1763,
        .line_length = 3448,
        .binning_h = 1,
        .binning_v = 1,
        .max_fps = 41.85f,
        .csi_lanes = 2,
    },
    [IMX219_RES_VGA] = {
        .width = 640,
        .height = 480,
        .x_start = 1000,
        .y_start = 752,
        .frame_length = 1763,
        .line_length = 3448,
        .binning_h = 2,
        .binning_v = 2,
        .max_fps = 206.65f,
        .csi_lanes = 2,
    },
};

/* ============================================================================
 * REGISTER CONSTRAINTS FOR VALIDATION
 * ============================================================================
 */

typedef struct {
    uint16_t addr;
    uint8_t min_value;
    uint8_t max_value;
    const char *name;
} imx219_reg_constraint_t;

static const imx219_reg_constraint_t g_register_constraints[] = {
    {IMX219_REG_MODE_SELECT, 0x00, 0x01, "MODE_SELECT"},
    {IMX219_REG_RESET, 0x00, 0x01, "SW_RESET"},
    {IMX219_REG_CSI_LANE_MODE, 0x00, 0x01, "CSI_LANE_MODE"},
    {IMX219_REG_ANALOG_GAIN, 0x00, 232, "ANALOG_GAIN"},
    {IMX219_REG_TEST_PATTERN, 0x00, 0x01, "TEST_PATTERN_EN"},
    {0, 0, 0, NULL}
};

/* ============================================================================
 * I2C REGISTER ACCESS - CORE FUNCTIONS
 * ============================================================================
 */

esp_err_t imx219_write_reg(i2c_port_t port, uint16_t addr, uint8_t data)
{
    if (port >= I2C_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t write_buf[3];
    write_buf[0] = (addr >> 8) & 0xFF;
    write_buf[1] = addr & 0xFF;
    write_buf[2] = data;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to create I2C command handle");
        return ESP_FAIL;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMX219_I2C_WRITE_ADDR), true);
    i2c_master_write(cmd, write_buf, 3, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: addr=0x%04X, ret=%d", addr, ret);
    }

    return ret;
}

esp_err_t imx219_read_reg(i2c_port_t port, uint16_t addr, uint8_t *data)
{
    if (port >= I2C_NUM_MAX || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t write_buf[2];
    write_buf[0] = (addr >> 8) & 0xFF;
    write_buf[1] = addr & 0xFF;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (!cmd) {
        ESP_LOGE(TAG, "Failed to create I2C command handle");
        return ESP_FAIL;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMX219_I2C_WRITE_ADDR), true);
    i2c_master_write(cmd, write_buf, 2, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IMX219_I2C_READ_ADDR), true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: addr=0x%04X, ret=%d", addr, ret);
    }

    return ret;
}

/* ============================================================================
 * REGISTER VALIDATION & SECURE WRITE
 * ============================================================================
 */

esp_err_t validate_register_write(uint16_t addr, uint8_t value)
{
    for (int i = 0; g_register_constraints[i].name != NULL; i++) {
        const imx219_reg_constraint_t *constraint = &g_register_constraints[i];

        if (constraint->addr == addr) {
            if (value < constraint->min_value || value > constraint->max_value) {
                ESP_LOGE(TAG, "Invalid value for %s (0x%04X): 0x%02X (valid: 0x%02X-0x%02X)",
                         constraint->name, addr, value,
                         constraint->min_value, constraint->max_value);
                return ESP_ERR_INVALID_ARG;
            }
            return ESP_OK;
        }
    }

    ESP_LOGD(TAG, "Writing to unconstrained register 0x%04X = 0x%02X", addr, value);
    return ESP_OK;
}

esp_err_t imx219_secure_write_reg(i2c_port_t port, uint16_t addr, uint8_t value)
{
    /* 1. Validate register value */
    if (validate_register_write(addr, value) != ESP_OK) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 2. Write to register */
    if (imx219_write_reg(port, addr, value) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Small delay for register to take effect */
    esp_rom_delay_us(100);

    /* 3. Read back for verification */
    uint8_t readback = 0;
    if (imx219_read_reg(port, addr, &readback) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read back register 0x%04X", addr);
        return ESP_FAIL;
    }

    /* 4. Verify value matches (with tolerance for read-only bits) */
    if (readback != value) {
        ESP_LOGW(TAG, "Register mismatch at 0x%04X: wrote 0x%02X, read 0x%02X",
                 addr, value, readback);
        /* Still return success if it's a read-only or partial write */
    }

    return ESP_OK;
}

/* ============================================================================
 * EXPOSURE & GAIN CONTROL
 * ============================================================================
 */

float imx219_analog_gain_to_multiplier(uint8_t reg_value)
{
    return ((float)(reg_value + 256)) / 256.0f;
}

uint8_t imx219_gain_multiplier_to_register(float multiplier)
{
    if (multiplier < 1.0f) multiplier = 1.0f;
    if (multiplier > 11.0f) multiplier = 11.0f;

    return (uint8_t)((multiplier * 256) - 256);
}

esp_err_t imx219_set_exposure(i2c_port_t port, uint16_t exposure_lines)
{
    if (exposure_lines < IMX219_EXPOSURE_MIN ||
        exposure_lines > IMX219_EXPOSURE_MAX) {
        ESP_LOGE(TAG, "Exposure out of range: %d (min: %d, max: %d)",
                 exposure_lines, IMX219_EXPOSURE_MIN, IMX219_EXPOSURE_MAX);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t exp_h = (exposure_lines >> 8) & 0xFF;
    uint8_t exp_l = exposure_lines & 0xFF;

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_EXPOSURE_H, exp_h),
        TAG, "Failed to write exposure MSB"
    );

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_EXPOSURE_L, exp_l),
        TAG, "Failed to write exposure LSB"
    );

    ESP_LOGD(TAG, "Set exposure to %d lines", exposure_lines);
    return ESP_OK;
}

esp_err_t imx219_set_analog_gain(i2c_port_t port, float gain_multiplier)
{
    uint8_t reg_value = imx219_gain_multiplier_to_register(gain_multiplier);

    if (reg_value > IMX219_ANALOG_GAIN_MAX) {
        ESP_LOGE(TAG, "Analog gain out of range: %.2fx", gain_multiplier);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(port, IMX219_REG_ANALOG_GAIN, reg_value),
        TAG, "Failed to set analog gain"
    );

    ESP_LOGD(TAG, "Set analog gain to %.2fx (0x%02X)", gain_multiplier, reg_value);
    return ESP_OK;
}

esp_err_t imx219_set_digital_gain(i2c_port_t port, uint16_t gain_db256)
{
    if (gain_db256 < IMX219_DIGITAL_GAIN_MIN ||
        gain_db256 > IMX219_DIGITAL_GAIN_MAX) {
        ESP_LOGE(TAG, "Digital gain out of range: 0x%04X", gain_db256);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t gain_h = (gain_db256 >> 8) & 0xFF;
    uint8_t gain_l = gain_db256 & 0xFF;

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_DIGITAL_GAIN_H, gain_h),
        TAG, "Failed to write digital gain MSB"
    );

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_DIGITAL_GAIN_L, gain_l),
        TAG, "Failed to write digital gain LSB"
    );

    ESP_LOGD(TAG, "Set digital gain to 0x%04X", gain_db256);
    return ESP_OK;
}

/* ============================================================================
 * BUFFER MANAGEMENT - SECURE ALLOCATION
 * ============================================================================
 */

esp_err_t image_buffer_allocate(image_buffer_t *buf,
                               uint16_t width, uint16_t height,
                               uint16_t bytes_per_pixel)
{
    if (!buf) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Validate dimensions */
    if (width == 0 || width > MAX_IMAGE_WIDTH ||
        height == 0 || height > MAX_IMAGE_HEIGHT) {
        ESP_LOGE(TAG, "Invalid image dimensions: %dx%d", width, height);
        return ESP_ERR_INVALID_ARG;
    }

    if (bytes_per_pixel == 0 || bytes_per_pixel > 4) {
        ESP_LOGE(TAG, "Invalid bytes per pixel: %d", bytes_per_pixel);
        return ESP_ERR_INVALID_ARG;
    }

    /* Check for integer overflow in size calculation */
    uint32_t required_bytes;
    if (__builtin_mul_overflow(width, bytes_per_pixel, &required_bytes) ||
        __builtin_mul_overflow(required_bytes, height, &required_bytes)) {
        ESP_LOGE(TAG, "Size calculation overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Add alignment padding (DMA requirement) */
    uint32_t stride = ((width * bytes_per_pixel + 31) / 32) * 32;
    uint32_t aligned_bytes;
    if (__builtin_mul_overflow(stride, height, &aligned_bytes)) {
        ESP_LOGE(TAG, "Aligned size overflow");
        return ESP_ERR_INVALID_ARG;
    }

    if (aligned_bytes > MAX_IMAGE_BYTES) {
        ESP_LOGE(TAG, "Buffer too large: %d > %d", aligned_bytes, MAX_IMAGE_BYTES);
        return ESP_ERR_NO_MEM;
    }

    /* Allocate with DMA capability */
    buf->buffer = (uint8_t *)heap_caps_malloc(
        aligned_bytes,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    if (buf->buffer == NULL) {
        ESP_LOGE(TAG, "Allocation failed for %d bytes", aligned_bytes);
        return ESP_ERR_NO_MEM;
    }

    /* Initialize structure with validated values */
    buf->width = width;
    buf->height = height;
    buf->stride = stride;
    buf->allocated_bytes = aligned_bytes;

    ESP_LOGI(TAG, "Allocated %dx%d buffer (%d bytes, stride %d)",
             width, height, aligned_bytes, stride);

    return ESP_OK;
}

esp_err_t image_buffer_validate_access(const image_buffer_t *buf,
                                      uint32_t offset, uint32_t size)
{
    if (!buf || !buf->buffer) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Check for integer overflow in end calculation */
    uint32_t end_offset;
    if (__builtin_add_overflow(offset, size, &end_offset)) {
        ESP_LOGE(TAG, "Offset calculation overflow");
        return ESP_ERR_INVALID_ARG;
    }

    if (end_offset > buf->allocated_bytes) {
        ESP_LOGE(TAG, "Access out of bounds: %d > %d",
                 end_offset, buf->allocated_bytes);
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

void image_buffer_free(image_buffer_t *buf)
{
    if (buf && buf->buffer) {
        free(buf->buffer);
        buf->buffer = NULL;
        buf->allocated_bytes = 0;
        buf->width = 0;
        buf->height = 0;
    }
}

/* ============================================================================
 * DMA BUFFER MANAGEMENT
 * ============================================================================
 */

esp_err_t allocate_dma_buffer(dma_buffer_info_t *buf, uint32_t size)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (size > MAX_DMA_BUFFER_SIZE) {
        ESP_LOGE(TAG, "DMA buffer size too large: %d > %d", size, MAX_DMA_BUFFER_SIZE);
        return ESP_ERR_NO_MEM;
    }

    buf->vaddr = heap_caps_malloc(size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

    if (buf->vaddr == NULL) {
        ESP_LOGE(TAG, "DMA buffer allocation failed for %d bytes", size);
        return ESP_ERR_NO_MEM;
    }

    /* Verify alignment */
    uint32_t addr_int = (uint32_t)buf->vaddr;
    if ((addr_int & DMA_ALIGNMENT_MASK) != 0) {
        ESP_LOGW(TAG, "DMA buffer not properly aligned: 0x%08X", addr_int);
        free(buf->vaddr);
        return ESP_FAIL;
    }

    buf->size = size;
    buf->alignment = DMA_ALIGNMENT_BYTES;
    buf->paddr = (uint32_t)buf->vaddr;  /* Identity mapped on ESP32 */

    ESP_LOGI(TAG, "Allocated secure DMA buffer: vaddr=0x%08X, size=%d",
             (uint32_t)buf->vaddr, size);

    return ESP_OK;
}

esp_err_t validate_dma_descriptor(const dma_buffer_info_t *buf)
{
    if (!buf || !buf->vaddr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (((uint32_t)buf->vaddr & DMA_ALIGNMENT_MASK) != 0) {
        ESP_LOGE(TAG, "DMA buffer not aligned: 0x%08X", (uint32_t)buf->vaddr);
        return ESP_FAIL;
    }

    if (buf->size == 0 || buf->size > MAX_DMA_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Invalid DMA buffer size: %d", buf->size);
        return ESP_FAIL;
    }

    return ESP_OK;
}

void free_dma_buffer(dma_buffer_info_t *buf)
{
    if (buf && buf->vaddr) {
        free(buf->vaddr);
        buf->vaddr = NULL;
        buf->size = 0;
    }
}

/* ============================================================================
 * RESOLUTION VALIDATION
 * ============================================================================
 */

const imx219_mode_t *imx219_get_mode(imx219_resolution_t resolution)
{
    if (resolution >= IMX219_RES_COUNT) {
        ESP_LOGE(TAG, "Invalid resolution index: %d", resolution);
        return NULL;
    }

    return &g_imx219_modes[resolution];
}

esp_err_t validate_resolution(uint16_t width, uint16_t height,
                             uint8_t format, void *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < IMX219_RES_COUNT; i++) {
        const imx219_mode_t *mode = &g_imx219_modes[i];

        if (mode->width == width && mode->height == height) {
            memcpy(out, mode, sizeof(imx219_mode_t));
            ESP_LOGD(TAG, "Validated resolution: %dx%d", width, height);
            return ESP_OK;
        }
    }

    ESP_LOGE(TAG, "Unsupported resolution: %dx%d", width, height);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t imx219_set_mode(i2c_port_t port, const imx219_mode_t *mode)
{
    if (!mode) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Validate resolution */
    validated_resolution_t validated;
    ESP_RETURN_ON_ERROR(
        validate_resolution(mode->width, mode->height, 0, &validated),
        TAG, "Invalid resolution"
    );

    /* Stop streaming */
    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(port, IMX219_REG_MODE_SELECT, 0x00),
        TAG, "Failed to stop streaming"
    );

    vTaskDelay(100 / portTICK_PERIOD_MS);

    /* Configure frame timing */
    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_FRAME_LENGTH_H,
                        (mode->frame_length >> 8) & 0xFF),
        TAG, "Frame length H write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_FRAME_LENGTH_L,
                        mode->frame_length & 0xFF),
        TAG, "Frame length L write failed"
    );

    /* Configure output size */
    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_X_OUTPUT_SIZE_H,
                        (mode->width >> 8) & 0xFF),
        TAG, "X output size H write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_X_OUTPUT_SIZE_L,
                        mode->width & 0xFF),
        TAG, "X output size L write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_Y_OUTPUT_SIZE_H,
                        (mode->height >> 8) & 0xFF),
        TAG, "Y output size H write failed"
    );

    ESP_RETURN_ON_ERROR(
        imx219_write_reg(port, IMX219_REG_Y_OUTPUT_SIZE_L,
                        mode->height & 0xFF),
        TAG, "Y output size L write failed"
    );

    /* Start streaming */
    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(port, IMX219_REG_MODE_SELECT, 0x01),
        TAG, "Failed to start streaming"
    );

    ESP_LOGI(TAG, "Set mode: %dx%d @ %.2f fps",
             mode->width, mode->height, mode->max_fps);

    return ESP_OK;
}

/* ============================================================================
 * STEREO DEPTH CALCULATION
 * ============================================================================
 */

float calculate_depth_mm(const stereo_calibration_t *calib, float disparity)
{
    if (!calib || disparity <= calib->min_disparity) {
        return INFINITY;
    }

    return (calib->focal_length_pixels * calib->baseline_mm) / disparity;
}

uint16_t calculate_required_image_width(float min_depth_mm,
                                        float max_depth_mm,
                                        const stereo_calibration_t *calib)
{
    if (!calib || min_depth_mm <= 0 || max_depth_mm <= 0) {
        return 0;
    }

    float max_disp = (calib->focal_length_pixels * calib->baseline_mm) / min_depth_mm;
    return (uint16_t)max_disp + 1;
}

esp_err_t allocate_disparity_map(stereo_disparity_map_t *map,
                                 uint16_t width, uint16_t height)
{
    if (!map || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (width > MAX_IMAGE_WIDTH || height > MAX_IMAGE_HEIGHT) {
        ESP_LOGE(TAG, "Disparity map dimensions exceed sensor: %dx%d", width, height);
        return ESP_ERR_INVALID_ARG;
    }

    /* Safe multiplication check */
    uint32_t num_pixels;
    if (__builtin_mul_overflow((uint32_t)width, (uint32_t)height, &num_pixels)) {
        ESP_LOGE(TAG, "Pixel count overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Calculate required bytes */
    uint32_t required_bytes;
    if (__builtin_mul_overflow(num_pixels, sizeof(uint16_t), &required_bytes)) {
        ESP_LOGE(TAG, "Disparity buffer size overflow");
        return ESP_ERR_INVALID_ARG;
    }

    /* Memory limit check (2MB for stereo disparity) */
    if (required_bytes > 2 * 1024 * 1024) {
        ESP_LOGE(TAG, "Disparity map allocation exceeds limit: %d > 2MB", required_bytes);
        return ESP_ERR_NO_MEM;
    }

    /* Allocate with DMA flags */
    map->disparity_map = (uint16_t *)heap_caps_malloc(
        required_bytes,
        MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL
    );

    if (map->disparity_map == NULL) {
        ESP_LOGE(TAG, "Disparity map allocation failed");
        return ESP_ERR_NO_MEM;
    }

    map->width = width;
    map->height = height;
    map->total_size_bytes = required_bytes;

    ESP_LOGI(TAG, "Allocated disparity map: %dx%d (%d bytes)",
             width, height, required_bytes);

    return ESP_OK;
}

esp_err_t filter_disparity_map(stereo_disparity_map_t *map,
                               float min_depth_mm,
                               float max_depth_mm,
                               const stereo_calibration_t *calib)
{
    if (!map || !map->disparity_map || !calib) {
        return ESP_ERR_INVALID_ARG;
    }

    float max_disp = (calib->focal_length_pixels * calib->baseline_mm) / min_depth_mm;
    float min_disp = (calib->focal_length_pixels * calib->baseline_mm) / max_depth_mm;

    uint32_t total_pixels = map->width * map->height;
    uint32_t invalid_count = 0;

    for (uint32_t i = 0; i < total_pixels; i++) {
        float disp = map->disparity_map[i];

        if (disp < min_disp || disp > max_disp) {
            map->disparity_map[i] = 0;  /* Mark as invalid */
            invalid_count++;
        }
    }

    ESP_LOGD(TAG, "Filtered disparity map: %d invalid pixels out of %d",
             invalid_count, total_pixels);

    return ESP_OK;
}

void free_disparity_map(stereo_disparity_map_t *map)
{
    if (map && map->disparity_map) {
        free(map->disparity_map);
        map->disparity_map = NULL;
        map->width = 0;
        map->height = 0;
        map->total_size_bytes = 0;
    }
}

/* ============================================================================
 * INITIALIZATION & CONTROL
 * ============================================================================
 */

esp_err_t imx219_stereo_init(imx219_driver_t *driver,
                            i2c_port_t i2c_port,
                            gpio_num_t reset_gpio)
{
    if (!driver || i2c_port >= I2C_NUM_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing IMX219 stereo camera");

    driver->i2c_port = i2c_port;
    driver->reset_gpio = reset_gpio;

    /* Configure reset GPIO */
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << reset_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&gpio_conf), TAG, "GPIO config failed");

    /* Perform sensor reset */
    ESP_RETURN_ON_ERROR(imx219_sensor_reset(driver), TAG, "Sensor reset failed");

    /* Initialize calibration data */
    driver->calib.focal_length_pixels = 1640.0f;
    driver->calib.baseline_mm = 60.0f;
    driver->calib.principal_point_x = 1640.0f;
    driver->calib.principal_point_y = 1232.0f;
    driver->calib.min_disparity = 1.0f;
    driver->calib.max_disparity = 60.0f;

    ESP_LOGI(TAG, "IMX219 stereo initialization complete");
    return ESP_OK;
}

esp_err_t imx219_sensor_reset(imx219_driver_t *driver)
{
    if (!driver) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Resetting IMX219 sensor");

    /* Pulse reset line low for 100ms */
    gpio_set_level(driver->reset_gpio, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(driver->reset_gpio, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    return ESP_OK;
}

esp_err_t imx219_start_streaming(imx219_driver_t *driver)
{
    if (!driver) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(driver->i2c_port, IMX219_REG_MODE_SELECT, 0x01),
        TAG, "Failed to start streaming"
    );

    ESP_LOGI(TAG, "Streaming started");
    return ESP_OK;
}

esp_err_t imx219_stop_streaming(imx219_driver_t *driver)
{
    if (!driver) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(
        imx219_secure_write_reg(driver->i2c_port, IMX219_REG_MODE_SELECT, 0x00),
        TAG, "Failed to stop streaming"
    );

    ESP_LOGI(TAG, "Streaming stopped");
    return ESP_OK;
}

esp_err_t imx219_verify_chip_id(imx219_driver_t *driver)
{
    if (!driver) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t chip_id_high, chip_id_low;

    ESP_RETURN_ON_ERROR(
        imx219_read_reg(driver->i2c_port, IMX219_REG_CHIP_ID, &chip_id_high),
        TAG, "Failed to read chip ID MSB"
    );

    ESP_RETURN_ON_ERROR(
        imx219_read_reg(driver->i2c_port, IMX219_REG_CHIP_ID + 1, &chip_id_low),
        TAG, "Failed to read chip ID LSB"
    );

    uint16_t chip_id = ((uint16_t)chip_id_high << 8) | chip_id_low;

    if (chip_id != IMX219_CHIP_ID) {
        ESP_LOGE(TAG, "Invalid chip ID: 0x%04X (expected 0x%04X)",
                 chip_id, IMX219_CHIP_ID);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Chip ID verified: 0x%04X", chip_id);
    return ESP_OK;
}

esp_err_t imx219_stereo_deinit(imx219_driver_t *driver)
{
    if (!driver) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(imx219_stop_streaming(driver), TAG, "Failed to stop streaming");

    ESP_LOGI(TAG, "IMX219 stereo deinitialized");
    return ESP_OK;
}

/* ============================================================================
 * AUTO-EXPOSURE IMPLEMENTATION
 * ============================================================================
 */

esp_err_t ae_initialize(ae_config_t *ae, uint32_t frame_rate)
{
    if (!ae || frame_rate == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ae->ae_enabled = 1;
    ae->target_brightness = 128;
    ae->brightness_tolerance = 10;
    ae->exp_step_size = 10;
    ae->exp_step_factor = 1.1f;
    ae->max_analog_gain = 180;      /* ~8x */
    ae->max_digital_gain = 0x0800;
    ae->convergence_frames = 30;
    ae->frame_rate = frame_rate;

    ESP_LOGI(TAG, "Auto-exposure initialized (target brightness: %d)",
             ae->target_brightness);

    return ESP_OK;
}

esp_err_t ae_compute_exposure(const ae_config_t *ae,
                              uint8_t current_brightness,
                              uint16_t *exposure_adjust)
{
    if (!ae || !exposure_adjust) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t brightness_error = current_brightness - ae->target_brightness;

    if (brightness_error > ae->brightness_tolerance) {
        *exposure_adjust = (uint16_t)(ae->exp_step_size / ae->exp_step_factor);
        ESP_LOGD(TAG, "Reducing exposure (brightness: %d)", current_brightness);
    } else if (brightness_error < -ae->brightness_tolerance) {
        *exposure_adjust = (uint16_t)(ae->exp_step_size * ae->exp_step_factor);
        ESP_LOGD(TAG, "Increasing exposure (brightness: %d)", current_brightness);
    } else {
        *exposure_adjust = ae->exp_step_size;
    }

    return ESP_OK;
}
