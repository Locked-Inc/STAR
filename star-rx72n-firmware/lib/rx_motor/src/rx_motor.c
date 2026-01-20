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
typedef enum : int16_t {
  k_motor_duty_min  = -100, /**< Minimum duty cycle (full reverse) */
  k_motor_duty_max  = 100,  /**< Maximum duty cycle (full forward) */
  k_motor_duty_zero = 0,    /**< Zero duty cycle (stopped) */
  k_motor_ph_high   = 100,  /**< PH signal for forward direction */
  k_motor_ph_low    = 0,    /**< PH signal for reverse direction */
} motor_constants_t;

/** @brief Motor configuration validation limits (NASA Rule 5 compliance) */
typedef enum : uint32_t {
  k_motor_min_pwm_freq  = 1000,  /**< Minimum PWM frequency (1 kHz) */
  k_motor_max_pwm_freq  = 50000, /**< Maximum PWM frequency (50 kHz) */
  k_motor_min_dead_time = 100,   /**< Minimum dead-time (100 ns) */
  k_motor_max_dead_time = 10000, /**< Maximum dead-time (10 us) */
} motor_validation_limits_t;

/* Zero duty for stopped outputs (floats can't be enums). */
static const float s_duty_zero = 0.0f;

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
static float internal_clamp_duty(const float duty)
{
  /* Safety check for invalid float values (NASA Rule 5 compliance) */
  if (isnan(duty) || isinf(duty)) {
    return (float)k_motor_duty_zero; /* Safe default: stopped */
  }

  if (duty > (float)k_motor_duty_max) {
    return (float)k_motor_duty_max;
  }
  if (duty < (float)k_motor_duty_min) {
    return (float)k_motor_duty_min;
  }
  return duty;
}

/**
 * @brief Initialize GPTW and set initial outputs
 *
 * @param[in] channel GPTW channel
 * @param[in] output_a PH output
 * @param[in] output_b EN output
 * @param[in] gptw_config GPTW configuration
 *
 * @return k_rx_ok on success, error code on failure
 */
static rx_err_t internal_init_gptw_outputs(const rx_gptw_channel_t channel,
                                           const rx_gptw_output_t  output_a,
                                           const rx_gptw_output_t  output_b,
                                           const rx_gptw_config_t* gptw_config)
{
  rx_err_t err = k_rx_err_invalid_state;

  RX_CHECK_NULL_PTR(gptw_config, s_tag, "gptw_config pointer is NULL");

  if ((output_a != k_gptw_output_a && output_a != k_gptw_output_b) ||
      (output_b != k_gptw_output_a && output_b != k_gptw_output_b)) {
    rx_log_error(s_tag, "Invalid GPTW output selection");
    return k_rx_err_invalid_arg;
  }

  err = rx_gptw_init_pwm(channel, gptw_config);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to initialize GPTW PWM");
    return err;
  }

  err = rx_gptw_set_duty(channel, output_a, s_duty_zero);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_a initial duty");
    (void)rx_gptw_deinit(channel);
    return err;
  }

  err = rx_gptw_set_duty(channel, output_b, s_duty_zero);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_b initial duty");
    (void)rx_gptw_deinit(channel);
    return err;
  }

  return k_rx_ok;
}

/* =============================================================================
 * Public API Implementation
 * =============================================================================
 */

rx_err_t rx_motor_init(rx_motor_handle_t* handle, const rx_motor_config_t* config)
{
  rx_err_t         err;
  rx_gptw_config_t gptw_config;

  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(config, s_tag, "config pointer is NULL");

  if (handle->initialized) {
    rx_log_warn(s_tag, "Motor already initialized");
    return k_rx_err_invalid_state;
  }

  /* Pre-condition: Validate PWM frequency (NASA Rule 5 compliance) */
  if (config->pwm_freq_hz < k_motor_min_pwm_freq || config->pwm_freq_hz > k_motor_max_pwm_freq) {
    rx_log_error(s_tag, "PWM frequency out of range (1kHz-50kHz)");
    return k_rx_err_invalid_arg;
  }

  /* Pre-condition: Validate dead-time (NASA Rule 5 compliance) */
  if (config->dead_time_ns < k_motor_min_dead_time ||
      config->dead_time_ns > k_motor_max_dead_time) {
    rx_log_error(s_tag, "Dead-time out of range (100ns-10us)");
    return k_rx_err_invalid_arg;
  }

  rx_log_info(s_tag, "Initializing motor");

  /* Initialize GPTW PWM */
  gptw_config = (rx_gptw_config_t){
    .frequency_hz         = config->pwm_freq_hz,
    .deadtime_ns          = config->dead_time_ns,
    .enable_complementary = false, /* We control direction manually */
    .invert_polarity      = config->invert_pwm,
  };

  err =
    internal_init_gptw_outputs(config->channel, config->output_a, config->output_b, &gptw_config);
  if (err != k_rx_ok) {
    return err;
  }

  /* Save configuration */
  handle->channel      = config->channel;
  handle->output_a     = config->output_a;
  handle->output_b     = config->output_b;
  handle->pwm_freq_hz  = config->pwm_freq_hz;
  handle->current_duty = s_duty_zero;
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
  rx_err_t err;

  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_warn(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  /* Stop motor before deinit */
  err = rx_motor_stop(handle, false);
  if (err != k_rx_ok) {
    rx_log_warn(s_tag, "Failed to stop motor during deinit");
  }

  /* Deinitialize GPTW (note: this affects the entire channel) */
  err = rx_gptw_deinit(handle->channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to deinitialize GPTW");
    return err;
  }

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

  /* Pre-condition: Validate duty value is reasonable (NASA Rule 5 compliance) */
  if (isnan(duty) || isinf(duty)) {
    rx_log_error(s_tag, "Invalid duty value (NaN or Inf)");
    return k_rx_err_invalid_arg;
  }

  /* Clamp duty cycle to valid range (after validation) */
  duty = internal_clamp_duty(duty);

  /* Apply inversion if configured */
  if (handle->invert_pwm) {
    duty = -duty;
  }

  /* Set PWM outputs based on direction (PH/EN mode)
   * PH (output_a) = direction (HIGH=forward, LOW=reverse)
   * EN (output_b) = speed (PWM duty cycle)
   *
   * Extract speed magnitude (absolute value) - direction is encoded in PH signal.
   * Duty sign determines PH (+ = forward/HIGH, - = reverse/LOW).
   * EN always receives positive PWM duty proportional to speed.
   */
  const float speed_pwm = fabsf(duty);
  rx_err_t    err;

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

rx_err_t rx_motor_stop(rx_motor_handle_t* handle, const bool brake)
{
  rx_err_t err = k_rx_err_invalid_state;

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
  err = rx_gptw_set_duty(handle->channel, handle->output_a, s_duty_zero);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_a during stop");
    return err;
  }

  err = rx_gptw_set_duty(handle->channel, handle->output_b, s_duty_zero);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "Failed to set output_b during stop");
    return err;
  }

  handle->current_duty = s_duty_zero;

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
  rx_err_t result = k_rx_ok;
  rx_err_t err;

  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    rx_log_error(s_tag, "Motor not initialized");
    return k_rx_err_invalid_state;
  }

  /* Immediately set duty to 0% */
  err = rx_gptw_set_duty(handle->channel, handle->output_a, s_duty_zero);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "E-STOP: Failed to clear output_a duty");
    if (result == k_rx_ok) {
      result = err;
    }
  }
  err = rx_gptw_set_duty(handle->channel, handle->output_b, s_duty_zero);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "E-STOP: Failed to clear output_b duty");
    if (result == k_rx_ok) {
      result = err;
    }
  }

  /* Disable GPTW outputs at hardware level */
  err = rx_gptw_enable_output(handle->channel, handle->output_a, false);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "E-STOP: Failed to disable output_a");
    if (result == k_rx_ok) {
      result = err;
    }
  }
  err = rx_gptw_enable_output(handle->channel, handle->output_b, false);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "E-STOP: Failed to disable output_b");
    if (result == k_rx_ok) {
      result = err;
    }
  }

  /* Stop timer to prevent glitches */
  err = rx_gptw_stop(handle->channel);
  if (err != k_rx_ok) {
    rx_log_error(s_tag, "E-STOP: Failed to stop GPTW timer");
    if (result == k_rx_ok) {
      result = err;
    }
  }

  /* Mark as no longer initialized - requires re-init to use */
  handle->initialized  = false;
  handle->current_duty = s_duty_zero;

  rx_log_warn(s_tag, "EMERGENCY STOP - motor disabled");

  return result;
}
