/* lib/star_sensor_pca9685/src/star_sensor_pca9685.c */

#include "star_sensor_pca9685.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <math.h>
#include <string.h>

#include "star_bus_i2c.h"

static const char* s_TAG = "pca9685";

/* --- Internal Helper Functions --- */

/**
 * @brief Write to PCA9685 register with validation
 */
static esp_err_t pca9685_write_reg(star_bus_manager_t* manager,
                                   const char*         bus_name,
                                   uint8_t             addr,
                                   uint8_t             reg,
                                   uint8_t             value)
{
  if (manager == NULL || bus_name == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return star_bus_i2c_write(manager, bus_name, &value, 1, reg, NULL);
}

/**
 * @brief Read from PCA9685 register
 */
static esp_err_t pca9685_read_reg(star_bus_manager_t* manager,
                                  const char*         bus_name,
                                  uint8_t             addr,
                                  uint8_t             reg,
                                  uint8_t*            value)
{
  if (manager == NULL || bus_name == NULL || value == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return star_bus_i2c_read(manager, bus_name, value, 1, reg, NULL);
}

/**
 * @brief Set MODE1 register bits
 */
static esp_err_t pca9685_set_mode1_bits(pca9685_handle_t* handle, uint8_t bits, bool set)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t mode1;
  esp_err_t ret =
    pca9685_read_reg(handle->manager, handle->bus_name, handle->i2c_addr, PCA9685_REG_MODE1, &mode1);
  if (ret != ESP_OK) {
    return ret;
  }

  if (set) {
    mode1 |= bits;
  } else {
    mode1 &= ~bits;
  }

  return pca9685_write_reg(handle->manager,
                           handle->bus_name,
                           handle->i2c_addr,
                           PCA9685_REG_MODE1,
                           mode1);
}

/* --- Core Functions --- */

esp_err_t star_sensor_pca9685_init(pca9685_handle_t*       handle,
                                   star_bus_manager_t*     manager,
                                   const char*             bus_name,
                                   const pca9685_config_t* config)
{
  if (handle == NULL || manager == NULL || bus_name == NULL || config == NULL) {
    ESP_LOGE(s_TAG, "NULL parameter in init");
    return ESP_ERR_INVALID_ARG;
  }

  // Validate frequency range
  if (config->pwm_freq < PCA9685_MIN_FREQ || config->pwm_freq > PCA9685_MAX_FREQ) {
    ESP_LOGE(s_TAG,
             "Invalid frequency: %d Hz (valid range: %d-%d)",
             config->pwm_freq,
             PCA9685_MIN_FREQ,
             PCA9685_MAX_FREQ);
    return ESP_ERR_INVALID_ARG;
  }

  // Initialize handle
  memset(handle, 0, sizeof(pca9685_handle_t));
  handle->manager  = manager;
  handle->bus_name = bus_name;
  handle->i2c_addr = config->i2c_addr;
  memcpy(&handle->config, config, sizeof(pca9685_config_t));

  // Initialize error handler
  esp_err_t ret = error_handler_init(&handle->error_handler,
                                     3,    /* max_retries */
                                     10,   /* base_retry_delay (ms) */
                                     100,  /* max_retry_delay (ms) */
                                     NULL, /* no reset function */
                                     NULL); /* no reset context */
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to initialize error handler: %s", esp_err_to_name(ret));
    return ret;
  }

  ESP_LOGI(s_TAG,
           "Initializing PCA9685 on bus '%s' at address 0x%02X",
           bus_name,
           config->i2c_addr);

  // Reset device
  ret = pca9685_write_reg(manager, bus_name, config->i2c_addr, PCA9685_REG_MODE1, 0x00);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to reset device: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(10)); // Wait for reset

  // Configure MODE1: Auto-increment enabled, respond to all-call
  uint8_t mode1 = PCA9685_MODE1_AI | PCA9685_MODE1_ALLCALL;
  if (config->ext_clock) {
    mode1 |= PCA9685_MODE1_EXTCLK;
  }

  ret = pca9685_write_reg(manager, bus_name, config->i2c_addr, PCA9685_REG_MODE1, mode1);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure MODE1: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  // Configure MODE2
  uint8_t mode2 = 0;
  if (config->output_mode == PCA9685_OUTPUT_TOTEM_POLE) {
    mode2 |= PCA9685_MODE2_OUTDRV;
  }
  if (config->invert_output) {
    mode2 |= PCA9685_MODE2_INVRT;
  }

  ret = pca9685_write_reg(manager, bus_name, config->i2c_addr, PCA9685_REG_MODE2, mode2);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure MODE2: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(1)); // Wait for oscillator

  // Set PWM frequency
  ret = star_sensor_pca9685_set_frequency(handle, config->pwm_freq);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to set frequency: %s", esp_err_to_name(ret));
    error_handler_deinit(&handle->error_handler);
    return ret;
  }

  handle->initialized = true;
  ESP_LOGI(s_TAG, "PCA9685 initialization successful");

  return ESP_OK;
}

esp_err_t star_sensor_pca9685_deinit(pca9685_handle_t* handle)
{
  if (handle == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (!handle->initialized) {
    return ESP_OK;
  }

  ESP_LOGI(s_TAG, "Deinitializing PCA9685");

  // Put device to sleep
  star_sensor_pca9685_sleep(handle, true);

  // Deinitialize error handler
  error_handler_deinit(&handle->error_handler);

  // Clear handle
  handle->initialized = false;

  return ESP_OK;
}

esp_err_t star_sensor_pca9685_reset(const pca9685_handle_t* handle)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGW(s_TAG, "Issuing software reset to PCA9685");

  // Write reset command to general call address
  // Note: This uses write_command since it's a single command byte with no register
  esp_err_t ret = star_bus_i2c_write_command(handle->manager, handle->bus_name, 0x06);

  if (ret == ESP_OK) {
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for reset to complete
  }

  return ret;
}

/* --- Frequency Configuration --- */

esp_err_t star_sensor_pca9685_calculate_prescale(uint16_t freq_hz, uint8_t* prescale)
{
  if (prescale == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (freq_hz < PCA9685_MIN_FREQ || freq_hz > PCA9685_MAX_FREQ) {
    ESP_LOGE(s_TAG, "Frequency %d Hz out of range (%d-%d)", freq_hz, PCA9685_MIN_FREQ, PCA9685_MAX_FREQ);
    return ESP_ERR_INVALID_ARG;
  }

  // Calculate prescale: round((25MHz / (4096 * freq)) - 1)
  // Use floating point to prevent integer overflow
  float prescale_f = roundf(((float)PCA9685_OSCILLATOR_FREQ / (4096.0f * freq_hz)) - 1.0f);

  // Clamp to valid range (3-255)
  if (prescale_f < 3.0f) {
    prescale_f = 3.0f;
  } else if (prescale_f > 255.0f) {
    prescale_f = 255.0f;
  }

  *prescale = (uint8_t)prescale_f;

  ESP_LOGD(s_TAG, "Frequency %d Hz -> prescale %d", freq_hz, *prescale);

  return ESP_OK;
}

esp_err_t star_sensor_pca9685_prescale_to_freq(uint8_t prescale, uint16_t* freq_hz)
{
  if (freq_hz == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  if (prescale < 3) {
    return ESP_ERR_INVALID_ARG;
  }

  // Frequency = 25MHz / (4096 * (prescale + 1))
  *freq_hz = (uint16_t)(PCA9685_OSCILLATOR_FREQ / (4096 * (prescale + 1)));

  return ESP_OK;
}

esp_err_t star_sensor_pca9685_set_frequency(pca9685_handle_t* handle, uint16_t freq_hz)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  uint8_t prescale;
  esp_err_t ret = star_sensor_pca9685_calculate_prescale(freq_hz, &prescale);
  if (ret != ESP_OK) {
    return ret;
  }

  // Device must be in sleep mode to change prescale
  uint8_t old_mode1;
  ret = pca9685_read_reg(handle->manager,
                        handle->bus_name,
                        handle->i2c_addr,
                        PCA9685_REG_MODE1,
                        &old_mode1);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to read MODE1: %s", esp_err_to_name(ret));
    return ret;
  }

  // Set sleep bit
  uint8_t sleep_mode = (old_mode1 & ~PCA9685_MODE1_RESTART) | PCA9685_MODE1_SLEEP;
  ret = pca9685_write_reg(handle->manager,
                         handle->bus_name,
                         handle->i2c_addr,
                         PCA9685_REG_MODE1,
                         sleep_mode);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to enter sleep mode: %s", esp_err_to_name(ret));
    return ret;
  }

  // Write prescale
  ret = pca9685_write_reg(handle->manager,
                         handle->bus_name,
                         handle->i2c_addr,
                         PCA9685_REG_PRESCALE,
                         prescale);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to write prescale: %s", esp_err_to_name(ret));
    return ret;
  }

  // Restore old mode (wake up)
  ret = pca9685_write_reg(handle->manager,
                         handle->bus_name,
                         handle->i2c_addr,
                         PCA9685_REG_MODE1,
                         old_mode1);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to restore MODE1: %s", esp_err_to_name(ret));
    return ret;
  }

  vTaskDelay(pdMS_TO_TICKS(1)); // Wait for oscillator

  // Restart if needed
  if (old_mode1 & PCA9685_MODE1_RESTART) {
    ret = pca9685_write_reg(handle->manager,
                           handle->bus_name,
                           handle->i2c_addr,
                           PCA9685_REG_MODE1,
                           old_mode1 | PCA9685_MODE1_RESTART);
    if (ret != ESP_OK) {
      ESP_LOGW(s_TAG, "Failed to restart: %s", esp_err_to_name(ret));
    }
  }

  handle->prescale = prescale;
  ESP_LOGI(s_TAG, "Set frequency to %d Hz (prescale=%d)", freq_hz, prescale);

  return ESP_OK;
}

esp_err_t star_sensor_pca9685_get_frequency(const pca9685_handle_t* handle, uint16_t* freq_hz)
{
  if (handle == NULL || freq_hz == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t prescale;
  esp_err_t ret = pca9685_read_reg(handle->manager,
                                   handle->bus_name,
                                   handle->i2c_addr,
                                   PCA9685_REG_PRESCALE,
                                   &prescale);
  if (ret != ESP_OK) {
    return ret;
  }

  return star_sensor_pca9685_prescale_to_freq(prescale, freq_hz);
}

/* --- PWM Control Functions --- */

esp_err_t star_sensor_pca9685_set_pwm(const pca9685_handle_t* handle,
                                      uint8_t                 channel,
                                      uint16_t                on_time,
                                      uint16_t                off_time)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (channel >= PCA9685_NUM_CHANNELS) {
    ESP_LOGE(s_TAG, "Invalid channel: %d (max %d)", channel, PCA9685_NUM_CHANNELS - 1);
    return ESP_ERR_INVALID_ARG;
  }

  // Validate times are within 12-bit range
  if (on_time >= PCA9685_PWM_RESOLUTION || off_time >= PCA9685_PWM_RESOLUTION) {
    ESP_LOGE(s_TAG, "PWM values out of range (0-%d): on=%d, off=%d",
             PCA9685_PWM_RESOLUTION - 1, on_time, off_time);
    return ESP_ERR_INVALID_ARG;
  }

  // Calculate register address for this channel
  uint8_t reg_base = PCA9685_REG_LED0_ON_L + (4 * channel);

  // Write all 4 bytes at once using auto-increment
  uint8_t data[4] = {
    (uint8_t)(on_time & 0xFF),        // ON_L
    (uint8_t)((on_time >> 8) & 0x0F), // ON_H
    (uint8_t)(off_time & 0xFF),       // OFF_L
    (uint8_t)((off_time >> 8) & 0x0F) // OFF_H
  };

  esp_err_t ret = star_bus_i2c_write(handle->manager, handle->bus_name, data, 4, reg_base, NULL);

  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG,
             "Failed to write PWM for channel %d: %s",
             channel,
             esp_err_to_name(ret));
  } else {
    ESP_LOGD(s_TAG,
             "Channel %d: on=%d, off=%d (duty=%.1f%%)",
             channel,
             on_time,
             off_time,
             ((float)(off_time - on_time) / PCA9685_PWM_RESOLUTION) * 100.0f);
  }

  return ret;
}

esp_err_t star_sensor_pca9685_set_duty_cycle(const pca9685_handle_t* handle,
                                              uint8_t                 channel,
                                              float                   duty_percent)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Clamp duty cycle to 0-100%
  if (duty_percent < 0.0f) {
    duty_percent = 0.0f;
  } else if (duty_percent > 100.0f) {
    duty_percent = 100.0f;
  }

  // Convert to 12-bit value
  uint16_t duty_value = (uint16_t)((duty_percent / 100.0f) * PCA9685_PWM_RESOLUTION);

  // Set on_time to 0 for no phase shift
  return star_sensor_pca9685_set_pwm(handle, channel, 0, duty_value);
}

esp_err_t star_sensor_pca9685_set_duty_with_phase(const pca9685_handle_t* handle,
                                                   uint8_t                 channel,
                                                   float                   duty_percent,
                                                   float                   phase_percent)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Clamp values to 0-100%
  if (duty_percent < 0.0f) duty_percent = 0.0f;
  if (duty_percent > 100.0f) duty_percent = 100.0f;
  if (phase_percent < 0.0f) phase_percent = 0.0f;
  if (phase_percent > 100.0f) phase_percent = 100.0f;

  // Convert to 12-bit values
  uint16_t on_time = (uint16_t)((phase_percent / 100.0f) * PCA9685_PWM_RESOLUTION);
  uint16_t duty_value = (uint16_t)((duty_percent / 100.0f) * PCA9685_PWM_RESOLUTION);

  // Calculate off_time with wraparound handling
  uint16_t off_time = (on_time + duty_value) % PCA9685_PWM_RESOLUTION;

  return star_sensor_pca9685_set_pwm(handle, channel, on_time, off_time);
}

esp_err_t star_sensor_pca9685_get_pwm(const pca9685_handle_t* handle,
                                      uint8_t                 channel,
                                      uint16_t*               on_time,
                                      uint16_t*               off_time)
{
  if (handle == NULL || on_time == NULL || off_time == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_ARG;
  }

  if (channel >= PCA9685_NUM_CHANNELS) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg_base = PCA9685_REG_LED0_ON_L + (4 * channel);
  uint8_t data[4];

  // Read all 4 bytes
  esp_err_t ret = star_bus_i2c_read(handle->manager, handle->bus_name, data, 4, reg_base, NULL);
  if (ret != ESP_OK) {
    return ret;
  }

  *on_time = data[0] | ((uint16_t)(data[1] & 0x0F) << 8);
  *off_time = data[2] | ((uint16_t)(data[3] & 0x0F) << 8);

  return ESP_OK;
}

esp_err_t star_sensor_pca9685_set_channel_on(const pca9685_handle_t* handle, uint8_t channel)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (channel >= PCA9685_NUM_CHANNELS) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg_base = PCA9685_REG_LED0_ON_L + (4 * channel);

  // Set full ON bit in ON_H register
  uint8_t data[4] = {0x00, PCA9685_LED_FULL_ON, 0x00, 0x00};

  return star_bus_i2c_write(handle->manager, handle->bus_name, data, 4, reg_base, NULL);
}

esp_err_t star_sensor_pca9685_set_channel_off(const pca9685_handle_t* handle, uint8_t channel)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  if (channel >= PCA9685_NUM_CHANNELS) {
    return ESP_ERR_INVALID_ARG;
  }

  uint8_t reg_base = PCA9685_REG_LED0_ON_L + (4 * channel);

  // Set full OFF bit in OFF_H register
  uint8_t data[4] = {0x00, 0x00, 0x00, PCA9685_LED_FULL_OFF};

  return star_bus_i2c_write(handle->manager, handle->bus_name, data, 4, reg_base, NULL);
}

esp_err_t star_sensor_pca9685_set_all_duty_cycle(const pca9685_handle_t* handle,
                                                  float                   duty_percent)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  // Clamp duty cycle
  if (duty_percent < 0.0f) duty_percent = 0.0f;
  if (duty_percent > 100.0f) duty_percent = 100.0f;

  uint16_t duty_value = (uint16_t)((duty_percent / 100.0f) * PCA9685_PWM_RESOLUTION);

  // Write to ALL_LED registers
  uint8_t data[4] = {
    0x00, // ALL_LED_ON_L
    0x00, // ALL_LED_ON_H
    (uint8_t)(duty_value & 0xFF),       // ALL_LED_OFF_L
    (uint8_t)((duty_value >> 8) & 0x0F) // ALL_LED_OFF_H
  };

  return star_bus_i2c_write(handle->manager, handle->bus_name, data, 4, PCA9685_REG_ALL_LED_ON_L, NULL);
}

/* --- Helper Functions --- */

esp_err_t star_sensor_pca9685_sleep(pca9685_handle_t* handle, bool sleep)
{
  if (handle == NULL || !handle->initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  return pca9685_set_mode1_bits(handle, PCA9685_MODE1_SLEEP, sleep);
}
