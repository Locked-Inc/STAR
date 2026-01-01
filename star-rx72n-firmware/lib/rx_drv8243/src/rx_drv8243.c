/* lib/rx_drv8243/src/rx_drv8243.c */

/**
 * @file rx_drv8243.c
 * @brief DRV8243 H-bridge motor driver integration implementation for RX72N
 * @details
 * Implements integration layer for Texas Instruments DRV8243 H-bridge motor driver.
 * Combines rx_motor for PWM control with bus manager for current sensing (ADC) and
 * fault detection (GPIO). Provides current limiting and protection features.
 *
 * Port of star_drv8243 from ESP32 to RX72N platform.
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_drv8243.h"

#include <string.h>

#include "rx72n_regs.h"
#include "rx_bus_adc.h"
#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "DRV8243";

/* =============================================================================
 * Constants
 * =============================================================================
 */

/**
 * @brief DRV8243 configuration constants
 */
typedef enum {
  k_drv8243_default_ki_propi = 525,   /**< 525 A/V IPROPI ratio (typical) */
  k_drv8243_max_pwm_freq_hz  = 25000, /**< 25 kHz max recommended PWM */
} drv8243_constants_t;

/**
 * @brief Speed reduction factor when current limit is exceeded
 */
static const float s_current_limit_reduction_factor = 0.9f;

/**
 * @brief Conversion factor from millivolts to volts
 */
static const float s_mv_to_v_divisor = 1000.0f;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

static rx_err_t            internal_drv8243_check_current_limit(rx_drv8243_handle_t* handle);
static rx_err_t            internal_drv8243_configure_fault_pin(rx_drv8243_handle_t* handle);
static volatile rx_port_regs_t* internal_get_port_base(uint8_t port);

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_drv8243_init(rx_drv8243_handle_t* handle, const rx_drv8243_config_t* config)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");
  RX_CHECK_NULL_PTR(config->bus_manager, s_tag, "bus_manager pointer is NULL");
  RX_CHECK_NULL_PTR(config->gpio_bus_name, s_tag, "gpio_bus_name pointer is NULL");
  RX_CHECK_NULL_PTR(config->adc_bus_name, s_tag, "adc_bus_name pointer is NULL");

  if (handle->initialized) {
    rx_log_warn(s_tag, "DRV8243 already initialized");
    return k_rx_err_invalid_state;
  }

  /* Validate PWM frequency */
  if (config->pwm_freq_hz > k_drv8243_max_pwm_freq_hz) {
    rx_log_error(s_tag, "PWM frequency exceeds 25 kHz limit");
    return k_rx_err_invalid_arg;
  }

  /* Zero out handle */
  memset(handle, 0, sizeof(rx_drv8243_handle_t));

  /* Store configuration */
  handle->bus_manager      = config->bus_manager;
  handle->gpio_bus_name    = config->gpio_bus_name;
  handle->adc_bus_name     = config->adc_bus_name;
  handle->pin_ipropi       = config->pin_ipropi;
  handle->port_nfault      = config->port_nfault;
  handle->pin_nfault       = config->pin_nfault;
  handle->current_limit_ma = config->current_limit_ma;
  handle->ki_propi         = config->ki_propi > 0 ? config->ki_propi : k_drv8243_default_ki_propi;

  /* Initialize motor control (MTU for H-bridge) */
  rx_motor_config_t motor_config = {
    .channel      = config->mtu_channel,
    .output_a     = config->output_ph,
    .output_b     = config->output_en,
    .pwm_freq_hz  = config->pwm_freq_hz,
    .dead_time_ns = config->dead_time_ns,
    .invert_pwm   = false,
  };

  rx_err_t err = rx_motor_init(&handle->motor, &motor_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize motor controller");
    return err;
  }

  /* Configure nFAULT pin as input with pull-up (active low) */
  err = internal_drv8243_configure_fault_pin(handle);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to configure fault pin");
    rx_motor_deinit(&handle->motor);
    return err;
  }

  handle->current_speed = 0.0f;
  handle->fault_active  = false;
  handle->initialized   = true;

  rx_log_info(s_tag, "DRV8243 initialized");

  return k_rx_ok;
}

rx_err_t rx_drv8243_deinit(rx_drv8243_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_warn(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  /* Stop motor */
  rx_drv8243_stop(handle, false);

  /* Deinitialize motor controller */
  rx_motor_deinit(&handle->motor);

  /* Clear handle */
  memset(handle, 0, sizeof(rx_drv8243_handle_t));

  rx_log_info(s_tag, "DRV8243 deinitialized");

  return k_rx_ok;
}

rx_err_t rx_drv8243_set_speed(rx_drv8243_handle_t* handle, float speed)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  /* Check for fault condition */
  bool     fault_active;
  rx_err_t err = rx_drv8243_get_fault_status(handle, &fault_active);
  if (err == k_rx_ok && fault_active) {
    rx_log_warn(s_tag, "Cannot set speed: fault condition active");
    return k_rx_err_invalid_state;
  }

  /* Check current limit if enabled */
  if (handle->current_limit_ma > 0) {
    err = internal_drv8243_check_current_limit(handle);
    if (err != k_rx_ok) {
      rx_log_warn(s_tag, "Current limit exceeded, reducing speed");
      speed *= s_current_limit_reduction_factor;
    }
  }

  /* Set motor duty cycle */
  err = rx_motor_set_duty(&handle->motor, speed);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set motor duty");
    return err;
  }

  handle->current_speed = speed;

  rx_log_debug(s_tag, "Debug");

  return k_rx_ok;
}

rx_err_t rx_drv8243_stop(rx_drv8243_handle_t* handle, bool brake)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  rx_err_t err = rx_motor_stop(&handle->motor, brake);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to stop motor");
    return err;
  }

  handle->current_speed = 0.0f;

  rx_log_debug(s_tag, "Debug");

  return k_rx_ok;
}

rx_err_t rx_drv8243_read_current(rx_drv8243_handle_t* handle, float* out_current)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(out_current, s_tag, "out_current pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  /*
   * Read ADC voltage from IPROPI pin.
   * The voltage is returned in millivolts after calibration.
   */
  uint32_t voltage_mv;
  rx_err_t err = rx_bus_adc_read_voltage_mv(handle->bus_manager, handle->adc_bus_name, &voltage_mv);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read IPROPI ADC");
    return err;
  }

  /*
   * Convert IPROPI voltage to motor current using the Ki_PROPI ratio.
   *
   * Formula:
   * - I_motor (mA) = V (mV) * Ki_PROPI / 1000
   *
   * Example:
   * - voltage_mv = 1000 mV
   * - ki_propi = 525 A/V
   * - Result: 1000 * 525 / 1000 = 525 mA
   */
  *out_current = (float)(voltage_mv * handle->ki_propi) / s_mv_to_v_divisor;

  rx_log_debug(s_tag, "Debug");

  return k_rx_ok;
}

rx_err_t rx_drv8243_get_fault_status(rx_drv8243_handle_t* handle, bool* out_fault)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(out_fault, s_tag, "out_fault pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  /* Read nFAULT pin (active low) using PORT register */
  volatile rx_port_regs_t* port = internal_get_port_base(handle->port_nfault);
  if (port == NULL) {
    rx_log_error(s_tag, "Error occurred");
    return k_rx_err_invalid_arg;
  }

  uint8_t level = (port->PIDR >> handle->pin_nfault) & 0x01;

  /* Fault is active when pin is LOW */
  *out_fault           = (level == 0);
  handle->fault_active = *out_fault;

  if (*out_fault) {
    rx_log_warn(s_tag, "DRV8243 fault detected on nFAULT pin");
  }

  return k_rx_ok;
}

rx_err_t rx_drv8243_get_speed(const rx_drv8243_handle_t* handle, float* out_speed)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(out_speed, s_tag, "out_speed pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  *out_speed = handle->current_speed;

  return k_rx_ok;
}

rx_err_t rx_drv8243_set_current_limit(rx_drv8243_handle_t* handle, uint16_t limit_ma)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "DRV8243 not initialized");
    return k_rx_err_invalid_state;
  }

  handle->current_limit_ma = limit_ma;

  rx_log_info(s_tag, "Info");

  return k_rx_ok;
}

/* =============================================================================
 * Internal Helper Functions Implementation
 * =============================================================================
 */

/**
 * @brief Check if current limit is exceeded
 *
 * Reads the motor current via IPROPI ADC and compares against the configured
 * current limit. Used for software current limiting protection.
 *
 * @param[in] handle Pointer to DRV8243 handle
 *
 * @return k_rx_ok if within limit
 * @return k_rx_err_invalid_state if current exceeds limit
 */
static rx_err_t internal_drv8243_check_current_limit(rx_drv8243_handle_t* handle)
{
  float    current_ma;
  rx_err_t err = rx_drv8243_read_current(handle, &current_ma);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to read current for limit check");
    return err;
  }

  if (current_ma > (float)handle->current_limit_ma) {
    rx_log_warn(s_tag, "Current limit exceeded");
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

/**
 * @brief Configure nFAULT pin as input with pull-up
 *
 * Configures the specified GPIO pin to monitor the DRV8243 nFAULT signal.
 * The pin is set as input with internal pull-up enabled (fault signal is active low).
 *
 * @param[in] handle Pointer to DRV8243 handle
 *
 * @return k_rx_ok on success
 * @return k_rx_err_invalid_arg if invalid port number
 */
static rx_err_t internal_drv8243_configure_fault_pin(rx_drv8243_handle_t* handle)
{
  volatile rx_port_regs_t* port = internal_get_port_base(handle->port_nfault);
  if (port == NULL) {
    rx_log_error(s_tag, "Error occurred");
    return k_rx_err_invalid_arg;
  }

  /* Configure as input */
  port->PDR &= ~(1 << handle->pin_nfault);

  /* Enable pull-up (active low fault signal) */
  port->PCR |= (1 << handle->pin_nfault);

  return k_rx_ok;
}

/**
 * @brief Get PORT register base address
 *
 * Returns the base address of the PORT register structure for the specified
 * port number. Supports decimal ports (0-9) and port J (10).
 *
 * @param[in] port Port number (0-9 for PORT0-PORT9, 10 for PORTJ)
 *
 * @return Pointer to PORT register structure
 * @return NULL if port number is invalid
 */
static volatile rx_port_regs_t* internal_get_port_base(uint8_t port)
{
  switch (port) {
    case 0:
      return &PORT0;
    case 1:
      return &PORT1;
    case 2:
      return &PORT2;
    case 3:
      return &PORT3;
    case 4:
      return &PORT4;
    case 5:
      return &PORT5;
    case 6:
      return &PORT6;
    case 7:
      return &PORT7;
    case 8:
      return &PORT8;
    case 9:
      return &PORT9;
    case 10:
      return &PORTJ;
    default:
      return NULL;
  }
}
