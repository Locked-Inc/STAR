/**
 * @file star_drv8243.c
 * @brief DRV8243 H-bridge motor driver integration implementation
 * @details
 * Implements integration layer for Texas Instruments DRV8243 H-bridge motor driver.
 * Combines star_motor for PWM control with bus manager for current sensing (ADC) and
 * fault detection (GPIO). Provides current limiting and protection features.
 *
 * @date 2025-12-19
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "star_drv8243.h"

#include "driver/gpio.h"

#include <inttypes.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "star_bus_adc.h"

/* --- Constants --- */

static const char* s_tag = "STAR_DRV8243";

/**
 * @brief DRV8243 configuration constants
 *
 * These values are derived from the DRV8243 datasheet (SLVSGD7A).
 * - Timer resolution: 10 MHz provides good duty cycle resolution at 20-25 kHz PWM
 * - Ki_PROPI: Current sense ratio from IPROPI pin (typical 525 A/V at 25C)
 * - Max PWM frequency: DRV8243 supports up to 200 kHz, but 25 kHz is recommended
 *   for low audible noise and efficient switching
 */
enum {
  k_drv8243_default_timer_res_hz = 10000000, /**< 10 MHz timer resolution */
  k_drv8243_default_ki_propi     = 525,      /**< 525 A/V IPROPI ratio (typical) */
  k_drv8243_max_pwm_freq_hz      = 25000,    /**< 25 kHz max recommended PWM */
};

/**
 * @brief Speed reduction factor when current limit is exceeded
 *
 * When motor current exceeds the configured limit, speed is reduced by this
 * factor (0.9 = 10% reduction) as a soft protection measure. This provides
 * gradual current limiting without abrupt motor stops that could cause
 * mechanical stress or loss of control.
 */
static const float s_current_limit_reduction_factor = 0.9f;

/**
 * @brief Conversion factor from millivolts to volts
 *
 * Used in current calculation to convert ADC reading (mV) to proper units
 * for the Ki_PROPI formula: I_motor (mA) = V (mV) * Ki_PROPI (A/V) / 1000
 */
static const float s_mv_to_v_divisor = 1000.0f;

/* --- Private Function Prototypes --- */

static esp_err_t internal_drv8243_check_current_limit(star_drv8243_handle_t* handle);
static esp_err_t internal_drv8243_configure_fault_pin(star_drv8243_handle_t* handle);

/* --- Public Functions --- */

esp_err_t star_drv8243_init(star_drv8243_handle_t* handle, const star_drv8243_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_tag, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->bus_manager, ESP_ERR_INVALID_ARG, s_tag, "Bus manager is NULL");
  ESP_RETURN_ON_FALSE(config->gpio_bus_name, ESP_ERR_INVALID_ARG, s_tag, "GPIO bus name is NULL");
  ESP_RETURN_ON_FALSE(config->adc_bus_name, ESP_ERR_INVALID_ARG, s_tag, "ADC bus name is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Already initialized");

  /* Validate PWM frequency */
  ESP_RETURN_ON_FALSE(config->pwm_freq_hz <= k_drv8243_max_pwm_freq_hz,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "PWM frequency exceeds 25 kHz limit");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_drv8243_handle_t));

  /* Store configuration */
  handle->bus_manager      = config->bus_manager;
  handle->gpio_bus_name    = config->gpio_bus_name;
  handle->adc_bus_name     = config->adc_bus_name;
  handle->pin_ipropi       = config->pin_ipropi;
  handle->pin_nfault       = config->pin_nfault;
  handle->current_limit_ma = config->current_limit_ma;
  handle->ki_propi         = config->ki_propi > 0 ? config->ki_propi : k_drv8243_default_ki_propi;

  esp_err_t ret = ESP_OK;

  /* Initialize motor control (MCPWM for H-bridge) */
  star_motor_config_t motor_config = {
    .group_id            = config->group_id,
    .pin_pwm_a           = config->pin_pwm_ph,
    .pin_pwm_b           = config->pin_pwm_en,
    .pwm_freq_hz         = config->pwm_freq_hz,
    .timer_resolution_hz = config->timer_resolution_hz > 0 ? config->timer_resolution_hz
                                                           : k_drv8243_default_timer_res_hz,
    .dead_time_ns        = config->dead_time_ns,
    .invert_pwm          = false,
    .fault_pin           = -1, /* We handle fault separately via GPIO bus */
  };

  ret = star_motor_init(&handle->motor, &motor_config);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to initialize motor controller");

  /* Configure nFAULT pin as input with pull-up (active low) */
  ret = internal_drv8243_configure_fault_pin(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_tag, "Failed to configure fault pin: %s", esp_err_to_name(ret));
    star_motor_deinit(&handle->motor);
    return ret;
  }

  handle->current_speed = 0.0f;
  handle->fault_active  = false;
  handle->initialized   = true;

  ESP_LOGI(s_tag,
           "DRV8243 initialized: PH=%d, EN=%d, IPROPI=%d, nFAULT=%d, freq=%luHz, limit=%umA",
           config->pin_pwm_ph,
           config->pin_pwm_en,
           config->pin_ipropi,
           config->pin_nfault,
           config->pwm_freq_hz,
           config->current_limit_ma);

  return ESP_OK;
}

esp_err_t star_drv8243_deinit(star_drv8243_handle_t* handle)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Stop motor */
  star_drv8243_stop(handle, false);

  /* Deinitialize motor controller */
  star_motor_deinit(&handle->motor);

  /* Clear handle */
  memset(handle, 0, sizeof(star_drv8243_handle_t));

  ESP_LOGI(s_tag, "DRV8243 deinitialized");
  return ESP_OK;
}

esp_err_t star_drv8243_set_speed(star_drv8243_handle_t* handle, float speed)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Check for fault condition */
  bool      fault_active;
  esp_err_t ret = star_drv8243_get_fault_status(handle, &fault_active);
  if (ret == ESP_OK && fault_active) {
    ESP_LOGW(s_tag, "Cannot set speed: fault condition active");
    return ESP_ERR_INVALID_STATE;
  }

  /* Check current limit if enabled */
  if (handle->current_limit_ma > 0) {
    ret = internal_drv8243_check_current_limit(handle);
    if (ret != ESP_OK) {
      ESP_LOGW(s_tag, "Current limit exceeded, reducing speed");
      speed *= s_current_limit_reduction_factor;
    }
  }

  /* Set motor duty cycle */
  ret = star_motor_set_duty(&handle->motor, speed);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to set motor duty");

  handle->current_speed = speed;

  ESP_LOGD(s_tag, "Motor speed set: %.1f%%", speed);
  return ESP_OK;
}

esp_err_t star_drv8243_stop(star_drv8243_handle_t* handle, bool brake)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  esp_err_t ret = star_motor_stop(&handle->motor, brake);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to stop motor");

  handle->current_speed = 0.0f;

  ESP_LOGD(s_tag, "Motor stopped (%s)", brake ? "brake" : "coast");
  return ESP_OK;
}

esp_err_t star_drv8243_read_current(star_drv8243_handle_t* handle, float* out_current)
{
  ESP_RETURN_ON_FALSE(handle && out_current,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or out_current is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /*
   * Read ADC voltage from IPROPI pin.
   * The ADC bus was configured with the IPROPI channel during init.
   * The voltage is returned in millivolts after ESP-IDF calibration.
   */
  int32_t   voltage_mv;
  esp_err_t ret = star_bus_adc_read_voltage(handle->bus_manager, handle->adc_bus_name, &voltage_mv);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read IPROPI ADC");

  /*
   * Convert IPROPI voltage to motor current using the Ki_PROPI ratio.
   *
   * Formula derivation:
   * - IPROPI outputs voltage V = I_motor / Ki_PROPI (where Ki_PROPI is in A/V)
   * - Therefore: I_motor (A) = V (V) * Ki_PROPI (A/V)
   * - Converting units: I_motor (mA) = V (mV) * Ki_PROPI / 1000
   *
   * Example with typical values:
   * - voltage_mv = 1000 mV (1V from IPROPI)
   * - ki_propi = 525 A/V (datasheet typical at 25C)
   * - Result: 1000 * 525 / 1000 = 525 mA motor current
   *
   * Note: Ki_PROPI varies with temperature; for high-precision applications,
   * consider temperature compensation using the DS18B20 sensor.
   */
  *out_current = (float)(voltage_mv * handle->ki_propi) / s_mv_to_v_divisor;

  ESP_LOGD(s_tag, "Motor current: %.1f mA (IPROPI=%" PRId32 " mV)", *out_current, voltage_mv);
  return ESP_OK;
}

esp_err_t star_drv8243_get_fault_status(star_drv8243_handle_t* handle, bool* out_fault)
{
  ESP_RETURN_ON_FALSE(handle && out_fault,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or out_fault is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  /* Read nFAULT pin (active low) directly using ESP-IDF GPIO */
  int level = gpio_get_level(handle->pin_nfault);

  /* Fault is active when pin is LOW */
  *out_fault           = (level == 0);
  handle->fault_active = *out_fault;

  if (*out_fault) {
    ESP_LOGW(s_tag, "DRV8243 fault detected on nFAULT pin");
  }

  return ESP_OK;
}

esp_err_t star_drv8243_get_speed(const star_drv8243_handle_t* handle, float* out_speed)
{
  ESP_RETURN_ON_FALSE(handle && out_speed,
                      ESP_ERR_INVALID_ARG,
                      s_tag,
                      "Handle or out_speed is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  *out_speed = handle->current_speed;
  return ESP_OK;
}

esp_err_t star_drv8243_set_current_limit(star_drv8243_handle_t* handle, uint16_t limit_ma)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_tag, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_tag, "Not initialized");

  handle->current_limit_ma = limit_ma;

  ESP_LOGI(s_tag, "Current limit updated: %u mA", limit_ma);
  return ESP_OK;
}

/* --- Private Functions --- */

static esp_err_t internal_drv8243_check_current_limit(star_drv8243_handle_t* handle)
{
  float     current_ma;
  esp_err_t ret = star_drv8243_read_current(handle, &current_ma);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to read current for limit check");

  if (current_ma > (float)handle->current_limit_ma) {
    ESP_LOGW(s_tag,
             "Current limit exceeded: %.1f mA > %u mA",
             current_ma,
             handle->current_limit_ma);
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

static esp_err_t internal_drv8243_configure_fault_pin(star_drv8243_handle_t* handle)
{
  /* Configure nFAULT pin as input with pull-up (active low) using ESP-IDF GPIO */
  gpio_config_t fault_cfg = {
    .pin_bit_mask = (1ULL << handle->pin_nfault),
    .mode         = GPIO_MODE_INPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE,
  };

  esp_err_t ret = gpio_config(&fault_cfg);
  ESP_RETURN_ON_ERROR(ret, s_tag, "Failed to configure nFAULT pin");

  return ESP_OK;
}
