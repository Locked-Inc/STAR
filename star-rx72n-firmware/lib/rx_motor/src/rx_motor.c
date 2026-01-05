/* lib/rx_motor/src/rx_motor.c */

/**
 * @file rx_motor.c
 * @brief Brushed DC motor control implementation for RX72N
 * @details
 * H-bridge motor control using GPTW PWM peripheral.
 *
 * Motor Control Modes (PH/EN for DRV8243):
 * - PH (output_a) = direction (HIGH=forward, LOW=reverse)
 * - EN (output_b) = speed (PWM duty cycle)
 *
 * States:
 * 1. Forward: PH = HIGH (100%), EN = PWM (speed)
 * 2. Reverse: PH = LOW (0%), EN = PWM (speed)
 * 3. Coast: EN = LOW (0%) - motor in high impedance
 * 4. Brake: NOT SUPPORTED in PH/EN mode (falls back to coast)
 *
 * PWM Frequency:
 * - Typical: 20 kHz (above human hearing, motor-friendly)
 * - Range: 1 kHz - 50 kHz depending on motor inductance
 *
 * Dead-time:
 * - Prevents shoot-through in H-bridge
 * - Typical: 500ns - 2us depending on FET switching speed
 *
 * @date 2026-01-04
 * @copyright Copyright (c) 2026 STAR Project
 */

#include "rx_motor.h"

#include <math.h>

#include "rx_check.h"
#include "rx_log.h"

static const char* s_tag = "MOTOR";

/** @brief Motor control constants for DRV8243 PH/EN mode */
typedef enum {
  k_motor_duty_min  = -100, /**< Minimum duty cycle (full reverse) */
  k_motor_duty_max  = 100,  /**< Maximum duty cycle (full forward) */
  k_motor_duty_zero = 0,    /**< Zero duty cycle (stopped) */
  k_motor_ph_high   = 100,  /**< PH signal for forward direction */
  k_motor_ph_low    = 0,    /**< PH signal for reverse direction */
} motor_constants_t;

/* =============================================================================
 * Internal Helper Functions
 * =============================================================================
 */

/**
 * @brief Clamp duty cycle to valid range
 *
 * @param[in] duty Duty cycle percentage
 *
 * @return Clamped duty cycle (-100 to +100)
 */
static float internal_clamp_duty(float duty)
{
  /* Safety check for invalid float values (NASA Rule 5 compliance) */
  if (isnan(duty) || isinf(duty)) {
    return (float)k_motor_duty_zero;  /* Safe default: stopped */
  }

  if (duty > (float)k_motor_duty_max) {
    return (float)k_motor_duty_max;
  }
  if (duty < (float)k_motor_duty_min) {
    return (float)k_motor_duty_min;
  }
  return duty;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_motor_init(rx_motor_handle_t* handle, const rx_motor_config_t* config)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  if (handle->initialized) {
    rx_log_warn(s_tag, "Motor already initialized");
    return k_rx_err_invalid_state;
  }

  rx_log_info(s_tag, "Initializing motor");

  /* Initialize GPTW PWM */
  rx_gptw_config_t gptw_config = {
    .frequency_hz         = config->pwm_freq_hz,
    .deadtime_ns          = config->dead_time_ns,
    .enable_complementary = false, /* We control direction manually */
    .invert_polarity      = config->invert_pwm,
  };

  rx_err_t err = rx_gptw_init_pwm(config->channel, &gptw_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize GPTW PWM");
    return err;
  }

  /* Initialize both outputs to 0% duty (stopped) - NASA Rule 7 compliance */
  err = rx_gptw_set_duty(config->channel, config->output_a, 0.0f);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_a initial duty");
    rx_gptw_deinit(config->channel);
    return err;
  }

  err = rx_gptw_set_duty(config->channel, config->output_b, 0.0f);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_b initial duty");
    rx_gptw_deinit(config->channel);
    return err;
  }

  /* Save configuration */
  handle->channel      = config->channel;
  handle->output_a     = config->output_a;
  handle->output_b     = config->output_b;
  handle->pwm_freq_hz  = config->pwm_freq_hz;
  handle->current_duty = 0.0f;
  handle->invert_pwm   = config->invert_pwm;
  handle->initialized  = true;

  /* Post-condition: Verify handle was properly initialized (NASA Rule 5 compliance) */
  if (!handle->initialized || handle->pwm_freq_hz != config->pwm_freq_hz) {
    rx_log_error(s_tag, "Post-condition failed: handle not properly initialized");
    return k_rx_err_invalid_state;
  }

  rx_log_info(s_tag, "Motor initialized successfully");

  return k_rx_ok;
}

rx_err_t rx_motor_deinit(rx_motor_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_warn(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  /* Stop motor before deinit */
  rx_motor_stop(handle, false);

  /* Deinitialize GPTW (note: this affects the entire channel) */
  rx_gptw_deinit(handle->channel);

  handle->initialized = false;

  rx_log_info(s_tag, "Motor deinitialized");

  return k_rx_ok;
}

rx_err_t rx_motor_set_duty(rx_motor_handle_t* handle, float duty)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  /* Clamp duty cycle to valid range */
  duty = internal_clamp_duty(duty);

  /* Apply inversion if configured */
  if (handle->invert_pwm) {
    duty = -duty;
  }

  /* Set PWM outputs based on direction (PH/EN mode)
   * PH (output_a) = direction (HIGH=forward, LOW=reverse)
   * EN (output_b) = speed (PWM duty cycle)
   */
  float    speed_pwm = fabsf(duty);
  rx_err_t err;

  if (duty >= (float)k_motor_duty_zero) {
    /* Forward: PH = HIGH, EN = PWM - NASA Rule 7 compliance */
    err = rx_gptw_set_duty(handle->channel, handle->output_a, (float)k_motor_ph_high);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to set PH output (forward)");
      return err;
    }

    err = rx_gptw_set_duty(handle->channel, handle->output_b, speed_pwm);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to set EN output (forward)");
      return err;
    }
  } else {
    /* Reverse: PH = LOW, EN = PWM - NASA Rule 7 compliance */
    err = rx_gptw_set_duty(handle->channel, handle->output_a, (float)k_motor_ph_low);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to set PH output (reverse)");
      return err;
    }

    err = rx_gptw_set_duty(handle->channel, handle->output_b, speed_pwm);
    if (err != k_rx_ok) {
      rx_log_error(s_tag, "Failed to set EN output (reverse)");
      return err;
    }
  }

  handle->current_duty = duty;

  /* Post-condition: Verify duty was updated correctly (NASA Rule 5 compliance) */
  if (handle->current_duty != duty) {
    rx_log_error(s_tag, "Post-condition failed: duty not updated correctly");
    return k_rx_err_invalid_state;
  }

  return k_rx_ok;
}

rx_err_t rx_motor_stop(rx_motor_handle_t* handle, bool brake)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  if (brake) {
    /* Brake mode not supported in PH/EN mode - coast instead */
    rx_log_warn(s_tag, "Brake not supported in PH/EN mode, coasting");
  }

  /* Coast mode: set both outputs to LOW for high impedance - NASA Rule 7 compliance */
  rx_err_t err = rx_gptw_set_duty(handle->channel, handle->output_a, 0.0f);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_a during stop");
    return err;
  }

  err = rx_gptw_set_duty(handle->channel, handle->output_b, 0.0f);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_b during stop");
    return err;
  }

  handle->current_duty = 0.0f;

  return k_rx_ok;
}

rx_err_t rx_motor_get_duty(const rx_motor_handle_t* handle, float* out_duty)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(out_duty, s_tag, "out_duty pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  *out_duty = handle->current_duty;

  return k_rx_ok;
}

rx_err_t rx_motor_emergency_stop(rx_motor_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  /* Immediately set duty to 0% */
  rx_gptw_set_duty(handle->channel, handle->output_a, 0.0f);
  rx_gptw_set_duty(handle->channel, handle->output_b, 0.0f);

  /* Disable GPTW outputs at hardware level */
  rx_gptw_enable_output(handle->channel, handle->output_a, false);
  rx_gptw_enable_output(handle->channel, handle->output_b, false);

  /* Stop timer to prevent glitches */
  rx_gptw_stop(handle->channel);

  /* Mark as no longer initialized - requires re-init to use */
  handle->initialized  = false;
  handle->current_duty = 0.0f;

  rx_log_warn(s_tag, "EMERGENCY STOP - motor disabled");

  return k_rx_ok;
}
