/* src/rx_motor.c */

/**
 * @file rx_motor.c
 * @brief Brushed DC motor control implementation for RX72N
 * @details
 * H-bridge motor control using MTU3a PWM peripheral.
 *
 * Motor Control Modes:
 * 1. Forward: output_a = PWM, output_b = LOW
 * 2. Reverse: output_a = LOW, output_b = PWM
 * 3. Brake: output_a = HIGH, output_b = HIGH (short circuit)
 * 4. Coast: output_a = LOW, output_b = LOW (high impedance)
 *
 * PWM Frequency:
 * - Typical: 20 kHz (above human hearing, motor-friendly)
 * - Range: 1 kHz - 50 kHz depending on motor inductance
 *
 * Dead-time:
 * - Prevents shoot-through in H-bridge
 * - Typical: 500ns - 2us depending on FET switching speed
 *
 * @date 2025-12-21
 * @copyright Copyright (c) 2025 STAR Project
 */

#include "rx_motor.h"

#include "rx_check.h"
#include "rx_log.h"

#include <math.h>

static const char* s_tag = "MOTOR";

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
  if (duty > 100.0f) {
    return 100.0f;
  }
  if (duty < -100.0f) {
    return -100.0f;
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
    RX_LOG_WARN(s_tag, "Motor already initialized");
    return RX_ERR_INVALID_STATE;
  }

  RX_LOG_INFO(s_tag, "Initializing motor");

  /* Initialize MTU PWM */
  rx_mtu_config_t mtu_config = {
    .frequency_hz         = config->pwm_freq_hz,
    .deadtime_ns          = config->dead_time_ns,
    .enable_complementary = false, /* We control direction manually */
    .invert_polarity      = config->invert_pwm,
  };

  rx_err_t err = rx_mtu_init_pwm(config->channel, &mtu_config);
  if (err != RX_OK) {
    RX_LOG_ERROR(s_tag, "Failed to initialize MTU PWM");
    return err;
  }

  /* Initialize both outputs to 0% duty (stopped) */
  rx_mtu_set_duty(config->channel, config->output_a, 0.0f);
  rx_mtu_set_duty(config->channel, config->output_b, 0.0f);

  /* Save configuration */
  handle->channel      = config->channel;
  handle->output_a     = config->output_a;
  handle->output_b     = config->output_b;
  handle->pwm_freq_hz  = config->pwm_freq_hz;
  handle->current_duty = 0.0f;
  handle->invert_pwm   = config->invert_pwm;
  handle->initialized  = true;

  RX_LOG_INFO(s_tag, "Motor initialized successfully");

  return RX_OK;
}

rx_err_t rx_motor_deinit(rx_motor_handle_t* handle)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_WARN(s_tag, "Motor not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Stop motor before deinit */
  rx_motor_stop(handle, false);

  /* Deinitialize MTU (note: this affects the entire channel) */
  rx_mtu_deinit(handle->channel);

  handle->initialized = false;

  RX_LOG_INFO(s_tag, "Motor deinitialized");

  return RX_OK;
}

rx_err_t rx_motor_set_duty(rx_motor_handle_t* handle, float duty)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "Motor not initialized");
    return RX_ERR_INVALID_STATE;
  }

  /* Clamp duty cycle to valid range */
  duty = internal_clamp_duty(duty);

  /* Apply inversion if configured */
  if (handle->invert_pwm) {
    duty = -duty;
  }

  /* Set PWM outputs based on direction */
  if (duty >= 0.0f) {
    /* Forward: output_a = PWM, output_b = 0 */
    rx_mtu_set_duty(handle->channel, handle->output_a, duty);
    rx_mtu_set_duty(handle->channel, handle->output_b, 0.0f);
  } else {
    /* Reverse: output_a = 0, output_b = PWM */
    rx_mtu_set_duty(handle->channel, handle->output_a, 0.0f);
    rx_mtu_set_duty(handle->channel, handle->output_b, fabsf(duty));
  }

  handle->current_duty = duty;

  return RX_OK;
}

rx_err_t rx_motor_stop(rx_motor_handle_t* handle, bool brake)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "Motor not initialized");
    return RX_ERR_INVALID_STATE;
  }

  if (brake) {
    /* Brake mode: both outputs HIGH (short circuit brake) */
    rx_mtu_set_duty(handle->channel, handle->output_a, 100.0f);
    rx_mtu_set_duty(handle->channel, handle->output_b, 100.0f);
  } else {
    /* Coast mode: both outputs LOW (high impedance) */
    rx_mtu_set_duty(handle->channel, handle->output_a, 0.0f);
    rx_mtu_set_duty(handle->channel, handle->output_b, 0.0f);
  }

  handle->current_duty = 0.0f;

  return RX_OK;
}

rx_err_t rx_motor_get_duty(const rx_motor_handle_t* handle, float* out_duty)
{
  RX_CHECK_NULL_PTR(handle, s_tag, "handle pointer is NULL");
  RX_CHECK_NULL_PTR(out_duty, s_tag, "out_duty pointer is NULL");

  if (!handle->initialized) {
    RX_LOG_ERROR(s_tag, "Motor not initialized");
    return RX_ERR_INVALID_STATE;
  }

  *out_duty = handle->current_duty;

  return RX_OK;
}
