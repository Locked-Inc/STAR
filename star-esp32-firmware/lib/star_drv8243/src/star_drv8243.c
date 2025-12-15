/* lib/star_drv8243/src/star_drv8243.c */

#include "star_drv8243.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

#include <string.h>

#include "star_bus_adc.h"

/* --- Constants --- */

static const char* s_TAG = "STAR_DRV8243";

/* Default configuration values */
#define DRV8243_DEFAULT_TIMER_RES_HZ 10000000  /* 10 MHz */
#define DRV8243_DEFAULT_KI_PROPI     525       /* 525 A/V typical */
#define DRV8243_MAX_PWM_FREQ_HZ      25000     /* 25 kHz max */

/* --- Private Function Prototypes --- */

static esp_err_t internal_drv8243_check_current_limit(star_drv8243_handle_t* handle);
static esp_err_t internal_drv8243_configure_fault_pin(star_drv8243_handle_t* handle);

/* --- Public Functions --- */

esp_err_t star_drv8243_init(star_drv8243_handle_t*       handle,
                             const star_drv8243_config_t* config)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, s_TAG, "Config is NULL");
  ESP_RETURN_ON_FALSE(config->bus_manager, ESP_ERR_INVALID_ARG, s_TAG, "Bus manager is NULL");
  ESP_RETURN_ON_FALSE(config->gpio_bus_name, ESP_ERR_INVALID_ARG, s_TAG, "GPIO bus name is NULL");
  ESP_RETURN_ON_FALSE(config->adc_bus_name, ESP_ERR_INVALID_ARG, s_TAG, "ADC bus name is NULL");
  ESP_RETURN_ON_FALSE(!handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Already initialized");

  /* Validate PWM frequency */
  ESP_RETURN_ON_FALSE(config->pwm_freq_hz <= DRV8243_MAX_PWM_FREQ_HZ,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "PWM frequency exceeds 25 kHz limit");

  /* Zero out handle */
  memset(handle, 0, sizeof(star_drv8243_handle_t));

  /* Store configuration */
  handle->bus_manager    = config->bus_manager;
  handle->gpio_bus_name  = config->gpio_bus_name;
  handle->adc_bus_name   = config->adc_bus_name;
  handle->pin_ipropi     = config->pin_ipropi;
  handle->pin_nfault     = config->pin_nfault;
  handle->current_limit_ma = config->current_limit_ma;
  handle->ki_propi       = config->ki_propi > 0 ? config->ki_propi : DRV8243_DEFAULT_KI_PROPI;

  esp_err_t ret = ESP_OK;

  /* Initialize motor control (MCPWM for H-bridge) */
  star_motor_config_t motor_config = {
    .group_id            = config->group_id,
    .pin_pwm_a           = config->pin_pwm_ph,
    .pin_pwm_b           = config->pin_pwm_en,
    .pwm_freq_hz         = config->pwm_freq_hz,
    .timer_resolution_hz = config->timer_resolution_hz > 0 ? config->timer_resolution_hz
                                                            : DRV8243_DEFAULT_TIMER_RES_HZ,
    .dead_time_ns        = config->dead_time_ns,
    .invert_pwm          = false,
    .fault_pin           = -1,  /* We handle fault separately via GPIO bus */
  };

  ret = star_motor_init(&handle->motor, &motor_config);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to initialize motor controller");

  /* Configure nFAULT pin as input with pull-up (active low) */
  ret = internal_drv8243_configure_fault_pin(handle);
  if (ret != ESP_OK) {
    ESP_LOGE(s_TAG, "Failed to configure fault pin: %s", esp_err_to_name(ret));
    star_motor_deinit(&handle->motor);
    return ret;
  }

  handle->current_speed = 0.0f;
  handle->fault_active  = false;
  handle->initialized   = true;

  ESP_LOGI(s_TAG,
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
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Stop motor */
  star_drv8243_stop(handle, false);

  /* Deinitialize motor controller */
  star_motor_deinit(&handle->motor);

  /* Clear handle */
  memset(handle, 0, sizeof(star_drv8243_handle_t));

  ESP_LOGI(s_TAG, "DRV8243 deinitialized");
  return ESP_OK;
}

esp_err_t star_drv8243_set_speed(star_drv8243_handle_t* handle, float speed)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Check for fault condition */
  bool fault_active;
  esp_err_t ret = star_drv8243_get_fault_status(handle, &fault_active);
  if (ret == ESP_OK && fault_active) {
    ESP_LOGW(s_TAG, "Cannot set speed: fault condition active");
    return ESP_ERR_INVALID_STATE;
  }

  /* Check current limit if enabled */
  if (handle->current_limit_ma > 0) {
    ret = internal_drv8243_check_current_limit(handle);
    if (ret != ESP_OK) {
      ESP_LOGW(s_TAG, "Current limit exceeded, reducing speed");
      /* Reduce speed by 10% when current limit hit */
      speed *= 0.9f;
    }
  }

  /* Set motor duty cycle */
  ret = star_motor_set_duty(&handle->motor, speed);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to set motor duty");

  handle->current_speed = speed;

  ESP_LOGD(s_TAG, "Motor speed set: %.1f%%", speed);
  return ESP_OK;
}

esp_err_t star_drv8243_stop(star_drv8243_handle_t* handle, bool brake)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  esp_err_t ret = star_motor_stop(&handle->motor, brake);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to stop motor");

  handle->current_speed = 0.0f;

  ESP_LOGD(s_TAG, "Motor stopped (%s)", brake ? "brake" : "coast");
  return ESP_OK;
}

esp_err_t star_drv8243_read_current(star_drv8243_handle_t* handle, float* out_current)
{
  ESP_RETURN_ON_FALSE(handle && out_current,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Handle or out_current is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Read ADC voltage from IPROPI pin */
  uint32_t voltage_mv;
  esp_err_t ret = star_bus_adc_read_voltage(handle->bus_manager,
                                              handle->adc_bus_name,
                                              handle->pin_ipropi,
                                              &voltage_mv);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read IPROPI ADC");

  /* Convert voltage to current using ki_propi ratio
   * Current (mA) = Voltage (mV) * Ki_PROPI (A/V) / 1000
   * Example: 1000mV * 525 A/V / 1000 = 525mA
   */
  *out_current = (float)(voltage_mv * handle->ki_propi) / 1000.0f;

  ESP_LOGD(s_TAG, "Motor current: %.1f mA (IPROPI=%lu mV)", *out_current, voltage_mv);
  return ESP_OK;
}

esp_err_t star_drv8243_get_fault_status(star_drv8243_handle_t* handle, bool* out_fault)
{
  ESP_RETURN_ON_FALSE(handle && out_fault,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Handle or out_fault is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  /* Read nFAULT pin (active low) directly using ESP-IDF GPIO */
  int level = gpio_get_level(handle->pin_nfault);

  /* Fault is active when pin is LOW */
  *out_fault       = (level == 0);
  handle->fault_active = *out_fault;

  if (*out_fault) {
    ESP_LOGW(s_TAG, "DRV8243 fault detected on nFAULT pin");
  }

  return ESP_OK;
}

esp_err_t star_drv8243_get_speed(const star_drv8243_handle_t* handle, float* out_speed)
{
  ESP_RETURN_ON_FALSE(handle && out_speed,
                      ESP_ERR_INVALID_ARG,
                      s_TAG,
                      "Handle or out_speed is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  *out_speed = handle->current_speed;
  return ESP_OK;
}

esp_err_t star_drv8243_set_current_limit(star_drv8243_handle_t* handle, uint16_t limit_ma)
{
  ESP_RETURN_ON_FALSE(handle, ESP_ERR_INVALID_ARG, s_TAG, "Handle is NULL");
  ESP_RETURN_ON_FALSE(handle->initialized, ESP_ERR_INVALID_STATE, s_TAG, "Not initialized");

  handle->current_limit_ma = limit_ma;

  ESP_LOGI(s_TAG, "Current limit updated: %u mA", limit_ma);
  return ESP_OK;
}

/* --- Private Functions --- */

static esp_err_t internal_drv8243_check_current_limit(star_drv8243_handle_t* handle)
{
  float current_ma;
  esp_err_t ret = star_drv8243_read_current(handle, &current_ma);
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to read current for limit check");

  if (current_ma > (float)handle->current_limit_ma) {
    ESP_LOGW(s_TAG, "Current limit exceeded: %.1f mA > %u mA", current_ma, handle->current_limit_ma);
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
  ESP_RETURN_ON_ERROR(ret, s_TAG, "Failed to configure nFAULT pin");

  return ESP_OK;
}
