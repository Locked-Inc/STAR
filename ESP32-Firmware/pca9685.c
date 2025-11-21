/**
 * PCA9685 16-Channel PWM LED Controller Driver for ESP32-IDF
 *
 * Complete implementation with security and memory safety
 *
 * Author: Technical Research - Anthropic Claude Code
 * Version: 1.0
 * Date: November 2025
 * License: MIT
 */

#include "pca9685.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

// Logging tag
static const char *TAG = "PCA9685";

// Debug logging control
#define PCA9685_DEBUG_LEVEL ESP_LOG_INFO

/* ============================================================================
 * LOW-LEVEL I2C OPERATIONS
 * ============================================================================ */

/**
 * Internal: Write single register with error handling
 */
esp_err_t pca9685_write_register(pca9685_t *dev, uint8_t reg, uint8_t value) {
    if (dev == NULL || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(PCA9685_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        dev->error_count++;
        dev->last_error_time = xTaskGetTickCount();
        ESP_LOGE(TAG, "Write register 0x%02X failed: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

/**
 * Internal: Read single register with error handling
 */
esp_err_t pca9685_read_register(pca9685_t *dev, uint8_t reg, uint8_t *value) {
    if (dev == NULL || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (value == NULL) {
        ESP_LOGE(TAG, "Value pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Send register address
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(PCA9685_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        dev->error_count++;
        dev->last_error_time = xTaskGetTickCount();
        ESP_LOGE(TAG, "Address register 0x%02X failed: %s", reg, esp_err_to_name(ret));
        return ret;
    }

    // Read data
    cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(PCA9685_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        dev->error_count++;
        dev->last_error_time = xTaskGetTickCount();
        ESP_LOGE(TAG, "Read register 0x%02X failed: %s", reg, esp_err_to_name(ret));
    }

    return ret;
}

/**
 * Internal: Write burst of registers with auto-increment
 */
esp_err_t pca9685_write_registers_burst(pca9685_t *dev, uint8_t start_reg,
                                        const uint8_t *data, size_t len) {
    if (dev == NULL || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL || len == 0 || len > 64) {
        ESP_LOGE(TAG, "Invalid burst parameters: data=%p, len=%d", data, len);
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev->address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, start_reg, true);

    for (size_t i = 0; i < len; i++) {
        i2c_master_write_byte(cmd, data[i], true);
    }

    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(dev->i2c_port, cmd, pdMS_TO_TICKS(PCA9685_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        dev->error_count++;
        dev->last_error_time = xTaskGetTickCount();
        ESP_LOGE(TAG, "Burst write from 0x%02X failed: %s", start_reg, esp_err_to_name(ret));
    }

    return ret;
}

/* ============================================================================
 * DEVICE INITIALIZATION AND CONTROL
 * ============================================================================ */

/**
 * Calculate register address for LED channel
 */
static inline uint8_t pca9685_get_channel_register(uint8_t channel) {
    // Each channel uses 4 registers: ON_L, ON_H, OFF_L, OFF_H
    // Channel 0: 0x06, 0x07, 0x08, 0x09
    // Channel 1: 0x0A, 0x0B, 0x0C, 0x0D
    // etc.
    return PCA9685_REG_LED0_ON_L + (channel * 4);
}

/**
 * Validate channel number
 */
static inline bool pca9685_is_valid_channel(uint8_t channel) {
    return (channel < PCA9685_NUM_CHANNELS || channel == 0xFF);
}

/**
 * Validate PWM value (12-bit)
 */
static inline bool pca9685_is_valid_pwm_value(uint16_t value) {
    return (value <= PCA9685_MAX_PWM_VALUE);
}

/**
 * Validate I2C address
 */
static inline bool pca9685_is_valid_address(uint8_t addr) {
    return (addr >= PCA9685_I2C_ADDR_BASE && addr <= 0x7E);
}

esp_err_t pca9685_init(pca9685_t *dev, i2c_port_t i2c_port, uint8_t address) {
    // Input validation
    if (dev == NULL) {
        ESP_LOGE(TAG, "Device pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (!pca9685_is_valid_address(address)) {
        ESP_LOGE(TAG, "Invalid I2C address: 0x%02X (valid range: 0x40-0x7E)", address);
        return ESP_ERR_INVALID_ARG;
    }

    // Initialize device structure
    dev->i2c_port = i2c_port;
    dev->address = address;
    dev->current_frequency = PCA9685_DEFAULT_FREQUENCY;
    dev->initialized = false;
    dev->error_count = 0;
    dev->last_error_time = 0;

    ESP_LOGI(TAG, "Initializing PCA9685 at address 0x%02X on I2C port %d", address, i2c_port);

    // Verify device presence and readability
    uint8_t mode1;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device not responding at address 0x%02X", address);
        return ret;
    }

    ESP_LOGD(TAG, "Read MODE1: 0x%02X", mode1);

    // Initialize MODE1 register
    // Set SLEEP=1, AI=1, clear others
    uint8_t init_mode1 = PCA9685_MODE1_SLEEP | PCA9685_MODE1_AI;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, init_mode1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MODE1");
        return ret;
    }

    dev->mode1_shadow = init_mode1;
    vTaskDelay(pdMS_TO_TICKS(PCA9685_I2C_INIT_DELAY_MS));

    // Initialize MODE2 register
    // Open-drain output, PWM mode
    uint8_t init_mode2 = PCA9685_MODE2_OUTNE_PWM;  // OUTNE = 01
    ret = pca9685_write_register(dev, PCA9685_REG_MODE2, init_mode2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write MODE2");
        return ret;
    }

    dev->mode2_shadow = init_mode2;
    vTaskDelay(pdMS_TO_TICKS(PCA9685_I2C_INIT_DELAY_MS));

    // Set default frequency (200 Hz)
    ret = pca9685_set_frequency(dev, PCA9685_DEFAULT_FREQUENCY);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set default frequency");
        return ret;
    }

    // Mark as initialized
    dev->initialized = true;

    ESP_LOGI(TAG, "PCA9685 initialized successfully");
    return ESP_OK;
}

esp_err_t pca9685_deinit(pca9685_t *dev) {
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!dev->initialized) {
        return ESP_OK;
    }

    // Set all channels to OFF using ALL_LED register
    // Write 0x10 to ALL_LED_OFF_H sets full OFF for all channels
    esp_err_t ret = pca9685_write_register(dev, PCA9685_REG_ALLLED_OFF_H, 0x10);

    dev->initialized = false;

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "PCA9685 at address 0x%02X deinitialized", dev->address);
    } else {
        ESP_LOGE(TAG, "Error during deinitialization");
    }

    return ret;
}

/* ============================================================================
 * FREQUENCY CONTROL
 * ============================================================================ */

esp_err_t pca9685_set_frequency(pca9685_t *dev, uint16_t frequency_hz) {
    if (dev == NULL || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate and clamp frequency
    if (frequency_hz < PCA9685_MIN_FREQUENCY_HZ) {
        ESP_LOGW(TAG, "Frequency too low: %d Hz, clamping to minimum %d Hz",
                 frequency_hz, PCA9685_MIN_FREQUENCY_HZ);
        frequency_hz = PCA9685_MIN_FREQUENCY_HZ;
    }

    if (frequency_hz > PCA9685_MAX_FREQUENCY_HZ) {
        ESP_LOGW(TAG, "Frequency too high: %d Hz, clamping to maximum %d Hz",
                 frequency_hz, PCA9685_MAX_FREQUENCY_HZ);
        frequency_hz = PCA9685_MAX_FREQUENCY_HZ;
    }

    // Calculate prescaler using floating-point to avoid integer overflow
    // Formula: prescale = round(25MHz / (4096 * frequency)) - 1
    float prescale_f = ((float)PCA9685_OSC_FREQUENCY / (4096.0f * (float)frequency_hz)) - 1.0f;

    // Clamp prescale to valid hardware range
    if (prescale_f < (float)PCA9685_PRESCALE_MIN) {
        prescale_f = (float)PCA9685_PRESCALE_MIN;
        ESP_LOGW(TAG, "Prescale too low, clamping to minimum %d", PCA9685_PRESCALE_MIN);
    }

    if (prescale_f > (float)PCA9685_PRESCALE_MAX) {
        prescale_f = (float)PCA9685_PRESCALE_MAX;
        ESP_LOGW(TAG, "Prescale too high, clamping to maximum %d", PCA9685_PRESCALE_MAX);
    }

    // Round to nearest integer
    uint8_t prescale = (uint8_t)(prescale_f + 0.5f);

    ESP_LOGD(TAG, "Frequency %d Hz → prescale %d (%.2f Hz calculated)",
             frequency_hz, prescale,
             (float)PCA9685_OSC_FREQUENCY / (4096.0f * (float)(prescale + 1)));

    // Read current MODE1
    uint8_t mode1;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Set SLEEP bit before modifying PRE_SCALE
    mode1 |= PCA9685_MODE1_SLEEP;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(PCA9685_I2C_INIT_DELAY_MS));

    // Write prescaler
    ret = pca9685_write_register(dev, PCA9685_REG_PRE_SCALE, prescale);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(PCA9685_I2C_INIT_DELAY_MS));

    // Clear SLEEP bit to resume oscillator
    mode1 &= ~PCA9685_MODE1_SLEEP;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Set RESTART bit to synchronize counters
    mode1 |= PCA9685_MODE1_RESTART;
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    dev->mode1_shadow = mode1;
    dev->current_frequency = frequency_hz;

    ESP_LOGI(TAG, "Frequency set to %d Hz (prescale: %d)", frequency_hz, prescale);

    return ESP_OK;
}

uint16_t pca9685_get_frequency(const pca9685_t *dev) {
    if (dev == NULL || !dev->initialized) {
        return 0;
    }

    return dev->current_frequency;
}

esp_err_t pca9685_get_actual_frequency(pca9685_t *dev, float *freq_hz) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (freq_hz == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Read current prescaler
    uint8_t prescale;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_PRE_SCALE, &prescale);
    if (ret != ESP_OK) {
        return ret;
    }

    // Calculate frequency from prescaler
    // frequency = 25MHz / (4096 * (prescale + 1))
    *freq_hz = (float)PCA9685_OSC_FREQUENCY / (4096.0f * (float)(prescale + 1));

    return ESP_OK;
}

/* ============================================================================
 * PWM OUTPUT CONTROL
 * ============================================================================ */

esp_err_t pca9685_set_pwm(pca9685_t *dev, uint8_t channel, uint16_t on, uint16_t off) {
    if (dev == NULL || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate channel
    if (!pca9685_is_valid_channel(channel)) {
        ESP_LOGE(TAG, "Invalid channel: %d (valid: 0-%d)", channel, PCA9685_NUM_CHANNELS - 1);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate PWM values (12-bit)
    if (!pca9685_is_valid_pwm_value(on)) {
        ESP_LOGE(TAG, "ON value out of range: 0x%04X (max: 0x%04X)", on, PCA9685_MAX_PWM_VALUE);
        return ESP_ERR_INVALID_ARG;
    }

    if (!pca9685_is_valid_pwm_value(off)) {
        ESP_LOGE(TAG, "OFF value out of range: 0x%04X (max: 0x%04X)", off, PCA9685_MAX_PWM_VALUE);
        return ESP_ERR_INVALID_ARG;
    }

    // CRITICAL HARDWARE LIMITATION: ON and OFF must not be identical
    // (unless using special full ON/OFF values)
    if (on == off && on != 0x1000) {
        ESP_LOGE(TAG, "ON and OFF values cannot be identical: 0x%04X", on);
        return ESP_ERR_INVALID_ARG;
    }

    // Split 12-bit values across two 8-bit registers
    uint8_t on_l = on & 0xFF;           // Lower 8 bits
    uint8_t on_h = (on >> 8) & 0x0F;    // Upper 4 bits (mask to 4 bits)
    uint8_t off_l = off & 0xFF;         // Lower 8 bits
    uint8_t off_h = (off >> 8) & 0x0F;  // Upper 4 bits (mask to 4 bits)

    // Get register addresses
    uint8_t base_reg = pca9685_get_channel_register(channel);
    uint8_t on_l_reg = base_reg;
    uint8_t on_h_reg = base_reg + 1;
    uint8_t off_l_reg = base_reg + 2;
    uint8_t off_h_reg = base_reg + 3;

    // Write registers
    esp_err_t ret;

    ret = pca9685_write_register(dev, on_l_reg, on_l);
    if (ret != ESP_OK) return ret;

    ret = pca9685_write_register(dev, on_h_reg, on_h);
    if (ret != ESP_OK) return ret;

    ret = pca9685_write_register(dev, off_l_reg, off_l);
    if (ret != ESP_OK) return ret;

    ret = pca9685_write_register(dev, off_h_reg, off_h);
    if (ret != ESP_OK) return ret;

    // Log PWM configuration
    float duty_cycle = ((float)(off - on) / 4096.0f) * 100.0f;
    ESP_LOGV(TAG, "Channel %d: ON=0x%04X, OFF=0x%04X, duty=%.2f%%",
             channel, on, off, duty_cycle);

    return ESP_OK;
}

esp_err_t pca9685_set_pwm_pulse(pca9685_t *dev, uint8_t channel, uint16_t pulse_us) {
    if (dev == NULL || !dev->initialized) {
        ESP_LOGE(TAG, "Device not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!pca9685_is_valid_channel(channel)) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate period in microseconds
    uint32_t period_us = 1000000 / dev->current_frequency;

    // Validate pulse width
    if (pulse_us > period_us) {
        ESP_LOGE(TAG, "Pulse width too long: %d us (period: %d us)", pulse_us, period_us);
        return ESP_ERR_INVALID_ARG;
    }

    // Convert microseconds to counter steps (0-4095)
    uint16_t pulse_steps = (pulse_us * 4096) / period_us;

    // Clamp to valid range
    if (pulse_steps > PCA9685_MAX_PWM_VALUE) {
        pulse_steps = PCA9685_MAX_PWM_VALUE;
    }

    // Set ON=0, OFF=pulse_steps
    uint16_t on = 0;
    uint16_t off = pulse_steps;

    // Ensure ON != OFF (hardware requirement)
    if (on == off && off < PCA9685_MAX_PWM_VALUE) {
        off++;
    } else if (on == off) {
        ESP_LOGE(TAG, "Cannot create valid PWM for %d us pulse", pulse_us);
        return ESP_ERR_INVALID_ARG;
    }

    return pca9685_set_pwm(dev, channel, on, off);
}

esp_err_t pca9685_get_pwm(pca9685_t *dev, uint8_t channel, uint16_t *on, uint16_t *off) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!pca9685_is_valid_channel(channel)) {
        return ESP_ERR_INVALID_ARG;
    }

    if (on == NULL || off == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get register addresses
    uint8_t base_reg = pca9685_get_channel_register(channel);
    uint8_t on_l_reg = base_reg;
    uint8_t on_h_reg = base_reg + 1;
    uint8_t off_l_reg = base_reg + 2;
    uint8_t off_h_reg = base_reg + 3;

    // Read register values
    uint8_t on_l, on_h, off_l, off_h;

    esp_err_t ret = pca9685_read_register(dev, on_l_reg, &on_l);
    if (ret != ESP_OK) return ret;

    ret = pca9685_read_register(dev, on_h_reg, &on_h);
    if (ret != ESP_OK) return ret;

    ret = pca9685_read_register(dev, off_l_reg, &off_l);
    if (ret != ESP_OK) return ret;

    ret = pca9685_read_register(dev, off_h_reg, &off_h);
    if (ret != ESP_OK) return ret;

    // Reconstruct 12-bit values
    *on = ((on_h & 0x0F) << 8) | on_l;
    *off = ((off_h & 0x0F) << 8) | off_l;

    return ESP_OK;
}

esp_err_t pca9685_set_all_pwm(pca9685_t *dev, uint16_t on, uint16_t off) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Validate PWM values
    if (!pca9685_is_valid_pwm_value(on) || !pca9685_is_valid_pwm_value(off)) {
        return ESP_ERR_INVALID_ARG;
    }

    // ON and OFF must be different
    if (on == off && on != 0x1000) {
        return ESP_ERR_INVALID_ARG;
    }

    // Split 12-bit values
    uint8_t on_l = on & 0xFF;
    uint8_t on_h = (on >> 8) & 0x0F;
    uint8_t off_l = off & 0xFF;
    uint8_t off_h = (off >> 8) & 0x0F;

    // Write to ALL_LED registers using burst write
    uint8_t data[4] = {on_l, on_h, off_l, off_h};
    esp_err_t ret = pca9685_write_registers_burst(dev, PCA9685_REG_ALLLED_ON_L, data, 4);

    if (ret == ESP_OK) {
        ESP_LOGV(TAG, "All channels: ON=0x%04X, OFF=0x%04X", on, off);
    }

    return ret;
}

/* ============================================================================
 * OUTPUT MODE CONTROL
 * ============================================================================ */

esp_err_t pca9685_set_output_driver(pca9685_t *dev, pca9685_outdrv_mode_t mode) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read current MODE2
    uint8_t mode2;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE2, &mode2);
    if (ret != ESP_OK) {
        return ret;
    }

    // Modify OUTDRV bit
    if (mode == PCA9685_OUTDRV_TOTEM_POLE) {
        mode2 |= PCA9685_MODE2_OUTDRV;
        ESP_LOGD(TAG, "Output mode: totem-pole");
    } else {
        mode2 &= ~PCA9685_MODE2_OUTDRV;
        ESP_LOGD(TAG, "Output mode: open-drain");
    }

    // Write back
    ret = pca9685_write_register(dev, PCA9685_REG_MODE2, mode2);
    if (ret == ESP_OK) {
        dev->mode2_shadow = mode2;
    }

    return ret;
}

esp_err_t pca9685_set_output_inverted(pca9685_t *dev, bool inverted) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read current MODE2
    uint8_t mode2;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE2, &mode2);
    if (ret != ESP_OK) {
        return ret;
    }

    // Modify INVRT bit
    if (inverted) {
        mode2 |= PCA9685_MODE2_INVRT;
        ESP_LOGD(TAG, "Output inversion: enabled");
    } else {
        mode2 &= ~PCA9685_MODE2_INVRT;
        ESP_LOGD(TAG, "Output inversion: disabled");
    }

    // Write back
    ret = pca9685_write_register(dev, PCA9685_REG_MODE2, mode2);
    if (ret == ESP_OK) {
        dev->mode2_shadow = mode2;
    }

    return ret;
}

esp_err_t pca9685_set_sleep_mode(pca9685_t *dev, bool enabled) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read current MODE1
    uint8_t mode1;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Modify SLEEP bit
    if (enabled) {
        mode1 |= PCA9685_MODE1_SLEEP;
        ESP_LOGD(TAG, "Sleep mode: enabled");
    } else {
        mode1 &= ~PCA9685_MODE1_SLEEP;
        ESP_LOGD(TAG, "Sleep mode: disabled");
    }

    // Write back
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret == ESP_OK) {
        dev->mode1_shadow = mode1;
    }

    return ret;
}

esp_err_t pca9685_set_allcall_address(pca9685_t *dev, bool enabled) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Read current MODE1
    uint8_t mode1;
    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        return ret;
    }

    // Modify ALLCALL bit
    if (enabled) {
        mode1 |= PCA9685_MODE1_ALLCALL;
        ESP_LOGD(TAG, "All Call address: enabled");
    } else {
        mode1 &= ~PCA9685_MODE1_ALLCALL;
        ESP_LOGD(TAG, "All Call address: disabled");
    }

    // Write back
    ret = pca9685_write_register(dev, PCA9685_REG_MODE1, mode1);
    if (ret == ESP_OK) {
        dev->mode1_shadow = mode1;
    }

    return ret;
}

/* ============================================================================
 * VERIFICATION AND DIAGNOSTICS
 * ============================================================================ */

esp_err_t pca9685_verify_device(pca9685_t *dev) {
    if (dev == NULL || !dev->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Try to read MODE1 and MODE2 registers
    uint8_t mode1, mode2;

    esp_err_t ret = pca9685_read_register(dev, PCA9685_REG_MODE1, &mode1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MODE1");
        return ret;
    }

    ret = pca9685_read_register(dev, PCA9685_REG_MODE2, &mode2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read MODE2");
        return ret;
    }

    // Verify device responds correctly
    if (mode1 == 0xFF && mode2 == 0xFF) {
        ESP_LOGE(TAG, "Device appears to not be responding (all 0xFF)");
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "Device verified: MODE1=0x%02X, MODE2=0x%02X", mode1, mode2);

    return ESP_OK;
}

esp_err_t pca9685_get_error_count(const pca9685_t *dev, uint32_t *count) {
    if (dev == NULL || count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = dev->error_count;
    return ESP_OK;
}
